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
#include "physics_core/matter_systems.h"
#include <immintrin.h>  // AVX2 intrinsics

namespace physics_core {

// ============================================================================
// RigidBodySystem Implementation
// ============================================================================

RigidBodySystem::RigidBodySystem()
    : bodies_(1024),
      gravity_(0, -9.81f, 0),
      simd_processor_(std::make_unique<SIMDProcessor>()) {
}

RigidBodySystem::~RigidBodySystem() = default;

void RigidBodySystem::update(float dt) {
    update_forces_and_torques(dt);
    bodies_.process_active_batch_simd(0, bodies_.get_active_count(), dt);
}

EntityID RigidBodySystem::add_body(const PhysicsBody& body) {
    EntityID id = bodies_.register_entity(body);
    force_x_.push_back(0.0f);
    force_y_.push_back(0.0f);
    force_z_.push_back(0.0f);
    torque_x_.push_back(0.0f);
    torque_y_.push_back(0.0f);
    torque_z_.push_back(0.0f);
    return id;
}

void RigidBodySystem::remove_body(EntityID id) {
    auto result = bodies_.remove_entity(id);
    if (!result.success) {
        return;
    }

    if (result.swapped_from_back) {
        force_x_[result.removed_index] = force_x_.back();
        force_y_[result.removed_index] = force_y_.back();
        force_z_[result.removed_index] = force_z_.back();
        torque_x_[result.removed_index] = torque_x_.back();
        torque_y_[result.removed_index] = torque_y_.back();
        torque_z_[result.removed_index] = torque_z_.back();
    }

    force_x_.pop_back();
    force_y_.pop_back();
    force_z_.pop_back();
    torque_x_.pop_back();
    torque_y_.pop_back();
    torque_z_.pop_back();
}

PhysicsBody* RigidBodySystem::get_body(EntityID id) {
    return bodies_.get_body(id);
}

void RigidBodySystem::apply_force(EntityID id, const Vec3& force) {
    size_t index = bodies_.get_index(id);
    if (index == SIZE_MAX || index >= force_x_.size()) {
        return;
    }
    force_x_[index] += force.x;
    force_y_[index] += force.y;
    force_z_[index] += force.z;
}

void RigidBodySystem::apply_impulse(EntityID id, const Vec3& impulse, const Vec3& point) {
    PhysicsBody* body = bodies_.get_body(id);
    if (!body) {
        return;
    }

    body->velocity = body->velocity + impulse * body->inv_mass;
}

void RigidBodySystem::apply_torque(EntityID id, const Vec3& torque) {
    size_t index = bodies_.get_index(id);
    if (index == SIZE_MAX || index >= torque_x_.size()) {
        return;
    }

    torque_x_[index] += static_cast<float>(torque.x);
    torque_y_[index] += static_cast<float>(torque.y);
    torque_z_[index] += static_cast<float>(torque.z);
}

void RigidBodySystem::set_gravity(const Vec3& gravity) {
    gravity_ = gravity;
}

void RigidBodySystem::set_gravity_enabled(EntityID id, bool enabled) {
    PhysicsBody* body = bodies_.get_body(id);
    if (body) {
        body->is_enabled = enabled;
    }
}

void RigidBodySystem::update_forces_and_torques(float dt) {
    auto& bodies = bodies_.get_all_bodies();
    const size_t count = bodies.size();
    
    if (count == 0) return;

    // SIMD batch processing for force accumulation
    const size_t batch_size = 8;  // AVX2 can process 8 floats at once
    size_t i = 0;

    // Process in batches of 8
    for (; i + batch_size <= count; i += batch_size) {
        // Load forces
        __m256 fx = _mm256_loadu_ps(force_x_.data() + i);
        __m256 fy = _mm256_loadu_ps(force_y_.data() + i);
        __m256 fz = _mm256_loadu_ps(force_z_.data() + i);

        // Load gravity
        __m256 gx = _mm256_set1_ps(static_cast<float>(gravity_.x));
        __m256 gy = _mm256_set1_ps(static_cast<float>(gravity_.y));
        __m256 gz = _mm256_set1_ps(static_cast<float>(gravity_.z));

        // Load body masses and types
        __m256 masses[batch_size];
        __m256 body_types[batch_size];
        __m256 inv_masses[batch_size];
        
        for (size_t j = 0; j < batch_size; ++j) {
            const PhysicsBody& body = bodies[i + j];
            masses[j] = _mm256_set1_ps(static_cast<float>(body.mass));
            body_types[j] = _mm256_set1_ps(static_cast<float>(body.body_type));
            inv_masses[j] = _mm256_set1_ps(static_cast<float>(body.inv_mass));
        }

        // Apply gravity to dynamic bodies (body_type == 1)
        __m256 dynamic_mask = _mm256_cmp_ps(body_types[0], _mm256_set1_ps(1.0f), _CMP_EQ_OQ);
        fx = _mm256_add_ps(fx, _mm256_and_ps(gx, dynamic_mask));
        fy = _mm256_add_ps(fy, _mm256_and_ps(gy, dynamic_mask));
        fz = _mm256_add_ps(fz, _mm256_and_ps(gz, dynamic_mask));

        // Compute accelerations: a = F / m
        __m256 ax = _mm256_mul_ps(fx, inv_masses[0]);
        __m256 ay = _mm256_mul_ps(fy, inv_masses[0]);
        __m256 az = _mm256_mul_ps(fz, inv_masses[0]);

        // Store accelerations back to bodies
        for (size_t j = 0; j < batch_size; ++j) {
            PhysicsBody& body = bodies[i + j];
            if (body.is_enabled && body.body_type != 0) {
                float ax_val = ((float*)&ax)[j];
                float ay_val = ((float*)&ay)[j];
                float az_val = ((float*)&az)[j];
                body.acceleration = Vec3(ax_val, ay_val, az_val);
            } else {
                body.acceleration = Vec3();
            }
        }

        // Clear forces for next frame
        __m256 zero = _mm256_setzero_ps();
        _mm256_storeu_ps(force_x_.data() + i, zero);
        _mm256_storeu_ps(force_y_.data() + i, zero);
        _mm256_storeu_ps(force_z_.data() + i, zero);
    }

    // Handle remaining elements
    for (; i < count; ++i) {
        PhysicsBody& body = bodies[i];
        if (!body.is_enabled || body.body_type == 0) {
            body.acceleration = Vec3();
            force_x_[i] = 0.0f;
            force_y_[i] = 0.0f;
            force_z_[i] = 0.0f;
            continue;
        }

        Vec3 total_force(force_x_[i], force_y_[i], force_z_[i]);
        if (body.body_type == 1) {
            total_force = total_force + gravity_ * body.mass;
        }

        body.acceleration = total_force * body.inv_mass;
        
        // Clear forces
        force_x_[i] = 0.0f;
        force_y_[i] = 0.0f;
        force_z_[i] = 0.0f;
    }

    // Apply accumulated torques and update angular state
    for (size_t j = 0; j < count; ++j) {
        PhysicsBody& body = bodies[j];
        if (!body.is_enabled || body.body_type == 0) {
            torque_x_[j] = 0.0f;
            torque_y_[j] = 0.0f;
            torque_z_[j] = 0.0f;
            continue;
        }

        Vec3 torque(static_cast<double>(torque_x_[j]), static_cast<double>(torque_y_[j]), static_cast<double>(torque_z_[j]));
        body.angular_acceleration = body.inertia_tensor_inv * torque;
        body.angular_velocity = body.angular_velocity + body.angular_acceleration * dt;

        // Small-angle approximation for orientation update
        Vec3 omega_dt = body.angular_velocity * static_cast<double>(dt);
        Mat3x3 omega_skew;
        omega_skew(0, 0) = 0.0;
        omega_skew(0, 1) = -omega_dt.z;
        omega_skew(0, 2) = omega_dt.y;
        omega_skew(1, 0) = omega_dt.z;
        omega_skew(1, 1) = 0.0;
        omega_skew(1, 2) = -omega_dt.x;
        omega_skew(2, 0) = -omega_dt.y;
        omega_skew(2, 1) = omega_dt.x;
        omega_skew(2, 2) = 0.0;

        body.orientation = body.orientation + omega_skew * body.orientation;

        torque_x_[j] = 0.0f;
        torque_y_[j] = 0.0f;
        torque_z_[j] = 0.0f;
    }
}

}  // namespace physics_core

