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
#include "physics_core/constraint_system.h"
#include "physics_core/matter_systems.h"
#include <cmath>

namespace physics_core {

ConstraintSolver::ConstraintSolver() = default;
ConstraintSolver::~ConstraintSolver() = default;

void ConstraintSolver::clear() {
    body_a_.clear();
    body_b_.clear();
    anchor_a_x_.clear();
    anchor_a_y_.clear();
    anchor_a_z_.clear();
    anchor_b_x_.clear();
    anchor_b_y_.clear();
    anchor_b_z_.clear();
    target_distance_.clear();
    compliance_.clear();
    error_.clear();
    lambda_x_.clear();
    lambda_y_.clear();
    lambda_z_.clear();
    type_.clear();
    hinge_axis_a_x_.clear();
    hinge_axis_a_y_.clear();
    hinge_axis_a_z_.clear();
    hinge_axis_b_x_.clear();
    hinge_axis_b_y_.clear();
    hinge_axis_b_z_.clear();
    min_angle_.clear();
    max_angle_.clear();
}

size_t ConstraintSolver::add_distance_constraint(
    EntityID body_a,
    EntityID body_b,
    const Vec3& local_anchor_a,
    const Vec3& local_anchor_b,
    float target_distance,
    float compliance
) {
    body_a_.push_back(body_a);
    body_b_.push_back(body_b);
    
    // Store anchor coordinates in separate arrays for SIMD friendliness
    anchor_a_x_.push_back(local_anchor_a.x);
    anchor_a_y_.push_back(local_anchor_a.y);
    anchor_a_z_.push_back(local_anchor_a.z);
    anchor_b_x_.push_back(local_anchor_b.x);
    anchor_b_y_.push_back(local_anchor_b.y);
    anchor_b_z_.push_back(local_anchor_b.z);
    
    target_distance_.push_back(target_distance);
    compliance_.push_back(compliance);
    error_.push_back(0.0f);
    
    lambda_x_.push_back(0.0f);
    lambda_y_.push_back(0.0f);
    lambda_z_.push_back(0.0f);
    
    type_.push_back(ConstraintType::DISTANCE);
    
    // Placeholder for hinge data
    hinge_axis_a_x_.push_back(0.0f);
    hinge_axis_a_y_.push_back(0.0f);
    hinge_axis_a_z_.push_back(0.0f);
    hinge_axis_b_x_.push_back(0.0f);
    hinge_axis_b_y_.push_back(0.0f);
    hinge_axis_b_z_.push_back(0.0f);
    min_angle_.push_back(0.0f);
    max_angle_.push_back(0.0f);
    
    return body_a_.size() - 1;
}

size_t ConstraintSolver::add_hinge_constraint(
    EntityID body_a,
    EntityID body_b,
    const Vec3& local_anchor_a,
    const Vec3& local_anchor_b,
    const Vec3& hinge_axis_a,
    const Vec3& hinge_axis_b,
    float min_angle_rad,
    float max_angle_rad,
    float compliance
) {
    body_a_.push_back(body_a);
    body_b_.push_back(body_b);
    
    anchor_a_x_.push_back(local_anchor_a.x);
    anchor_a_y_.push_back(local_anchor_a.y);
    anchor_a_z_.push_back(local_anchor_a.z);
    anchor_b_x_.push_back(local_anchor_b.x);
    anchor_b_y_.push_back(local_anchor_b.y);
    anchor_b_z_.push_back(local_anchor_b.z);
    
    target_distance_.push_back((local_anchor_b - local_anchor_a).magnitude());
    compliance_.push_back(compliance);
    error_.push_back(0.0f);
    
    lambda_x_.push_back(0.0f);
    lambda_y_.push_back(0.0f);
    lambda_z_.push_back(0.0f);
    
    type_.push_back(ConstraintType::HINGE);
    
    hinge_axis_a_x_.push_back(hinge_axis_a.x);
    hinge_axis_a_y_.push_back(hinge_axis_a.y);
    hinge_axis_a_z_.push_back(hinge_axis_a.z);
    hinge_axis_b_x_.push_back(hinge_axis_b.x);
    hinge_axis_b_y_.push_back(hinge_axis_b.y);
    hinge_axis_b_z_.push_back(hinge_axis_b.z);
    
    min_angle_.push_back(min_angle_rad);
    max_angle_.push_back(max_angle_rad);
    
    return body_a_.size() - 1;
}

PhysicsBody* ConstraintSolver::get_body(RigidBodySystem& rigid_body_system, EntityID id) const {
    return rigid_body_system.get_body(id);
}

float ConstraintSolver::compute_effective_mass(const PhysicsBody* a, const PhysicsBody* b) const {
    float inv_mass_a = a ? a->inv_mass : 0.0f;
    float inv_mass_b = b ? b->inv_mass : 0.0f;
    float inv_mass_sum = inv_mass_a + inv_mass_b;
    return inv_mass_sum > 0.0f ? 1.0f / inv_mass_sum : 0.0f;
}

void ConstraintSolver::solve(float dt, RigidBodySystem& rigid_body_system, const ContactManifolds& contacts) {
    constexpr int kIterations = 8;
    size_t count = body_a_.size();
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        size_t index = 0;
        while (index < count) {
            size_t batch_size = std::min<size_t>(BATCH_SIZE, count - index);
            // Use SIMD version if available (AVX2)
#ifdef __AVX2__
            if (batch_size == BATCH_SIZE) {
                solve_batch_simd(index, batch_size, dt, rigid_body_system);
            } else {
                solve_batch(index, batch_size, dt, rigid_body_system);
            }
#else
            solve_batch(index, batch_size, dt, rigid_body_system);
#endif
            index += batch_size;
        }
        solve_contacts(contacts, rigid_body_system);
    }
}

void ConstraintSolver::solve_batch(size_t start, size_t count, float dt, RigidBodySystem& rigid_body_system) {
    for (size_t i = start; i < start + count; ++i) {
        PhysicsBody* bodyA = get_body(rigid_body_system, body_a_[i]);
        PhysicsBody* bodyB = get_body(rigid_body_system, body_b_[i]);
        if (!bodyA || !bodyB) {
            continue;
        }

        // Transform local anchors to world space
        Vec3 anchorA = bodyA->position + bodyA->orientation * Vec3(
            anchor_a_x_[i], anchor_a_y_[i], anchor_a_z_[i]
        );
        Vec3 anchorB = bodyB->position + bodyB->orientation * Vec3(
            anchor_b_x_[i], anchor_b_y_[i], anchor_b_z_[i]
        );
        
        Vec3 delta = anchorB - anchorA;
        float distance = static_cast<float>(delta.magnitude());

        if (distance < 1e-6f) {
            continue;
        }

        Vec3 normal = delta / distance;
        float error = distance - target_distance_[i];
        error_[i] = error;

        float effective_mass = compute_effective_mass(bodyA, bodyB);
        float lambda = -error / (effective_mass + compliance_[i] + 1e-6f);
        
        // Accumulate lambda
        lambda_x_[i] += lambda * normal.x;
        lambda_y_[i] += lambda * normal.y;
        lambda_z_[i] += lambda * normal.z;
        
        Vec3 impulse = normal * lambda;

        if (bodyA->inv_mass > 0.0f) {
            bodyA->velocity = bodyA->velocity + impulse * bodyA->inv_mass;
        }
        if (bodyB->inv_mass > 0.0f) {
            bodyB->velocity = bodyB->velocity - impulse * bodyB->inv_mass;
        }

        if (type_[i] == ConstraintType::HINGE) {
            enforce_hinge(bodyA, bodyB, i);
        }
    }
}

#ifdef __AVX2__
void ConstraintSolver::solve_batch_simd(size_t start, size_t count, float dt, RigidBodySystem& rigid_body_system) {
    // Load constraint data into SIMD registers
    __m256 target_dist = _mm256_load_ps(&target_distance_[start]);
    __m256 compliance = _mm256_load_ps(&compliance_[start]);
    __m256 error = _mm256_load_ps(&error_[start]);
    
    // Load anchor coordinates
    __m256 anchor_ax = _mm256_load_ps(&anchor_a_x_[start]);
    __m256 anchor_ay = _mm256_load_ps(&anchor_a_y_[start]);
    __m256 anchor_az = _mm256_load_ps(&anchor_a_z_[start]);
    __m256 anchor_bx = _mm256_load_ps(&anchor_b_x_[start]);
    __m256 anchor_by = _mm256_load_ps(&anchor_b_y_[start]);
    __m256 anchor_bz = _mm256_load_ps(&anchor_b_z_[start]);
    
    // For now, process scalar due to body lookup complexity
    // TODO: Vectorize body position/orientation access
    for (size_t i = 0; i < count; ++i) {
        size_t idx = start + i;
        PhysicsBody* bodyA = get_body(rigid_body_system, body_a_[idx]);
        PhysicsBody* bodyB = get_body(rigid_body_system, body_b_[idx]);
        if (!bodyA || !bodyB) continue;
        
        // Transform anchors to world space
        Vec3 local_anchor_a(anchor_a_x_[idx], anchor_a_y_[idx], anchor_a_z_[idx]);
        Vec3 local_anchor_b(anchor_b_x_[idx], anchor_b_y_[idx], anchor_b_z_[idx]);
        
        Vec3 anchorA = bodyA->position + bodyA->orientation * local_anchor_a;
        Vec3 anchorB = bodyB->position + bodyB->orientation * local_anchor_b;
        
        Vec3 delta = anchorB - anchorA;
        float distance = delta.magnitude();
        
        if (distance < 1e-6f) continue;
        
        Vec3 normal = delta / distance;
        float constraint_error = distance - target_distance_[idx];
        error_[idx] = constraint_error;
        
        float effective_mass = compute_effective_mass(bodyA, bodyB);
        float lambda = -constraint_error / (effective_mass + compliance_[idx] + 1e-6f);
        
        // Accumulate lambda
        lambda_x_[idx] += lambda * normal.x;
        lambda_y_[idx] += lambda * normal.y;
        lambda_z_[idx] += lambda * normal.z;
        
        Vec3 impulse = normal * lambda;
        
        if (bodyA->inv_mass > 0.0f) {
            bodyA->velocity += impulse * bodyA->inv_mass;
        }
        if (bodyB->inv_mass > 0.0f) {
            bodyB->velocity -= impulse * bodyB->inv_mass;
        }
        
        if (type_[idx] == ConstraintType::HINGE) {
            enforce_hinge(bodyA, bodyB, idx);
        }
    }
}
#endif

void ConstraintSolver::enforce_hinge(PhysicsBody* a, PhysicsBody* b, size_t index) const {
    if (!a || !b) {
        return;
    }

    Vec3 axisA = a->orientation * Vec3(
        hinge_axis_a_x_[index], hinge_axis_a_y_[index], hinge_axis_a_z_[index]
    );
    Vec3 axisB = b->orientation * Vec3(
        hinge_axis_b_x_[index], hinge_axis_b_y_[index], hinge_axis_b_z_[index]
    );

    double dot = axisA.normalized().dot(axisB.normalized());
    dot = std::clamp(dot, -1.0, 1.0);
    float angle = static_cast<float>(std::acos(dot));
    float target = angle;

    if (angle < min_angle_[index]) {
        target = min_angle_[index];
    } else if (angle > max_angle_[index]) {
        target = max_angle_[index];
    }

    float angle_error = angle - target;
    if (std::abs(angle_error) < 1e-3f) {
        return;
    }

    Vec3 corrective = axisA.cross(axisB);
    float magnitude = static_cast<float>(corrective.magnitude());
    if (magnitude < 1e-6f) {
        return;
    }

    corrective = corrective / magnitude * angle_error * 0.5f;
    if (a->inv_mass > 0.0f) {
        a->angular_velocity = a->angular_velocity + corrective * a->inv_mass;
    }
    if (b->inv_mass > 0.0f) {
        b->angular_velocity = b->angular_velocity - corrective * b->inv_mass;
    }
}

void ConstraintSolver::solve_contacts(const ContactManifolds& contacts, RigidBodySystem& rigid_body_system) {
    for (const ContactManifold& manifold : contacts) {
        for (int point_index = 0; point_index < manifold.point_count; ++point_index) {
            const ContactPoint& point = manifold.points[point_index];
            PhysicsBody* bodyA = get_body(rigid_body_system, point.entity_a);
            PhysicsBody* bodyB = get_body(rigid_body_system, point.entity_b);
            if (!bodyA || !bodyB) {
                continue;
            }

            float inv_mass_sum = bodyA->inv_mass + bodyB->inv_mass;
            if (inv_mass_sum <= 0.0f) {
                continue;
            }

            float lambda = point.penetration / (inv_mass_sum + 1e-6f);
            Vec3 impulse = point.normal * lambda;

            if (bodyA->inv_mass > 0.0f) {
                bodyA->velocity = bodyA->velocity + impulse * bodyA->inv_mass;
            }
            if (bodyB->inv_mass > 0.0f) {
                bodyB->velocity = bodyB->velocity - impulse * bodyB->inv_mass;
            }
        }
    }
}

} // namespace physics_core
