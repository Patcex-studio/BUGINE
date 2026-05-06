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

    uint32_t num_fragments = std::min(object.fragment_count,
        static_cast<uint32_t>(std::max(2u, static_cast<uint32_t>(4 + avg_size * 0.5f))));
    std::vector<EntityID> fragment_ids;
    fragment_ids.reserve(num_fragments);

    DeterministicRNG rng(static_cast<uint32_t>(object.object_id) ^
                         static_cast<uint32_t>(center_x) ^
                         static_cast<uint32_t>(center_y) ^
                         static_cast<uint32_t>(center_z));

    float impulse_magnitude = impact_impulse.magnitude();
    Vec3 impulse_direction = impulse_magnitude > 0.1f ?
        impact_impulse / impulse_magnitude :
        Vec3(0, 0, -1);

    float base_velocity_scale = std::clamp(impulse_magnitude / 10000.0f, 0.1f, 20.0f);

    for (uint32_t i = 0; i < num_fragments; ++i) {
        float angle = (i * 2.0f * 3.14159f) / num_fragments;
        float radius_offset = 0.3f + rng.next_float() * 0.4f;
        float distance = avg_size * radius_offset * 0.5f;

        Vec3 fragment_position(
            center_x + std::cos(angle) * distance,
            center_y + std::sin(angle) * distance,
            center_z + (i % 2 == 0 ? size_z * 0.25f : -size_z * 0.25f)
        );

        Vec3 scatter = Vec3(
            (rng.next_float() - 0.5f) * 2.0f,
            (rng.next_float() - 0.5f) * 2.0f,
            rng.next_float() * 0.5f
        ).normalized();

        float scatter_strength = 0.3f + rng.next_float() * 0.4f;
        Vec3 velocity = impulse_direction * base_velocity_scale;
        velocity = velocity * (1.0f - scatter_strength) + scatter * base_velocity_scale * scatter_strength;
        velocity *= (0.7f + rng.next_float() * 0.6f);

        float mass = std::max(5.0f, std::min(500.0f, 20.0f + rng.next_float() * (avg_size * 10.0f)));
        EntityID id = physics_core.create_rigid_body(fragment_position, mass);
        fragment_ids.push_back(id);

        if (PhysicsBody* body = physics_core.get_body(id)) {
            body->velocity = velocity;
        }
    }

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
