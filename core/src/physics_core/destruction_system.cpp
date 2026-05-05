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
#include "physics_core/destruction_system.h"
#include "common/event_bus.h"
#include "physics_core/physics_core.h"
#include <cmath>
#include <algorithm>
#include <immintrin.h>
#include <ai_core/deterministic_rng.h>
#include <glm/glm.hpp>

namespace physics_core {

DestructionSystem::DestructionSystem() {
}

void DestructionSystem::register_object(const DestructibleObject& object) {
    objects_.push_back(object);
}

void DestructionSystem::apply_damage(EntityID object_id, float damage, TerrainSystem& terrain, PhysicsCore& physics_core) {
    for (auto& object : objects_) {
        if (object.object_id != object_id) {
            continue;
        }
        object.structural_integrity -= damage;
        if (object.structural_integrity <= object.destruction_threshold) {
            simulate_collapse(object, terrain, physics_core);
        }
        return;
    }
}

void DestructionSystem::simulate_collapse(DestructibleObject& object, TerrainSystem& terrain, PhysicsCore& physics_core, const Vec3& impact_impulse) {
    if (object.structural_integrity > object.destruction_threshold) {
        return;
    }

    // Extract bounding box bounds
    alignas(32) float min_values[8];
    alignas(32) float max_values[8];
    _mm256_store_ps(min_values, object.world_aabb_min);
    _mm256_store_ps(max_values, object.world_aabb_max);

    float center_x = (min_values[0] + max_values[0]) * 0.5f;
    float center_y = (min_values[1] + max_values[1]) * 0.5f;
    float center_z = (min_values[2] + max_values[2]) * 0.5f;
    float radius = std::min(32.0f, static_cast<float>(object.fragment_count) * 0.8f + 4.0f);
    float depth = std::min(6.0f, radius * 0.18f);

    
    // Calculate size-based crater parameters
    float size_x = max_values[0] - min_values[0];
    float size_y = max_values[1] - min_values[1];
    float size_z = max_values[2] - min_values[2];
    float avg_size = (size_x + size_y) * 0.5f;
    
    float radius = std::min(40.0f, std::max(8.0f, avg_size * 1.2f));
    float depth = std::min(8.0f, radius * 0.22f);

    // Create crater in terrain
    TerrainCrater crater;
    crater.center_x = center_x;
    crater.center_y = center_y;
    crater.center_z = center_z;
    crater.radius = radius;
    crater.depth = depth;
    crater.falloff = 1.4f;
    crater.age = 0;
    crater.max_age = 120;

    craters_.push_back(crater);
    terrain.apply_crater(crater);

    // Publish destruction event
    common::EventBus::Publish(common::DestructionEvent{
        object.object_id,
        Vec3(center_x, center_y, center_z),
        radius,
        2.0f
    });

    // Generate fragments using destruction templates (simplified: triangular fragments)
    std::vector<EntityID> fragment_ids;
    fragment_ids.reserve(object.fragment_count);
    DeterministicRNG rng(static_cast<uint32_t>(object.object_id) ^ static_cast<uint32_t>(center_x) ^ static_cast<uint32_t>(center_y));

    // Assume 4 main fragments for a typical component (e.g., turret)
    uint32_t num_fragments = std::min(object.fragment_count, 4u);
    
    for (uint32_t i = 0; i < num_fragments; ++i) {
        // Position along "weld lines" - simplified as corners
        float angle = (i * 2.0f * 3.14159f) / num_fragments;
        float dist = 0.5f; // Half size
        Vec3 position = {
            center_x + std::cos(angle) * dist,
            center_y + std::sin(angle) * dist,
            center_z
        };

        // Velocity from impact impulse + random
        Vec3 base_velocity = impact_impulse / 1000.0f; // Scale impulse to velocity
        Vec3 random_vel = Vec3(rng.next_float() - 0.5f, rng.next_float() - 0.5f, rng.next_float()) * 5.0f;
        Vec3 velocity = base_velocity + random_vel;

        // Mass based on size (triangular fragment)
        float mass = 50.0f + rng.next_float() * 100.0f; // 50-150 kg

        // Create rigid body
        EntityID id = physics_core.create_rigid_body(position, mass);
        fragment_ids.push_back(id);
        
        // Set velocity directly on the created body
    // Generate realistic number of fragments based on structure
    // Typical destruction generates 4-12 fragments depending on object size and damage
    uint32_t num_fragments = std::min(object.fragment_count, 
        static_cast<uint32_t>(4 + avg_size * 0.5f));  // Scale fragments with size
    num_fragments = std::max(2u, num_fragments);      // At least 2 fragments
    
    std::vector<EntityID> fragment_ids;
    fragment_ids.reserve(num_fragments);
    
    // Deterministic RNG seeded from object properties
    DeterministicRNG rng(static_cast<uint32_t>(object.object_id) ^ 
                         static_cast<uint32_t>(center_x) ^ 
                         static_cast<uint32_t>(center_y) ^
                         static_cast<uint32_t>(center_z));

    // Calculate impulse velocity (normalize impact impulse)
    float impulse_magnitude = impact_impulse.magnitude();
    Vec3 impulse_direction = impulse_magnitude > 0.1f ? 
        impact_impulse / impulse_magnitude : 
        Vec3(0, 0, -1);  // Default downward
    
    float base_velocity_scale = impulse_magnitude / 10000.0f;  // Scale impulse to velocity
    base_velocity_scale = std::max(0.1f, std::min(20.0f, base_velocity_scale));
    
    for (uint32_t i = 0; i < num_fragments; ++i) {
        // Distribute fragments in expanding pattern around impact center
        float angle = (i * 2.0f * 3.14159f) / num_fragments;
        float radius_offset = 0.3f + rng.next_float() * 0.4f;  // 0.3-0.7 of object size
        float distance = avg_size * radius_offset * 0.5f;
        
        Vec3 fragment_position(
            center_x + std::cos(angle) * distance,
            center_y + std::sin(angle) * distance,
            center_z + (i % 2 == 0 ? size_z * 0.25f : -size_z * 0.25f)  // Some up, some down
        );

        // Calculate fragment velocity
        // Base: direction of impulse + scatter
        Vec3 scatter = Vec3(
            (rng.next_float() - 0.5f) * 2.0f,
            (rng.next_float() - 0.5f) * 2.0f,
            rng.next_float() * 0.5f  // Bias upward
        ).normalized();
        
        float scatter_strength = 0.3f + rng.next_float() * 0.4f;  // 30-70% scatter
        Vec3 velocity = impulse_direction * base_velocity_scale;
        velocity = velocity * (1.0f - scatter_strength) + scatter * base_velocity_scale * scatter_strength;
        
        // Add some natural variation
        velocity *= (0.7f + rng.next_float() * 0.6f);  // 0.7-1.3x velocity randomization

        // Fragment mass varies by size
        float mass = 20.0f + rng.next_float() * (avg_size * 10.0f);  // Size-dependent mass
        mass = std::max(5.0f, std::min(500.0f, mass));

        // Create rigid body for fragment
        EntityID id = physics_core.create_rigid_body(fragment_position, mass);
        fragment_ids.push_back(id);
        
        // Set velocity on fragment
        if (PhysicsBody* body = physics_core.get_body(id)) {
            body->velocity = velocity;
        }
    }

    // Mark original for removal
    physics_core.destroy_body(object.object_id);
    object.structural_integrity = 0.0f;
    object.fragment_count = std::max<uint32_t>(object.fragment_count, 1u);
    // Destroy original object
    physics_core.destroy_body(object.object_id);
    object.structural_integrity = 0.0f;
    object.fragment_count = num_fragments;
}

void DestructionSystem::update(float dt, TerrainSystem& terrain) {
    for (auto& crater : craters_) {
        crater.age += static_cast<uint32_t>(std::ceil(dt * 60.0f));
        if (crater.age < crater.max_age) {
            TerrainCrater soften = crater;
            soften.depth *= 0.02f;
            terrain.apply_crater(soften);
        }
    }
    cleanup_old_effects();
}

void DestructionSystem::cleanup_old_effects() {
    craters_.erase(
        std::remove_if(craters_.begin(), craters_.end(), [](const TerrainCrater& crater) {
            return crater.age >= crater.max_age;
        }),
        craters_.end()
    );
}

}  // namespace physics_core
