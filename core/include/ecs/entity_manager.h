/*
 Copyright (C) 2026 Jocer S. <patcex@proton.me>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 SPDX-License-Identifier: AGPL-3.0 OR Commercial
*/
#pragma once

#include "component_pool.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <unordered_map>

namespace ecs {

class EntityManager;

class SystemBase {
public:
    virtual ~SystemBase() = default;
    virtual void update(float delta_time) = 0;
    virtual void on_entity_added(EntityID /*entity*/) {}
    virtual void on_entity_removed(EntityID /*entity*/) {}

    void set_entity_manager(EntityManager* manager) {
        entity_manager_ = manager;
    }

protected:
    EntityManager* entity_manager_ = nullptr;
    float current_delta_time_ = 0.0f;
};

struct BitsetHash {
    size_t operator()(ComponentMask const& mask) const noexcept {
        return static_cast<size_t>(mask.to_ullong());
    }
};

class Archetype {
public:
    Archetype(ComponentMask component_mask, std::vector<ComponentTypeID> component_types,
              std::vector<std::unique_ptr<IComponentStorage>> component_storages)
        : component_mask_(component_mask),
          component_types_(std::move(component_types)),
          component_storages_(std::move(component_storages)),
          entity_count_(0) {
        for (size_t i = 0; i < component_types_.size(); ++i) {
            component_lookup_[component_types_[i]] = static_cast<uint32_t>(i);
        }
    }

    bool has_component(ComponentTypeID type_id) const noexcept {
        return component_mask_.test(type_id);
    }

    IComponentStorage* get_storage(ComponentTypeID type_id) {
        auto it = component_lookup_.find(type_id);
        if (it == component_lookup_.end()) {
            return nullptr;
        }
        return component_storages_[it->second].get();
    }

    IComponentStorage* get_storage(ComponentTypeID type_id) const {
        auto it = component_lookup_.find(type_id);
        if (it == component_lookup_.end()) {
            return nullptr;
        }
        return component_storages_[it->second].get();
    }

    template<typename ComponentType>
    ComponentPool<ComponentType>* get_storage() const {
        return dynamic_cast<ComponentPool<ComponentType>*>(get_storage(detail::component_type_id<ComponentType>()));
    }

    void add_entity(EntityID entity) {
        for (auto& storage : component_storages_) {
            storage->ensure_capacity(entity_index(entity));
            storage->add_default(entity);
        }

        entities_.push_back(entity);
        entity_count_ = entities_.size();
    }

    void remove_entity(EntityID entity) {
        if (component_storages_.empty()) {
            erase_entity_from_list(entity);
            return;
        }

        uint32_t removal_index = component_storages_.front()->index_of(entity);
        if (removal_index == UINT32_MAX) {
            return;
        }

        for (auto& storage : component_storages_) {
            storage->remove(entity);
        }

        if (removal_index < entities_.size() - 1) {
            entities_[removal_index] = entities_.back();
        }
        entities_.pop_back();
        entity_count_ = entities_.size();
    }

    template<typename ComponentType>
    void copy_entity_data(EntityID entity, Archetype* destination) const {
        auto* source = get_storage<ComponentType>();
        if (!source) {
            return;
        }

        auto* target = destination->get_storage<ComponentType>();
        if (!target) {
            return;
        }

        source->copy_entity_data(entity, target);
    }

    const std::vector<EntityID>& entities() const noexcept {
        return entities_;
    }

    const std::vector<std::unique_ptr<IComponentStorage>>& storages() const noexcept {
        return component_storages_;
    }

    size_t entity_count() const noexcept {
        return entity_count_;
    }

    const ComponentMask& component_mask() const noexcept {
        return component_mask_;
    }

    const std::vector<ComponentTypeID>& component_types() const noexcept {
        return component_types_;
    }

private:
    void erase_entity_from_list(EntityID entity) {
        auto it = std::find(entities_.begin(), entities_.end(), entity);
        if (it == entities_.end()) {
            return;
        }
        std::iter_swap(it, entities_.end() - 1);
        entities_.pop_back();
        entity_count_ = entities_.size();
    }

    ComponentMask component_mask_;
    std::vector<ComponentTypeID> component_types_;
    std::vector<std::unique_ptr<IComponentStorage>> component_storages_;
    std::unordered_map<ComponentTypeID, uint32_t> component_lookup_;
    std::vector<EntityID> entities_;
    size_t entity_count_ = 0;
};

class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() = default;

    EntityID create_entity(const std::vector<ComponentTypeID>& initial_components = {}) {
        std::scoped_lock lock(entity_mutex_);
        uint32_t index = allocate_index();
        uint32_t generation = static_cast<uint32_t>(next_generation_.fetch_add(1, std::memory_order_relaxed));
        EntityID entity_id = make_entity_id(index, generation);

        if (index >= entity_pool_.size()) {
            entity_pool_.resize(index + 1);
        }

        entity_pool_[index].set_generation(generation);
        entity_pool_[index].set_active(true);
        entity_pool_[index].set_archetype_index(0);

        ComponentMask mask;
        std::vector<ComponentTypeID> component_types = normalize_component_list(initial_components, mask);
        Archetype* archetype = find_or_create_archetype(mask, component_types);
        archetype->add_entity(entity_id);
        entity_pool_[index].set_archetype_index(get_archetype_index(archetype));

        notify_entity_added(entity_id);
        return entity_id;
    }

    bool destroy_entity(EntityID entity) {
        std::scoped_lock lock(entity_mutex_);
        uint32_t index = entity_index(entity);
        if (!is_valid(entity)) {
            return false;
        }

        EntityInfo& info = entity_pool_[index];
        Archetype* archetype = archetype_by_index(info.archetype_index());
        if (archetype) {
            archetype->remove_entity(entity);
        }

        info.set_active(false);
        free_indices_.push(index);
        notify_entity_removed(entity);
        return true;
    }

    template<typename ComponentType>
    bool add_component(EntityID entity, const ComponentType& component) {
        std::scoped_lock lock(entity_mutex_);
        if (!is_valid(entity)) {
            return false;
        }

        uint32_t index = entity_index(entity);
        EntityInfo& info = entity_pool_[index];
        Archetype* current_archetype = archetype_by_index(info.archetype_index());
        ComponentMask new_mask = current_archetype ? current_archetype->component_mask() : ComponentMask();
        ComponentTypeID type_id = detail::component_type_id<ComponentType>();
        if (!ComponentRegistry::instance().is_registered(type_id)) {
            detail::register_component_type<ComponentType>(typeid(ComponentType).name());
        }

        if (new_mask.test(type_id)) {
            if (current_archetype) {
                auto* storage = current_archetype->get_storage<ComponentType>();
                if (storage) {
                    storage->add(entity, component);
                    return true;
                }
            }
            return false;
        }

        new_mask.set(type_id);
        std::vector<ComponentTypeID> component_types = component_types_from_mask(new_mask);
        Archetype* target_archetype = find_or_create_archetype(new_mask, component_types);
        target_archetype->add_entity(entity);
        copy_shared_components(current_archetype, target_archetype, entity);
        auto* target_storage = target_archetype->get_storage<ComponentType>();
        if (target_storage) {
            target_storage->add(entity, component);
        }

        if (current_archetype) {
            current_archetype->remove_entity(entity);
        }

        info.set_archetype_index(get_archetype_index(target_archetype));
        return true;
    }

    template<typename ComponentType>
    ComponentType* get_component(EntityID entity) {
        if (!is_valid(entity)) {
            return nullptr;
        }

        uint32_t index = entity_index(entity);
        EntityInfo const& info = entity_pool_[index];
        Archetype* archetype = archetype_by_index(info.archetype_index());
        if (!archetype) {
            return nullptr;
        }

        return archetype->get_storage<ComponentType>()->get(entity);
    }

    template<typename... ComponentTypes, typename Callback>
    void query_components(Callback&& callback) {
        ComponentMask mask = make_component_mask<ComponentTypes...>();
        std::shared_lock lock(archetype_mutex_);
        for (auto& archetype_uptr : archetypes_) {
            Archetype* archetype = archetype_uptr.get();
            if ((archetype->component_mask() & mask) == mask) {
                query_archetype<ComponentTypes...>(archetype, std::forward<Callback>(callback));
            }
        }
    }

    void add_system(std::unique_ptr<SystemBase> system) {
        if (!system) {
            return;
        }

        system->set_entity_manager(this);
        systems_.push_back(std::move(system));
    }

    void update_systems(float delta_time) {
        for (auto& system : systems_) {
            system->update(delta_time);
        }
    }

    Archetype* archetype_by_index(uint32_t index) {
        if (index >= archetypes_.size()) {
            return nullptr;
        }
        return archetypes_[index].get();
    }

    const Archetype* archetype_by_index(uint32_t index) const {
        if (index >= archetypes_.size()) {
            return nullptr;
        }
        return archetypes_[index].get();
    }

    const Archetype* archetype_for(EntityID entity) const {
        if (!is_valid(entity)) {
            return nullptr;
        }
        uint32_t index = entity_index(entity);
        const EntityInfo& info = entity_pool_[index];
        return archetype_by_index(info.archetype_index());
    }

    bool is_valid(EntityID entity) const noexcept {
        if (entity == INVALID_ENTITY) {
            return false;
        }

        uint32_t index = entity_index(entity);
        if (index >= entity_pool_.size()) {
            return false;
        }

        const EntityInfo& info = entity_pool_[index];
        return info.is_active() && info.generation() == entity_generation(entity);
    }

private:
    template<typename... ComponentTypes>
    static ComponentMask make_component_mask() {
        ComponentMask mask;
        (mask.set(detail::component_type_id<ComponentTypes>()), ...);
        return mask;
    }

    std::vector<ComponentTypeID> component_types_from_mask(const ComponentMask& mask) const {
        std::vector<ComponentTypeID> result;
        for (ComponentTypeID type_id = 0; type_id < MAX_COMPONENT_TYPES; ++type_id) {
            if (mask.test(type_id)) {
                result.push_back(type_id);
            }
        }
        return result;
    }

    std::vector<ComponentTypeID> normalize_component_list(const std::vector<ComponentTypeID>& initial_components, ComponentMask& out_mask) const {
        std::vector<ComponentTypeID> types = initial_components;
        std::sort(types.begin(), types.end());
        types.erase(std::unique(types.begin(), types.end()), types.end());
        for (ComponentTypeID type_id : types) {
            if (type_id < MAX_COMPONENT_TYPES) {
                out_mask.set(type_id);
            }
        }
        return types;
    }

    Archetype* find_or_create_archetype(const ComponentMask& mask, const std::vector<ComponentTypeID>& component_types) {
        std::unique_lock lock(archetype_mutex_);
        auto it = archetype_lookup_.find(mask);
        if (it != archetype_lookup_.end()) {
            return archetypes_[it->second].get();
        }

        std::vector<std::unique_ptr<IComponentStorage>> storages;
        storages.reserve(component_types.size());
        for (ComponentTypeID type_id : component_types) {
            storages.push_back(ComponentRegistry::instance().create_storage(type_id));
        }

        size_t index = archetypes_.size();
        archetypes_.push_back(std::make_unique<Archetype>(mask, component_types, std::move(storages)));
        archetype_lookup_.emplace(mask, static_cast<uint32_t>(index));
        return archetypes_.back().get();
    }

    uint32_t get_archetype_index(Archetype* archetype) const noexcept {
        for (uint32_t idx = 0; idx < archetypes_.size(); ++idx) {
            if (archetypes_[idx].get() == archetype) {
                return idx;
            }
        }
        return EntityInfo::INVALID_ARCHETYPE;
    }

    void copy_shared_components(Archetype* source, Archetype* destination, EntityID entity) {
        if (!source || !destination) {
            return;
        }

        for (ComponentTypeID type_id : source->component_types()) {
            if (destination->has_component(type_id)) {
                IComponentStorage* source_storage = source->get_storage(type_id);
                IComponentStorage* target_storage = destination->get_storage(type_id);
                if (source_storage && target_storage) {
                    source_storage->copy_entity_data(entity, target_storage);
                }
            }
        }
    }

    template<typename... ComponentTypes, typename Callback>
    void query_archetype(Archetype* archetype, Callback&& callback) {
        auto const storages = std::make_tuple(archetype->template get_storage<ComponentTypes>()...);
        size_t count = archetype->entity_count();

        for (size_t index = 0; index < count; ++index) {
            EntityID entity = archetype->entities()[index];
            callback(entity, std::get<ComponentPool<ComponentTypes>*>(storages)->get_by_index(index)...);
        }
    }

    uint32_t allocate_index() {
        if (!free_indices_.empty()) {
            uint32_t index = free_indices_.front();
            free_indices_.pop();
            return index;
        }
        return static_cast<uint32_t>(entity_pool_.size());
    }

    void notify_entity_added(EntityID entity) {
        for (auto& system : systems_) {
            system->on_entity_added(entity);
        }
    }

    void notify_entity_removed(EntityID entity) {
        for (auto& system : systems_) {
            system->on_entity_removed(entity);
        }
    }

    std::vector<EntityInfo> entity_pool_;
    std::queue<uint32_t> free_indices_;
    std::atomic<uint64_t> next_generation_{1};
    std::mutex entity_mutex_;
    std::shared_mutex archetype_mutex_;
    std::vector<std::unique_ptr<Archetype>> archetypes_;
    std::unordered_map<ComponentMask, uint32_t, BitsetHash> archetype_lookup_;
    std::vector<std::unique_ptr<SystemBase>> systems_;
};

} // namespace ecs
