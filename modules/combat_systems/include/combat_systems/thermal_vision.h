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

#include <cstdint>
#include <algorithm>
#include "ecs/entity_manager.h"
#include "ai_core/spatial_grid.h"
#include "ai_core/dynamic_modifiers.h"
#include "physics_core/physics_core.h"

namespace combat_systems {

enum class ThermalSource : uint8_t {
    ENGINE = 0,
    CREW,
    SUN_HEATED,
    FIRE
};

struct ThermalSignature {
    float intensity = 1.0f;
    float area = 1.0f;
    bool engine_running = true;
    bool low_profile = false;
};

struct ThermalSensorConfig {
    float sensitivity = 1.0f;
    float base_detection_range = 3000.0f;
    float detection_threshold = 0.25f;
    float engine_off_penalty = 0.3f;
};

struct ThermalSensor {
    ThermalSensorConfig config;
    float last_confidence = 0.0f;
    bool last_detected = false;
};

class ThermalVisionSystem {
public:
    ThermalVisionSystem() = default;

    void update(float dt,
                ecs::EntityManager& em,
                physics_core::PhysicsCore* physics,
                const SpatialGrid& spatial_grid,
                const WorldState& world_state);

    float get_detection_range(const ThermalSignature& signature,
                              const WorldState& world_state) const;

    bool is_detected(const physics_core::Vec3& target_position,
                     const physics_core::Vec3& observer_position,
                     float detection_range) const;
};

} // namespace combat_systems