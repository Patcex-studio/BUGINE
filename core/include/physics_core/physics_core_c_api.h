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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// C API for Physics Core (External Integration)
// ============================================================================

/**
 * Opaque pointers to C++ objects
 */
typedef void* PhysicsCoreHandle;
typedef uint64_t EntityID;

/**
 * Vector type for C API
 */
typedef struct {
    double x, y, z;
} Vec3_C;

/**
 * Matrix type (4x4) for C API
 */
typedef struct {
    double data[16];
} Mat4x4_C;

/**
 * Fluid types for C API
 */
typedef enum {
    FLUID_TYPE_WATER = 0,
    FLUID_TYPE_OIL = 1,
    FLUID_TYPE_GAS = 2
} FluidType_C;

// ========== Engine Lifecycle ==========

/**
 * Create physics engine
 */
PhysicsCoreHandle physics_core_create(size_t thread_count);

/**
 * Destroy physics engine
 */
void physics_core_destroy(PhysicsCoreHandle handle);

/**
 * Initialize engine
 */
void physics_core_initialize(PhysicsCoreHandle handle);

/**
 * Update physics
 */
void physics_core_update(PhysicsCoreHandle handle, float dt);

/**
 * Set gravity
 */
void physics_core_set_gravity(PhysicsCoreHandle handle, Vec3_C gravity);

/**
 * Get gravity
 */
Vec3_C physics_core_get_gravity(PhysicsCoreHandle handle);

// ========== Body Management ==========

/**
 * Create rigid body
 */
EntityID physics_core_create_rigid_body(
    PhysicsCoreHandle handle,
    Vec3_C position,
    float mass
);

/**
 * Create static body
 */
EntityID physics_core_create_static_body(
    PhysicsCoreHandle handle,
    Vec3_C position
);

/**
 * Create hinge joint
 */
EntityID physics_create_hinge_joint(
    PhysicsCoreHandle handle,
    EntityID body_a,
    EntityID body_b,
    Vec3_C pivot_a,
    Vec3_C axis_a,
    Vec3_C pivot_b,
    Vec3_C axis_b
);

/**
 * Create soft body
 */
EntityID physics_create_soft_body(
    PhysicsCoreHandle handle,
    const float* vertices,
    int count
);

/**
 * Create a fluid source
 */
EntityID physics_create_fluid_source(
    PhysicsCoreHandle handle,
    FluidType_C type,
    Vec3_C position,
    float rate
);

/**
 * Destroy body
 */
void physics_core_destroy_body(PhysicsCoreHandle handle, EntityID id);

/**
 * Get body position
 */
Vec3_C physics_core_get_body_position(PhysicsCoreHandle handle, EntityID id);

/**
 * Set body position
 */
void physics_core_set_body_position(PhysicsCoreHandle handle, EntityID id, Vec3_C pos);

/**
 * Get body velocity
 */
Vec3_C physics_core_get_body_velocity(PhysicsCoreHandle handle, EntityID id);

/**
 * Set body velocity
 */
void physics_core_set_body_velocity(PhysicsCoreHandle handle, EntityID id, Vec3_C vel);

/**
 * Get body mass
 */
float physics_core_get_body_mass(PhysicsCoreHandle handle, EntityID id);

/**
 * Get body count
 */
size_t physics_core_get_body_count(PhysicsCoreHandle handle);

// ========== Forces ==========

/**
 * Apply force to body
 */
void physics_core_apply_force(PhysicsCoreHandle handle, EntityID id, Vec3_C force);

/**
 * Apply impulse to body
 */
void physics_core_apply_impulse(
    PhysicsCoreHandle handle,
    EntityID id,
    Vec3_C impulse,
    Vec3_C point
);

// ========== Coordinate Systems ==========

/**
 * Create local coordinate frame
 */
EntityID physics_core_create_local_frame(
    PhysicsCoreHandle handle,
    Vec3_C position,
    EntityID parent_id
);

/**
 * Destroy local frame
 */
void physics_core_destroy_local_frame(PhysicsCoreHandle handle, EntityID frame_id);

/**
 * Transform point from world to local space
 */
Vec3_C physics_core_world_to_local(
    PhysicsCoreHandle handle,
    Vec3_C point,
    EntityID frame_id
);

/**
 * Transform point from local to world space
 */
Vec3_C physics_core_local_to_world(
    PhysicsCoreHandle handle,
    Vec3_C point,
    EntityID frame_id
);

#ifdef __cplusplus
}
#endif
