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
#include "physics_core/integrators.h"

namespace physics_core {

// ============================================================================
// VerletIntegrator Implementation
// ============================================================================

VerletIntegrator::VerletIntegrator() : initialized_(false) {
}

void VerletIntegrator::integrate(std::vector<PhysicsBody>& bodies, float dt) {
    ensure_storage(bodies.size());
    const float dt_sq = dt * dt;
    
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {  // Skip disabled and static
            continue;
        }
        
        Vec3 current_pos = bodies[i].position;
        Vec3 prev_pos = bodies[i].prev_position;
        Vec3 acceleration = bodies[i].acceleration;
        
        // Защита от неинициализированных prev_position
        if (prev_pos == Vec3() && bodies[i].velocity != Vec3()) {
            // Первый шаг: используем Euler
            Vec3 new_pos = current_pos + bodies[i].velocity * dt + acceleration * (0.5f * dt_sq);
            bodies[i].prev_position = current_pos;
            bodies[i].position = new_pos;
            bodies[i].velocity = (new_pos - current_pos) / dt;
            continue;
        }
        
        // Verlet integration: x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
        Vec3 new_pos = current_pos * 2.0f - prev_pos + acceleration * dt_sq;
        
        // velocity = (p(t+dt) - p(t-dt)) / (2*dt)
        Vec3 velocity = (new_pos - prev_pos) / (2.0f * dt);
        
        // Update
        bodies[i].prev_position = current_pos;
        bodies[i].position = new_pos;
        bodies[i].velocity = velocity;
    }
}

void VerletIntegrator::ensure_storage(size_t body_count) {
    if (previous_positions_.size() < body_count) {
        previous_positions_.resize(body_count);
    }
}

// ============================================================================
// RK4Integrator Implementation
// ============================================================================

RK4Integrator::RK4Integrator() {
}

void RK4Integrator::integrate(std::vector<PhysicsBody>& bodies, float dt) {
    ensure_storage(bodies.size());
    
    const float dt_half = dt * 0.5f;
    const float dt_sixth = dt / 6.0f;
    
    // Stage 1: k1
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) continue;
        
        k1_vel_[i] = bodies[i].acceleration;
        k1_pos_[i] = bodies[i].velocity;
    }
    
    // Stage 2: k2 (evaluate at t + dt/2)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) continue;
        
        Vec3 temp_vel = bodies[i].velocity + k1_vel_[i] * dt_half;
        Vec3 temp_pos = bodies[i].position + k1_pos_[i] * dt_half;
        
        PhysicsBody temp_body = bodies[i];
        temp_body.velocity = temp_vel;
        temp_body.position = temp_pos;
        
        k2_vel_[i] = temp_body.acceleration;
        k2_pos_[i] = temp_vel;
    }
    
    // Stage 3: k3 (evaluate at t + dt/2 with k2)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) continue;
        
        Vec3 temp_vel = bodies[i].velocity + k2_vel_[i] * dt_half;
        Vec3 temp_pos = bodies[i].position + k2_pos_[i] * dt_half;
        
        PhysicsBody temp_body = bodies[i];
        temp_body.velocity = temp_vel;
        temp_body.position = temp_pos;
        
        k3_vel_[i] = temp_body.acceleration;
        k3_pos_[i] = temp_vel;
    }
    
    // Stage 4: k4 (evaluate at t + dt)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) continue;
        
        Vec3 temp_vel = bodies[i].velocity + k3_vel_[i] * dt;
        Vec3 temp_pos = bodies[i].position + k3_pos_[i] * dt;
        
        PhysicsBody temp_body = bodies[i];
        temp_body.velocity = temp_vel;
        temp_body.position = temp_pos;
        
        k4_vel_[i] = temp_body.acceleration;
        k4_pos_[i] = temp_vel;
    }
    
    // Final update: y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) continue;
        
        Vec3 delta_vel = (k1_vel_[i] + k2_vel_[i] * 2.0 + k3_vel_[i] * 2.0 + k4_vel_[i]) * dt_sixth;
        Vec3 delta_pos = (k1_pos_[i] + k2_pos_[i] * 2.0 + k3_pos_[i] * 2.0 + k4_pos_[i]) * dt_sixth;
        
        bodies[i].velocity = bodies[i].velocity + delta_vel;
        bodies[i].position = bodies[i].position + delta_pos;
        
        // Apply damping
        bodies[i].velocity = bodies[i].velocity * (1.0f - bodies[i].linear_damping * dt);
    }
}

void RK4Integrator::ensure_storage(size_t body_count) {
    if (k1_vel_.size() < body_count) {
        k1_vel_.resize(body_count);
        k2_vel_.resize(body_count);
        k3_vel_.resize(body_count);
        k4_vel_.resize(body_count);
        k1_pos_.resize(body_count);
        k2_pos_.resize(body_count);
        k3_pos_.resize(body_count);
        k4_pos_.resize(body_count);
    }
}

Vec3 RK4Integrator::compute_acceleration(const PhysicsBody& body, const Vec3& external_force) {
    if (body.inv_mass == 0.0f) {
        return Vec3();
    }
    return external_force * body.inv_mass;
}

// ============================================================================
// EulerIntegrator Implementation (Baseline)
// ============================================================================

void EulerIntegrator::integrate(std::vector<PhysicsBody>& bodies, float dt) {
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].is_enabled || bodies[i].body_type == 0) {
            continue;
        }
        
        // v += a*dt
        bodies[i].velocity = bodies[i].velocity + bodies[i].acceleration * dt;
        
        // x += v*dt
        bodies[i].position = bodies[i].position + bodies[i].velocity * dt;
        
        // Apply damping
        bodies[i].velocity = bodies[i].velocity * (1.0f - bodies[i].linear_damping * dt);
    }
}

}  // namespace physics_core
