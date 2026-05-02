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
#include "physics_core/physics_container.h"
#include <cassert>
#include <iostream>

using namespace physics_core;

void test_physics_container_simd() {
    std::cout << "Testing PhysicsContainer SIMD sync and batch processing..." << std::endl;

    PhysicsContainer container(16);
    double origin[3] = {0.0, 0.0, 0.0};

    for (EntityID i = 1; i <= 8; ++i) {
        PhysicsBody body;
        body.entity_id = i;
        body.position = Vec3(static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3));
        body.velocity = Vec3(1.0f, 1.0f, 1.0f);
        body.mass = 1.0f;
        body.inv_mass = 1.0f;
        body.material_type = 0;
        body.body_type = 1;
        container.register_entity(body);
    }

    assert(container.get_entity_count() == 8);
    assert(container.get_active_count() == 8);

    container.sync_all_to_local_simd(origin);
    for (size_t i = 0; i < 8; ++i) {
        const PhysicsBody* body = container.get_all_bodies().data() + i;
        assert(body->position.x == static_cast<float>(i + 1));
        assert(body->position.y == static_cast<float>((i + 1) * 2));
        assert(body->position.z == static_cast<float>((i + 1) * 3));
    }

    container.process_active_batch_simd(0, container.get_active_count(), 0.5f);

    for (size_t i = 0; i < 8; ++i) {
        const PhysicsBody* body = container.get_all_bodies().data() + i;
        assert(body->position.x == static_cast<float>(i + 1) + 0.5f);
        assert(body->position.y == static_cast<float>((i + 1) * 2) + 0.5f);
        assert(body->position.z == static_cast<float>((i + 1) * 3) + 0.5f);
    }

    container.reorganize_memory_layout();
    assert(container.validate_consistency());
    assert(container.get_entity_count() == 8);

    assert(container.remove_entity(3).success);
    assert(container.get_entity_count() == 7);
    assert(!container.contains(3));

    std::cout << " ✓ PhysicsContainer SIMD sync and layout test passed" << std::endl;
}

int main() {
    test_physics_container_simd();
    std::cout << "All PhysicsContainer tests passed." << std::endl;
    return 0;
}
