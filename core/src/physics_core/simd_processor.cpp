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
#include "physics_core/simd_processor.h"
#include <algorithm>

namespace physics_core {

SIMDProcessor::SIMDProcessor() {
    detect_simd_capabilities();
}

SIMDProcessor::~SIMDProcessor() = default;

void SIMDProcessor::detect_simd_capabilities() {
    // CPUID check for AVX2 and AVX-512
    #ifdef __AVX2__
        has_avx2_ = true;
        optimal_batch_size_ = 4;  // 256-bit / 64-bit per double = 4
    #else
        has_avx2_ = false;
        optimal_batch_size_ = 1;
    #endif
    
    #ifdef __AVX512F__
        has_avx512_ = true;
        optimal_batch_size_ = 8;  // 512-bit / 64-bit per double = 8
    #else
        has_avx512_ = false;
    #endif
}

// ========== Batch Operations ==========

void SIMDProcessor::apply_gravity_batch(
    std::vector<PhysicsBody>& bodies,
    const Vec3& gravity,
    float dt,
    size_t start_index,
    size_t count
) {
    if (count == ~0ull) {
        count = bodies.size();
    }
    count = std::min(count, bodies.size() - start_index);
    
    const Vec3 g_accel = gravity * dt;
    
    // Simple non-SIMD version for now (portable)
    for (size_t i = start_index; i < start_index + count; ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {
            continue;  // Skip disabled and static bodies
        }
        bodies[i].acceleration = bodies[i].acceleration + g_accel;
    }
}

void SIMDProcessor::update_velocities_batch(
    std::vector<PhysicsBody>& bodies,
    float dt,
    size_t start_index,
    size_t count
) {
    if (count == ~0ull) {
        count = bodies.size();
    }
    count = std::min(count, bodies.size() - start_index);
    
    for (size_t i = start_index; i < start_index + count; ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {
            continue;
        }
        bodies[i].velocity = bodies[i].velocity + bodies[i].acceleration * dt;
    }
}

void SIMDProcessor::update_positions_batch(
    std::vector<PhysicsBody>& bodies,
    float dt,
    size_t start_index,
    size_t count
) {
    if (count == ~0ull) {
        count = bodies.size();
    }
    count = std::min(count, bodies.size() - start_index);
    
    for (size_t i = start_index; i < start_index + count; ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {
            continue;
        }
        bodies[i].position = bodies[i].position + bodies[i].velocity * dt;
    }
}

void SIMDProcessor::apply_damping_batch(
    std::vector<PhysicsBody>& bodies,
    float dt,
    size_t start_index,
    size_t count
) {
    if (count == ~0ull) {
        count = bodies.size();
    }
    count = std::min(count, bodies.size() - start_index);
    
    for (size_t i = start_index; i < start_index + count; ++i) {
        if (!bodies[i].is_enabled) {
            continue;
        }
        
        float damping_factor = 1.0f - bodies[i].linear_damping * dt;
        bodies[i].velocity = bodies[i].velocity * damping_factor;
        
        float angular_damping_factor = 1.0f - bodies[i].angular_damping * dt;
        bodies[i].angular_velocity = bodies[i].angular_velocity * angular_damping_factor;
    }
}

void SIMDProcessor::verlet_integrate_batch(
    std::vector<PhysicsBody>& bodies,
    const std::vector<Vec3>& prev_positions,
    float dt,
    size_t start_index,
    size_t count
) {
    if (count == ~0ull) {
        count = bodies.size();
    }
    count = std::min(count, bodies.size() - start_index);
    if (prev_positions.size() < count) {
        return;
    }
    
    const float dt_sq = dt * dt;
    
    for (size_t i = start_index; i < start_index + count; ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {
            continue;
        }
        
        // x(t+dt) = x(t) + (x(t) - x(t-dt)) + a*dt²
        const Vec3& curr_pos = bodies[i].position;
        const Vec3& prev_pos = prev_positions[i];
        Vec3 accel = bodies[i].acceleration;
        
        Vec3 new_pos = curr_pos + (curr_pos - prev_pos) + accel * dt_sq;
        Vec3 velocity = (new_pos - prev_pos) / (2.0f * dt);
        
        bodies[i].position = new_pos;
        bodies[i].velocity = velocity;
    }
}

}  // namespace physics_core
