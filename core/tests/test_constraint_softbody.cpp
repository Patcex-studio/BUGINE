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
#include "physics_core/collision_system.h"
#include "physics_core/constraint_system.h"
#include "physics_core/matter_systems.h"
#include "physics_core/soft_body_system.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace physics_core;

static float distance_between_bodies(const PhysicsBody* a, const PhysicsBody* b) {
    if (!a || !b) {
        return 0.0f;
    }
    return static_cast<float>((b->position - a->position).magnitude());
}

TEST_CASE("ConstraintSolver maintains a chain of distance constraints", "[constraint]") {
    RigidBodySystem system;
    ConstraintSolver solver;

    PhysicsBody anchor = PhysicsBody::create_static_body(1, Vec3(0.0, 0.0, 0.0));
    PhysicsBody link1 = PhysicsBody::create_rigid_body(2, Vec3(0.0, -1.0, 0.0), 1.0f, Mat3x3::identity());
    PhysicsBody link2 = PhysicsBody::create_rigid_body(3, Vec3(0.0, -2.0, 0.0), 1.0f, Mat3x3::identity());
    PhysicsBody link3 = PhysicsBody::create_rigid_body(4, Vec3(0.0, -3.0, 0.0), 1.0f, Mat3x3::identity());

    EntityID id_anchor = system.add_body(anchor);
    EntityID id_1 = system.add_body(link1);
    EntityID id_2 = system.add_body(link2);
    EntityID id_3 = system.add_body(link3);

    solver.add_distance_constraint(id_anchor, id_1, Vec3(), Vec3(), 1.0f, 0.0f);
    solver.add_distance_constraint(id_1, id_2, Vec3(), Vec3(), 1.0f, 0.0f);
    solver.add_distance_constraint(id_2, id_3, Vec3(), Vec3(), 1.0f, 0.0f);

    for (int frame = 0; frame < 120; ++frame) {
        system.update(1.0f / 60.0f);
        ContactManifolds contacts;
        solver.solve(1.0f / 60.0f, system, contacts);
    }

    const PhysicsBody* body0 = system.get_body(id_anchor);
    const PhysicsBody* body1 = system.get_body(id_1);
    const PhysicsBody* body2 = system.get_body(id_2);
    const PhysicsBody* body3 = system.get_body(id_3);

    REQUIRE(body0 != nullptr);
    REQUIRE(body1 != nullptr);
    REQUIRE(body2 != nullptr);
    REQUIRE(body3 != nullptr);

    float d01 = distance_between_bodies(body0, body1);
    float d12 = distance_between_bodies(body1, body2);
    float d23 = distance_between_bodies(body2, body3);

    CHECK(d01 == Catch::Approx(1.0f).margin(0.15f));
    CHECK(d12 == Catch::Approx(1.0f).margin(0.15f));
    CHECK(d23 == Catch::Approx(1.0f).margin(0.15f));
}

TEST_CASE("SoftBodySystem cube resists penetration and deforms under gravity", "[softbody]") {
    RigidBodySystem rigid_system;
    CollisionSystem collision_system;
    SoftBodySystem soft_system;

    PhysicsBody floor = PhysicsBody::create_static_body(10, Vec3(0.0, -1.5, 0.0));
    rigid_system.add_body(floor);
    collision_system.register_collision_shape(CollisionShape::create_box(
        floor.entity_id,
        Vec3(0.0, -1.5, 0.0),
        Vec3(10.0, 1.0, 10.0),
        Mat3x3::identity(),
        false
    ));

    SoftBody cube;
    const float half_size = 0.25f;
    cube.positions = {
        Vec3(-half_size, 0.25, -half_size), Vec3(half_size, 0.25, -half_size),
        Vec3(-half_size, 0.25, half_size), Vec3(half_size, 0.25, half_size),
        Vec3(-half_size, -half_size, -half_size), Vec3(half_size, -half_size, -half_size),
        Vec3(-half_size, -half_size, half_size), Vec3(half_size, -half_size, half_size)
    };
    cube.prev_positions = cube.positions;
    cube.inv_masses.assign(cube.positions.size(), 1.0f);
    cube.pinned.assign(cube.positions.size(), 0);
    cube.particle_radius = 0.05f;

    auto add_edge = [&](size_t a, size_t b) {
        Vec3 delta = cube.positions[a] - cube.positions[b];
        cube.constraints.push_back({static_cast<uint32_t>(a), static_cast<uint32_t>(b), static_cast<float>(delta.magnitude()), 1.0f});
    };

    add_edge(0, 1); add_edge(0, 2); add_edge(1, 3); add_edge(2, 3);
    add_edge(4, 5); add_edge(4, 6); add_edge(5, 7); add_edge(6, 7);
    add_edge(0, 4); add_edge(1, 5); add_edge(2, 6); add_edge(3, 7);
    add_edge(0, 3); add_edge(1, 2); add_edge(4, 7); add_edge(5, 6);

    soft_system.add_soft_body(std::move(cube));

    for (int step = 0; step < 120; ++step) {
        soft_system.update(1.0f / 60.0f, collision_system, rigid_system);
    }

    const SoftBody& result = soft_system.get_soft_bodies().front();
    float min_y = result.positions[0].y;
    for (const Vec3& pos : result.positions) {
        min_y = std::min(min_y, static_cast<float>(pos.y));
    }

    CHECK(min_y >= -0.5f);
}
