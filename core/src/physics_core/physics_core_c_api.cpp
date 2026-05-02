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
#include "physics_core/physics_core_c_api.h"
#include "physics_core/physics_core.h"
#include <memory>
#include <unordered_map>

using namespace physics_core;

// ============================================================================
// C API Wrapper Implementation
// ============================================================================

static std::unordered_map<uintptr_t, std::unique_ptr<PhysicsCore>> engine_map;
static uintptr_t next_handle_id = 1;

// Helper to convert C types
static Vec3 c_to_cpp_vec3(Vec3_C v) {
    return Vec3(v.x, v.y, v.z);
}

static Vec3_C cpp_to_c_vec3(const Vec3& v) {
    return {v.x, v.y, v.z};
}

static physics_core::FluidType c_to_cpp_fluid_type(FluidType_C type) {
    switch (type) {
        case FLUID_TYPE_OIL: return physics_core::FluidType::OIL;
        case FLUID_TYPE_GAS: return physics_core::FluidType::GAS;
        default: return physics_core::FluidType::WATER;
    }
}

// ========== Engine Lifecycle ==========

PhysicsCoreHandle physics_core_create(size_t thread_count) {
    auto engine = std::make_unique<PhysicsCore>();
    auto handle = reinterpret_cast<PhysicsCoreHandle>(next_handle_id++);
    engine_map[reinterpret_cast<uintptr_t>(handle)] = std::move(engine);
    return handle;
}

void physics_core_destroy(PhysicsCoreHandle handle) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        engine_map.erase(it);
    }
}

void physics_core_initialize(PhysicsCoreHandle handle) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->initialize();
    }
}

void physics_core_update(PhysicsCoreHandle handle, float dt) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->update(dt);
    }
}

void physics_core_set_gravity(PhysicsCoreHandle handle, Vec3_C gravity) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->set_gravity(c_to_cpp_vec3(gravity));
    }
}

Vec3_C physics_core_get_gravity(PhysicsCoreHandle handle) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return cpp_to_c_vec3(it->second->get_gravity());
    }
    return {0, 0, 0};
}

// ========== Body Management ==========

EntityID physics_core_create_rigid_body(
    PhysicsCoreHandle handle,
    Vec3_C position,
    float mass
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return it->second->create_rigid_body(c_to_cpp_vec3(position), mass);
    }
    return 0;
}

EntityID physics_core_create_static_body(
    PhysicsCoreHandle handle,
    Vec3_C position
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return it->second->create_static_body(c_to_cpp_vec3(position));
    }
    return 0;
}

EntityID physics_create_hinge_joint(
    PhysicsCoreHandle handle,
    EntityID body_a,
    EntityID body_b,
    Vec3_C pivot_a,
    Vec3_C axis_a,
    Vec3_C pivot_b,
    Vec3_C axis_b
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        size_t constraint_id = it->second->create_hinge_constraint(
            body_a,
            body_b,
            c_to_cpp_vec3(pivot_a),
            c_to_cpp_vec3(pivot_b),
            c_to_cpp_vec3(axis_a).normalized(),
            c_to_cpp_vec3(axis_b).normalized(),
            -3.14159265f,
            3.14159265f,
            0.0f
        );
        return static_cast<EntityID>(constraint_id);
    }
    return 0;
}

EntityID physics_create_soft_body(
    PhysicsCoreHandle handle,
    const float* vertices,
    int count
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end() && vertices && count >= 2) {
        uint32_t vertex_count = static_cast<uint32_t>(count);
        Vec3 start(vertices[0], vertices[1], vertices[2]);
        Vec3 end(vertices[(vertex_count - 1) * 3], vertices[(vertex_count - 1) * 3 + 1], vertices[(vertex_count - 1) * 3 + 2]);
        Vec3 direction = (end - start).normalized();
        float length = static_cast<float>((end - start).magnitude());
        if (length < 1e-4f) {
            length = 1.0f;
        }
        size_t soft_id = it->second->create_soft_rope(vertex_count, length, 0.1f, start, direction, true);
        return static_cast<EntityID>(soft_id);
    }
    return 0;
}

EntityID physics_create_fluid_source(
    PhysicsCoreHandle handle,
    FluidType_C type,
    Vec3_C position,
    float rate
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return it->second->create_fluid_source(c_to_cpp_fluid_type(type), c_to_cpp_vec3(position), rate);
    }
    return 0;
}

void physics_core_destroy_body(PhysicsCoreHandle handle, EntityID id) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->destroy_body(id);
    }
}

Vec3_C physics_core_get_body_position(PhysicsCoreHandle handle, EntityID id) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        PhysicsBody* body = it->second->get_body(id);
        if (body) {
            return cpp_to_c_vec3(body->position);
        }
    }
    return {0, 0, 0};
}

void physics_core_set_body_position(PhysicsCoreHandle handle, EntityID id, Vec3_C pos) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        PhysicsBody* body = it->second->get_body(id);
        if (body) {
            body->position = c_to_cpp_vec3(pos);
        }
    }
}

Vec3_C physics_core_get_body_velocity(PhysicsCoreHandle handle, EntityID id) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        PhysicsBody* body = it->second->get_body(id);
        if (body) {
            return cpp_to_c_vec3(body->velocity);
        }
    }
    return {0, 0, 0};
}

void physics_core_set_body_velocity(PhysicsCoreHandle handle, EntityID id, Vec3_C vel) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        PhysicsBody* body = it->second->get_body(id);
        if (body) {
            body->velocity = c_to_cpp_vec3(vel);
        }
    }
}

float physics_core_get_body_mass(PhysicsCoreHandle handle, EntityID id) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        PhysicsBody* body = it->second->get_body(id);
        if (body) {
            return body->mass;
        }
    }
    return 0.0f;
}

size_t physics_core_get_body_count(PhysicsCoreHandle handle) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return it->second->get_body_count();
    }
    return 0;
}

// ========== Forces ==========

void physics_core_apply_force(PhysicsCoreHandle handle, EntityID id, Vec3_C force) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->apply_force(id, c_to_cpp_vec3(force));
    }
}

void physics_core_apply_impulse(
    PhysicsCoreHandle handle,
    EntityID id,
    Vec3_C impulse,
    Vec3_C point
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->apply_impulse(id, c_to_cpp_vec3(impulse), c_to_cpp_vec3(point));
    }
}

// ========== Coordinate Systems ==========

EntityID physics_core_create_local_frame(
    PhysicsCoreHandle handle,
    Vec3_C position,
    EntityID parent_id
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        return it->second->create_local_frame(c_to_cpp_vec3(position), Mat3x3::identity(), 1.0, parent_id);
    }
    return 0;
}

void physics_core_destroy_local_frame(PhysicsCoreHandle handle, EntityID frame_id) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        it->second->destroy_local_frame(frame_id);
    }
}

Vec3_C physics_core_world_to_local(
    PhysicsCoreHandle handle,
    Vec3_C point,
    EntityID frame_id
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        Vec3 result = it->second->get_world_space_manager().world_to_local(
            c_to_cpp_vec3(point), frame_id
        );
        return cpp_to_c_vec3(result);
    }
    return {0, 0, 0};
}

Vec3_C physics_core_local_to_world(
    PhysicsCoreHandle handle,
    Vec3_C point,
    EntityID frame_id
) {
    auto it = engine_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it != engine_map.end()) {
        Vec3 result = it->second->get_world_space_manager().local_to_world(
            c_to_cpp_vec3(point), frame_id
        );
        return cpp_to_c_vec3(result);
    }
    return {0, 0, 0};
}
