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

#include "entity_manager.h"

#include <vector>

namespace ecs {

class SerializationSystem {
public:
    explicit SerializationSystem(EntityManager* entity_manager) : entity_manager_(entity_manager) {}

    bool serialize_entity(EntityID entity, SerializedEntity& output) const {
        if (!entity_manager_ || !entity_manager_->is_valid(entity)) {
            return false;
        }

        const Archetype* archetype = entity_manager_->archetype_for(entity);
        if (!archetype) {
            return false;
        }

        output.entity_id = entity;
        output.generation = entity_generation(entity);
        output.is_active = true;
        output.components.clear();
        output.components.reserve(archetype->component_types().size());

        for (ComponentTypeID type_id : archetype->component_types()) {
            IComponentStorage* storage = archetype->get_storage(type_id);
            if (!storage) {
                continue;
            }

            SerializedComponent component;
            storage->serialize(entity, component);
            component.type_id = type_id;
            component.size = component.data.size();
            output.components.push_back(std::move(component));
        }

        return true;
    }

    bool deserialize_entity(const SerializedEntity& input, EntityID& output_entity) {
        if (!entity_manager_) {
            return false;
        }

        std::vector<ComponentTypeID> component_types;
        component_types.reserve(input.components.size());
        for (auto const& component : input.components) {
            component_types.push_back(component.type_id);
        }

        output_entity = entity_manager_->create_entity(component_types);
        const Archetype* const archetype = entity_manager_->archetype_for(output_entity);
        if (!archetype) {
            return false;
        }

        for (auto const& component : input.components) {
            IComponentStorage* storage = const_cast<Archetype*>(archetype)->get_storage(component.type_id);
            if (!storage) {
                return false;
            }
            if (!storage->deserialize(output_entity, component)) {
                return false;
            }
        }

        return true;
    }

private:
    EntityManager* entity_manager_ = nullptr;
};

} // namespace ecs
