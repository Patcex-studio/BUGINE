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

#include "types.h"
#include "collision_system.h"
#include <cstdint>
#include <vector>

namespace physics_core {

class RigidBodySystem;
class CollisionSystem;

struct SoftConstraint {
    uint32_t vertex_a;
    uint32_t vertex_b;
    float rest_length;
    float stiffness;
};

struct SoftBody {
    physics_core::EntityID owner_entity = 0;
    std::vector<Vec3> positions;
    std::vector<Vec3> prev_positions;
    std::vector<float> inv_masses;
    std::vector<SoftConstraint> constraints;
    std::vector<uint8_t> pinned;
    float particle_radius = 0.1f;
};

class SoftBodySystem {
public:
    SoftBodySystem();
    ~SoftBodySystem();

    size_t add_soft_body(SoftBody&& body);
    size_t create_rope(
        uint32_t particle_count,
        float length,
        float particle_radius,
        const Vec3& start,
        const Vec3& direction,
        bool pin_start = true,
        physics_core::EntityID owner_entity = 0
    );
    size_t create_cloth(
        uint32_t width,
        uint32_t height,
        float spacing,
        const Vec3& origin,
        bool pin_corners = true,
        physics_core::EntityID owner_entity = 0
    );

    void update(float dt, CollisionSystem& collision_system, RigidBodySystem& rigid_body_system);
    const std::vector<SoftBody>& get_soft_bodies() const { return soft_bodies_; }

private:
    void solve_constraints(SoftBody& body);
    void resolve_collisions(SoftBody& body, CollisionSystem& collision_system, RigidBodySystem& rigid_body_system);
    void enforce_collision_with_shape(SoftBody& body, size_t vertex_index, const CollisionShape& shape);
    void enforce_self_collisions(SoftBody& body);

    Vec3 gravity_;
    std::vector<SoftBody> soft_bodies_;
};

} // namespace physics_core
