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
#include "physics_body.h"
#include "collision_system.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace physics_core {

class RigidBodySystem;

enum class ConstraintType : uint8_t {
    DISTANCE = 0,
    BALL_AND_SOCKET = 1,
    HINGE = 2,
    SLIDER = 3,
    FIXED = 4
};

class ConstraintSolver {
public:
    ConstraintSolver();
    ~ConstraintSolver();

    void clear();

    size_t add_distance_constraint(
        EntityID body_a,
        EntityID body_b,
        const Vec3& local_anchor_a,
        const Vec3& local_anchor_b,
        float target_distance,
        float compliance = 0.0f
    );

    size_t add_hinge_constraint(
        EntityID body_a,
        EntityID body_b,
        const Vec3& local_anchor_a,
        const Vec3& local_anchor_b,
        const Vec3& hinge_axis_a,
        const Vec3& hinge_axis_b,
        float min_angle_rad,
        float max_angle_rad,
        float compliance = 0.0f
    );

    void solve(float dt, RigidBodySystem& rigid_body_system, const ContactManifolds& contacts);

private:
    static constexpr size_t ALIGNMENT = 64;
    static constexpr size_t BATCH_SIZE = 8;  // AVX2 batch size

    void solve_batch(size_t start, size_t count, float dt, RigidBodySystem& rigid_body_system);
    void solve_batch_simd(size_t start, size_t count, float dt, RigidBodySystem& rigid_body_system);
    void solve_contacts(const ContactManifolds& contacts, RigidBodySystem& rigid_body_system);
    void enforce_hinge(PhysicsBody* a, PhysicsBody* b, size_t index) const;

    float compute_effective_mass(const PhysicsBody* a, const PhysicsBody* b) const;
    PhysicsBody* get_body(RigidBodySystem& rigid_body_system, EntityID id) const;

    // SoA arrays for contacts and constraints
    alignas(ALIGNMENT) std::vector<EntityID> body_a_;
    alignas(ALIGNMENT) std::vector<EntityID> body_b_;
    
    // Anchor positions (separate X,Y,Z for SIMD)
    alignas(ALIGNMENT) std::vector<float> anchor_a_x_;
    alignas(ALIGNMENT) std::vector<float> anchor_a_y_;
    alignas(ALIGNMENT) std::vector<float> anchor_a_z_;
    alignas(ALIGNMENT) std::vector<float> anchor_b_x_;
    alignas(ALIGNMENT) std::vector<float> anchor_b_y_;
    alignas(ALIGNMENT) std::vector<float> anchor_b_z_;
    
    // Constraint parameters
    alignas(ALIGNMENT) std::vector<float> target_distance_;
    alignas(ALIGNMENT) std::vector<float> compliance_;
    alignas(ALIGNMENT) std::vector<float> error_;
    
    // Accumulated impulses (vectorized)
    alignas(ALIGNMENT) std::vector<float> lambda_x_;
    alignas(ALIGNMENT) std::vector<float> lambda_y_;
    alignas(ALIGNMENT) std::vector<float> lambda_z_;
    
    alignas(ALIGNMENT) std::vector<ConstraintType> type_;

    // Hinge-specific data
    alignas(ALIGNMENT) std::vector<float> hinge_axis_a_x_;
    alignas(ALIGNMENT) std::vector<float> hinge_axis_a_y_;
    alignas(ALIGNMENT) std::vector<float> hinge_axis_a_z_;
    alignas(ALIGNMENT) std::vector<float> hinge_axis_b_x_;
    alignas(ALIGNMENT) std::vector<float> hinge_axis_b_y_;
    alignas(ALIGNMENT) std::vector<float> hinge_axis_b_z_;
    
    alignas(ALIGNMENT) std::vector<float> min_angle_;
    alignas(ALIGNMENT) std::vector<float> max_angle_;
};

} // namespace physics_core
