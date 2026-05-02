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

#include <algorithm>
#include <array>
#include <cassert>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs {

template<typename ComponentType>
class ComponentPool;

using ComponentTypeID = uint32_t;
using EntityID = uint64_t;
constexpr EntityID INVALID_ENTITY = 0ULL;
constexpr size_t MAX_COMPONENT_TYPES = 64;
using ComponentMask = std::bitset<MAX_COMPONENT_TYPES>;

struct SerializedComponent {
    ComponentTypeID type_id = 0;
    std::vector<uint8_t> data;
    size_t size = 0;
};

struct SerializedEntity {
    EntityID entity_id = INVALID_ENTITY;
    uint32_t generation = 0;
    bool is_active = false;
    std::vector<SerializedComponent> components;
};

inline EntityID make_entity_id(uint32_t entity_index, uint32_t generation) noexcept {
    return (static_cast<EntityID>(generation) << 32) | entity_index;
}

inline uint32_t entity_index(EntityID entity) noexcept {
    return static_cast<uint32_t>(entity);
}

inline uint32_t entity_generation(EntityID entity) noexcept {
    return static_cast<uint32_t>(entity >> 32);
}

struct EntityInfo {
    uint64_t packed_ = 0;

    static constexpr uint64_t ACTIVE_BIT = 1ull << 63;
    static constexpr uint64_t ARCHETYPE_MASK = (1ull << 31) - 1;
    static constexpr uint64_t GENERATION_SHIFT = 32;
    static constexpr uint32_t INVALID_ARCHETYPE = UINT32_MAX;

    EntityInfo() noexcept {
        set_generation(0);
        set_archetype_index(INVALID_ARCHETYPE);
        set_active(false);
    }

    uint32_t generation() const noexcept {
        return static_cast<uint32_t>((packed_ >> GENERATION_SHIFT) & ARCHETYPE_MASK);
    }

    uint32_t archetype_index() const noexcept {
        return static_cast<uint32_t>(packed_ & ARCHETYPE_MASK);
    }

    bool is_active() const noexcept {
        return (packed_ & ACTIVE_BIT) != 0;
    }

    void set_generation(uint32_t generation) noexcept {
        packed_ = (packed_ & ~(ARCHETYPE_MASK << GENERATION_SHIFT)) | (static_cast<uint64_t>(generation) << GENERATION_SHIFT);
    }

    void set_archetype_index(uint32_t archetype_index) noexcept {
        packed_ = (packed_ & ~ARCHETYPE_MASK) | (static_cast<uint64_t>(archetype_index) & ARCHETYPE_MASK);
    }

    void set_active(bool active) noexcept {
        if (active) {
            packed_ |= ACTIVE_BIT;
        } else {
            packed_ &= ~ACTIVE_BIT;
        }
    }
};

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    virtual ComponentTypeID type_id() const noexcept = 0;
    virtual const char* type_name() const noexcept = 0;
    virtual bool contains(EntityID entity) const noexcept = 0;
    virtual void ensure_capacity(uint32_t entity_index) = 0;
    virtual void add_default(EntityID entity) = 0;
    virtual void remove(EntityID entity) = 0;
    virtual uint32_t index_of(EntityID entity) const noexcept = 0;
    virtual void copy_entity_data(EntityID entity, IComponentStorage* destination) const = 0;
    virtual void serialize(EntityID entity, SerializedComponent& output) const = 0;
    virtual bool deserialize(EntityID entity, const SerializedComponent& input) = 0;
    virtual size_t component_size() const noexcept = 0;
};

struct ComponentDescriptor {
    ComponentTypeID type_id = 0;
    std::string_view type_name;
    size_t size = 0;
    std::function<std::unique_ptr<IComponentStorage>()> factory;
};

class ComponentRegistry;

namespace detail {
    inline ComponentTypeID next_component_type_id() {
        static ComponentTypeID id = 0;
        return id++;
    }

    template<typename ComponentType>
    ComponentTypeID component_type_id() {
        static ComponentTypeID id = next_component_type_id();
        return id;
    }
} // namespace detail

class ComponentRegistry {
public:
    static ComponentRegistry& instance() {
        static ComponentRegistry registry;
        return registry;
    }

    template<typename ComponentType>
    ComponentTypeID register_component(std::string_view name = {}) {
        ComponentTypeID id = detail::component_type_id<ComponentType>();
        if (id >= MAX_COMPONENT_TYPES) {
            throw std::out_of_range("Component type id exceeds MAX_COMPONENT_TYPES");
        }

        if (!registered_.test(id)) {
            registered_.set(id);
            descriptors_[id] = ComponentDescriptor{
                id,
                name.empty() ? typeid(ComponentType).name() : name,
                sizeof(ComponentType),
                []() -> std::unique_ptr<IComponentStorage> {
                    return std::make_unique<ComponentPool<ComponentType>>();
                }
            };
        }

        return id;
    }

    const ComponentDescriptor& descriptor(ComponentTypeID id) const {
        if (id >= MAX_COMPONENT_TYPES || !registered_.test(id)) {
            throw std::out_of_range("Unknown component type id");
        }
        return descriptors_[id];
    }

    std::unique_ptr<IComponentStorage> create_storage(ComponentTypeID id) const {
        const ComponentDescriptor& descriptor_ref = descriptor(id);
        return descriptor_ref.factory();
    }

    bool is_registered(ComponentTypeID id) const noexcept {
        return id < MAX_COMPONENT_TYPES && registered_.test(id);
    }

private:
    ComponentRegistry() = default;
    std::array<ComponentDescriptor, MAX_COMPONENT_TYPES> descriptors_;
    ComponentMask registered_;
};

namespace detail {
    template<typename ComponentType>
    ComponentTypeID register_component_type(std::string_view name = {}) {
        return ComponentRegistry::instance().register_component<ComponentType>(name);
    }
} // namespace detail

template<typename ComponentType>
class ComponentPool final : public IComponentStorage {
public:
    static_assert(std::is_default_constructible_v<ComponentType>, "ComponentType must be default constructible");

    ComponentPool() noexcept : type_id_(detail::component_type_id<ComponentType>()), type_name_(typeid(ComponentType).name()) {
    }

    ComponentTypeID type_id() const noexcept override {
        return type_id_;
    }

    const char* type_name() const noexcept override {
        return type_name_; 
    }

    bool contains(EntityID entity) const noexcept override {
        uint32_t index = entity_index(entity);
        return index < entity_to_index_.size() && entity_to_index_[index] != INVALID_INDEX;
    }

    void ensure_capacity(uint32_t entity_index) override {
        if (entity_index >= entity_to_index_.size()) {
            entity_to_index_.resize(entity_index + 1, INVALID_INDEX);
        }
    }

    void add_default(EntityID entity) override {
        uint32_t index = entity_index(entity);
        ensure_capacity(index);
        if (entity_to_index_[index] != INVALID_INDEX) {
            return;
        }

        entity_to_index_[index] = static_cast<uint32_t>(components_.size());
        components_.emplace_back();
        index_to_entity_.push_back(entity);
        active_flags_.push_back(1);
        ++active_count_;
    }

    void add(EntityID entity, const ComponentType& component) {
        uint32_t index = entity_index(entity);
        ensure_capacity(index);
        uint32_t storage_index = entity_to_index_[index];
        if (storage_index == INVALID_INDEX) {
            storage_index = static_cast<uint32_t>(components_.size());
            components_.push_back(component);
            index_to_entity_.push_back(entity);
            entity_to_index_[index] = storage_index;
            active_flags_.push_back(1);
            ++active_count_;
            return;
        }

        components_[storage_index] = component;
        active_flags_[storage_index] = 1;
    }

    ComponentType* get(EntityID entity) noexcept {
        uint32_t index = entity_index(entity);
        if (index >= entity_to_index_.size()) {
            return nullptr;
        }

        uint32_t storage_index = entity_to_index_[index];
        if (storage_index == INVALID_INDEX) {
            return nullptr;
        }

        return &components_[storage_index];
    }

    const ComponentType* get(EntityID entity) const noexcept {
        uint32_t index = entity_index(entity);
        if (index >= entity_to_index_.size()) {
            return nullptr;
        }

        uint32_t storage_index = entity_to_index_[index];
        if (storage_index == INVALID_INDEX) {
            return nullptr;
        }

        return &components_[storage_index];
    }

    ComponentType& get_by_index(size_t storage_index) noexcept {
        return components_[storage_index];
    }

    const ComponentType& get_by_index(size_t storage_index) const noexcept {
        return components_[storage_index];
    }

    void remove(EntityID entity) override {
        uint32_t index = entity_index(entity);
        if (index >= entity_to_index_.size()) {
            return;
        }

        uint32_t storage_index = entity_to_index_[index];
        if (storage_index == INVALID_INDEX) {
            return;
        }

        uint32_t last_index = static_cast<uint32_t>(components_.size() - 1);
        EntityID moved_entity = index_to_entity_[last_index];

        if (storage_index != last_index) {
            components_[storage_index] = std::move(components_[last_index]);
            index_to_entity_[storage_index] = moved_entity;
            active_flags_[storage_index] = active_flags_[last_index];
            entity_to_index_[entity_index(moved_entity)] = storage_index;
        }

        components_.pop_back();
        index_to_entity_.pop_back();
        active_flags_.pop_back();
        entity_to_index_[index] = INVALID_INDEX;
        --active_count_;
    }

    uint32_t index_of(EntityID entity) const noexcept override {
        uint32_t index = entity_index(entity);
        return index < entity_to_index_.size() ? entity_to_index_[index] : INVALID_INDEX;
    }

    void copy_entity_data(EntityID entity, IComponentStorage* destination) const override {
        auto* typed_destination = dynamic_cast<ComponentPool<ComponentType>*>(destination);
        if (!typed_destination) {
            return;
        }

        const ComponentType* source = get(entity);
        if (source) {
            typed_destination->add(entity, *source);
        }
    }

    void serialize(EntityID entity, SerializedComponent& output) const override {
        const ComponentType* component = get(entity);
        if (!component) {
            output.type_id = type_id_;
            output.data.clear();
            output.size = 0;
            return;
        }

        static_assert(std::is_trivially_copyable_v<ComponentType>, "Component type must be trivially copyable for serialization");
        output.type_id = type_id_;
        output.size = sizeof(ComponentType);
        output.data.resize(output.size);
        std::memcpy(output.data.data(), component, output.size);
    }

    bool deserialize(EntityID entity, const SerializedComponent& input) override {
        if (input.type_id != type_id_ || input.size != sizeof(ComponentType)) {
            return false;
        }

        static_assert(std::is_trivially_copyable_v<ComponentType>, "Component type must be trivially copyable for serialization");
        if (input.data.size() != input.size) {
            return false;
        }

        ComponentType component;
        std::memcpy(&component, input.data.data(), input.size);
        add(entity, component);
        return true;
    }

    size_t component_size() const noexcept override {
        return sizeof(ComponentType);
    }

    const std::vector<ComponentType>& components() const noexcept {
        return components_;
    }

    const std::vector<EntityID>& entities() const noexcept {
        return index_to_entity_;
    }

private:
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
    ComponentTypeID type_id_;
    const char* type_name_ = nullptr;
    std::vector<ComponentType> components_;
    std::vector<uint32_t> entity_to_index_;
    std::vector<EntityID> index_to_entity_;
    std::vector<uint8_t> active_flags_;
    size_t active_count_ = 0;
};

} // namespace ecs
