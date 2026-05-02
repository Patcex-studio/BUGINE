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
#include <vector>
#include "ecs/entity_manager.h"
#include "ai_core/spatial_grid.h"
#include "physics_core/physics_core.h"
#include "physics_core/projectile_properties.h"

namespace combat_systems {

enum class APSMode : uint8_t {
    OFF = 0,
    STANDBY,
    ACTIVE,
    ENGAGING
};

struct APSConfig {
    float detection_range = 2000.0f;      // meters
    float intercept_probability = 0.9f;   // base success probability
    float reload_time = 0.4f;             // seconds
    uint8_t max_threats = 2;
};

struct APSComponent {
    APSMode mode = APSMode::STANDBY;
    APSConfig config;
    float reload_timer = 0.0f;
    uint8_t active_intercepts = 0;
};

struct ProjectileComponent {
    physics_core::ProjectileProperties projectile;
    EntityId owner_id = 0;
    bool active = true;
};

struct ThreatScore {
    EntityId threat_id = 0;
    float priority = 0.0f;
    float intercept_time = 0.0f;
};

class ActiveProtectionSystem {
public:
    ActiveProtectionSystem() = default;

    void update(float dt,
                ecs::EntityManager& em,
                physics_core::PhysicsCore* physics,
                const SpatialGrid& spatial_grid,
                uint64_t frame_id);

    bool detect_threat(const ProjectileComponent& projectile,
                       const physics_core::Vec3& projectile_position,
                       const physics_core::Vec3& projectile_velocity,
                       const physics_core::Vec3& vehicle_position,
                       float detection_range,
                       ThreatScore& out_score) const;

    bool intercept(EntityId threat,
                   EntityId vehicle,
                   ecs::EntityManager& em,
                   physics_core::PhysicsCore* physics) const;
};

} // namespace combat_systems