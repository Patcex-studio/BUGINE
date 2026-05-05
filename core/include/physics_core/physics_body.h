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
#include <cstdint>

namespace physics_core {

// ============================================================================
// Physics Body Data Structure (Data-Oriented Design)
// ============================================================================

/**
 * @struct PhysicsBody
 * @brief SIMD-optimized physics body structure
 * 
 * Layout criteria:
 * - 32-byte aligned for cache efficiency
 * - Structure of Arrays (SoA) format for batch processing
 * - Minimal padding
 */
struct alignas(32) PhysicsBody {
    // Kinematics
    Vec3 position;          // World space position
    Vec3 prev_position;     // Previous position for Verlet integration
    Vec3 velocity;          // Linear velocity
    Vec3 acceleration;      // Linear acceleration
    Vec3 angular_velocity;  // Rotational velocity
    Vec3 angular_acceleration; // Rotational acceleration
    Vec3 torque;            // Accumulated torque for this frame
    
    // Dynamics
    float mass;             // Positive mass (kg)
    float inv_mass;         // Precomputed 1/mass for efficiency
    float restitution;      // Bounce factor [0, 1]
    float friction;         // Friction coefficient [0, 1]
    
    // Inertia and orientation
    Mat3x3 inertia_tensor;      // Local inertia tensor
    Mat3x3 inertia_tensor_inv;  // Inverse for efficiency
    Mat3x3 orientation;         // Rotation matrix

    // Material properties
    uint32_t material_type;     // Enum: RigidBody=0, Fluid=1, Gas=2
    uint32_t body_type;         // Static=0, Dynamic=1, Kinematic=2
    uint64_t entity_id;         // Reference to parent entity
    uint64_t user_data;         // User-defined data

    // Simulation state
    float linear_damping;       // Air resistance [0, 1]
    float angular_damping;      // Rotational damping [0, 1]
    bool is_sleeping;           // Optimization: sleeping bodies skip simulation
    bool is_enabled;            // Physics enabled/disabled
    
    // Entity state
    float health;               // Health for damageable entities (0.0 = dead)
    float bounding_radius;      // Approximate body radius for CCD and broadphase

    PhysicsBody() = default;

    // Create a default dynamic rigid body
    static PhysicsBody create_rigid_body(
        EntityID id,
        const Vec3& pos,
        float mass_val,
        const Mat3x3& inertia
    );

    // Create a static body (infinite mass)
    static PhysicsBody create_static_body(EntityID id, const Vec3& pos);

    // Create a kinematic body (moves but doesn't respond to forces)
    static PhysicsBody create_kinematic_body(EntityID id, const Vec3& pos, float mass_val);
};

// Static asserts for proper alignment
static_assert(alignof(PhysicsBody) >= 32, "PhysicsBody must be 32-byte aligned");
static_assert(sizeof(Vec3) == 32, "Vec3 must be 32 bytes");
static_assert(sizeof(Mat3x3) == 72, "Mat3x3 must be 72 bytes");

}  // namespace physics_core
