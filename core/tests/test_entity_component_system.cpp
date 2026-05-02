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
#include <cassert>
#include <iostream>

#include "ecs/ecs.h"

struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

int main() {
    using namespace ecs;

    ComponentRegistry::instance().register_component<Position>("Position");
    ComponentRegistry::instance().register_component<Velocity>("Velocity");

    EntityManager manager;

    EntityID entity = manager.create_entity({detail::component_type_id<Position>()});
    assert(entity != INVALID_ENTITY);
    assert(manager.is_valid(entity));

    Position* position = manager.get_component<Position>(entity);
    assert(position != nullptr);
    position->x = 100.0f;
    position->y = 200.0f;
    position->z = 300.0f;

    bool added_velocity = manager.add_component<Velocity>(entity, Velocity{1.0f, 2.0f, 3.0f});
    assert(added_velocity);

    int query_count = 0;
    manager.query_components<Position, Velocity>([&](EntityID queried_entity, Position& pos, Velocity& vel) {
        assert(queried_entity == entity);
        assert(pos.x == 100.0f);
        assert(pos.y == 200.0f);
        assert(pos.z == 300.0f);
        assert(vel.x == 1.0f);
        assert(vel.y == 2.0f);
        assert(vel.z == 3.0f);
        query_count++;
    });
    assert(query_count == 1);

    SerializationSystem serializer(&manager);
    SerializedEntity serialized;
    bool serialized_ok = serializer.serialize_entity(entity, serialized);
    assert(serialized_ok);
    assert(serialized.entity_id == entity);
    assert(serialized.components.size() == 2);

    EntityID deserialized_entity = INVALID_ENTITY;
    bool restored = serializer.deserialize_entity(serialized, deserialized_entity);
    assert(restored);
    assert(deserialized_entity != INVALID_ENTITY);
    assert(manager.is_valid(deserialized_entity));

    Position* restored_position = manager.get_component<Position>(deserialized_entity);
    Velocity* restored_velocity = manager.get_component<Velocity>(deserialized_entity);
    assert(restored_position != nullptr);
    assert(restored_velocity != nullptr);
    assert(restored_position->x == 100.0f);
    assert(restored_position->y == 200.0f);
    assert(restored_position->z == 300.0f);
    assert(restored_velocity->x == 1.0f);
    assert(restored_velocity->y == 2.0f);
    assert(restored_velocity->z == 3.0f);

    std::cout << "ECS core self-test passed.\n";
    return 0;
}
