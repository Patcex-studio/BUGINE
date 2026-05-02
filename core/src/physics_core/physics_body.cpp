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
#include "physics_core/physics_body.h"

namespace physics_core {

PhysicsBody PhysicsBody::create_rigid_body(
    EntityID id,
    const Vec3& pos,
    float mass_val,
    const Mat3x3& inertia
) {
    PhysicsBody body;
    body.position = pos;
    body.mass = mass_val;
    body.inv_mass = (mass_val > MIN_MASS) ? 1.0f / mass_val : 0.0f;
    body.inertia_tensor = inertia;
    
    // Compute inverse of inertia tensor
    // For simplicity, we'll use a scaled inverse (more complex computation deferred)
    body.inertia_tensor_inv = inertia;
    
    body.entity_id = id;
    body.material_type = 0;  // RigidBody
    body.body_type = 1;      // Dynamic
    body.is_sleeping = false;
    body.is_enabled = true;
    body.restitution = 0.5f;
    body.friction = 0.3f;
    body.linear_damping = 0.1f;
    body.angular_damping = 0.1f;
    body.health = 100.0f;  // Default health
    
    return body;
}

PhysicsBody PhysicsBody::create_static_body(EntityID id, const Vec3& pos) {
    PhysicsBody body;
    body.position = pos;
    body.mass = std::numeric_limits<float>::infinity();
    body.inv_mass = 0.0f;
    body.entity_id = id;
    body.material_type = 0;  // RigidBody
    body.body_type = 0;      // Static
    body.is_sleeping = true;
    body.is_enabled = true;
    body.restitution = 0.5f;
    body.friction = 0.5f;
    body.health = std::numeric_limits<float>::infinity();  // Static bodies have infinite health
    
    return body;
}

PhysicsBody PhysicsBody::create_kinematic_body(EntityID id, const Vec3& pos, float mass_val) {
    PhysicsBody body = create_rigid_body(id, pos, mass_val, Mat3x3::identity());
    body.body_type = 2;  // Kinematic
    body.health = 100.0f;  // Kinematic bodies have default health
    return body;
}

}  // namespace physics_core
