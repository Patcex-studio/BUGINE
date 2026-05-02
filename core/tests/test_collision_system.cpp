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
#include <iostream>
#include <cassert>
#include "physics_core/collision_system.h"

using namespace physics_core;

int main() {
    std::cout << "Testing CollisionSystem..." << std::endl;

    CollisionSystem system;
    system.register_collision_shape(CollisionShape::create_sphere(1, Vec3(0.0, 0.0, 0.0), 1.0f));
    system.register_collision_shape(CollisionShape::create_sphere(2, Vec3(1.5, 0.0, 0.0), 1.0f));
    system.register_collision_shape(CollisionShape::create_sphere(3, Vec3(5.0, 0.0, 0.0), 1.0f));

    system.update_broad_phase();
    system.solve_narrow_phase(1.0f / 60.0f);

    const ContactManifolds& manifolds = system.get_contact_manifolds();
    assert(!manifolds.empty());
    bool found_pair = false;
    for (const auto& manifold : manifolds) {
        for (int i = 0; i < manifold.point_count; ++i) {
            const ContactPoint& point = manifold.points[i];
            if ((point.entity_a == 1 && point.entity_b == 2) || (point.entity_a == 2 && point.entity_b == 1)) {
                found_pair = true;
                assert(point.penetration > 0.0f);
                break;
            }
        }
        if (found_pair) {
            break;
        }
    }
    assert(found_pair);

    std::vector<EntityID> query_results;
    system.sphere_query_simd(Vec3(0.0, 0.0, 0.0), 2.5f, query_results);
    assert(query_results.size() >= 2);

    EntityID hit_entity = INVALID_COLLISION_ENTITY;
    Vec3 hit_point;
    bool hit = system.raycast_simd(Vec3(-2.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0), 10.0f, hit_entity, hit_point);
    assert(hit && hit_entity != INVALID_COLLISION_ENTITY);

    std::cout << "✓ CollisionSystem test passed" << std::endl;
    return 0;
}
