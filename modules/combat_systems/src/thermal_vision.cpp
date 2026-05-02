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
#include "combat_systems/thermal_vision.h"

#include <algorithm>
#include <cmath>

namespace combat_systems {

namespace {

std::pair<float, float> get_entity_xy(physics_core::PhysicsCore* physics, EntityId entity) {
    if (!physics) {
        return {0.0f, 0.0f};
    }
    auto* body = physics->get_body(entity);
    if (!body) {
        return {0.0f, 0.0f};
    }
    return {static_cast<float>(body->position.x), static_cast<float>(body->position.y)};
}

float atmospheric_transmission(const WorldState& world_state) {
    const float rain_loss = world_state.precipitation * 0.45f;
    const float fog_loss = world_state.fog_density * 0.65f;
    return std::clamp(std::pow(10.0f, -(rain_loss + fog_loss) / 10.0f), 0.05f, 1.0f);
}

} // namespace

void ThermalVisionSystem::update(float dt,
                                 ecs::EntityManager& em,
                                 physics_core::PhysicsCore* physics,
                                 const SpatialGrid& spatial_grid,
                                 const WorldState& world_state) {
    if (!physics) {
        return;
    }

    em.query_components<ThermalSensor>([&](EntityId observer, ThermalSensor& sensor) {
        sensor.last_detected = false;
        sensor.last_confidence = 0.0f;

        auto* observer_body = physics->get_body(observer);
        if (!observer_body) {
            return;
        }

        const float max_query_radius = sensor.config.base_detection_range * 1.2f;
        auto nearby = spatial_grid.query_radius(
            static_cast<float>(observer_body->position.x),
            static_cast<float>(observer_body->position.y),
            max_query_radius,
            [&](EntityId id) {
                return get_entity_xy(physics, id);
            });

        for (EntityId target : nearby) {
            if (target == observer) {
                continue;
            }

            auto* signature = em.get_component<ThermalSignature>(target);
            if (!signature) {
                continue;
            }

            auto* target_body = physics->get_body(target);
            if (!target_body) {
                continue;
            }

            float detection_range = sensor.config.base_detection_range;
            float transmission = atmospheric_transmission(world_state);
            float signature_factor = signature->intensity * signature->area * transmission;
            if (!signature->engine_running) {
                signature_factor *= sensor.config.engine_off_penalty;
            }
            if (signature->low_profile) {
                signature_factor *= 0.65f;
            }
            detection_range *= std::clamp(signature_factor * sensor.config.sensitivity, 0.15f, 2.5f);

            if (is_detected(target_body->position, observer_body->position, detection_range)) {
                const float distance = static_cast<float>((target_body->position - observer_body->position).magnitude());
                sensor.last_detected = true;
                sensor.last_confidence = std::clamp(1.0f - distance / detection_range, 0.0f, 1.0f);
                return;
            }
        }
    });
}

float ThermalVisionSystem::get_detection_range(const ThermalSignature& signature,
                                               const WorldState& world_state) const {
    float transmission = atmospheric_transmission(world_state);
    float intensity = signature.intensity * signature.area * transmission;
    if (!signature.engine_running) {
        intensity *= 0.3f;
    }
    if (signature.low_profile) {
        intensity *= 0.65f;
    }
    return 3000.0f * std::clamp(intensity, 0.2f, 2.5f);
}

bool ThermalVisionSystem::is_detected(const physics_core::Vec3& target_position,
                                      const physics_core::Vec3& observer_position,
                                      float detection_range) const {
    const physics_core::Vec3 delta = {
        target_position.x - observer_position.x,
        target_position.y - observer_position.y,
        target_position.z - observer_position.z
    };
    const float distance = static_cast<float>(delta.magnitude());
    return distance <= detection_range;
}

} // namespace combat_systems