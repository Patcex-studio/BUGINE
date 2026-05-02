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
#include <cmath>
#include "ecs/entity_manager.h"
#include "ai_core/spatial_grid.h"
#include "physics_core/physics_core.h"

namespace combat_systems {

enum class EWEffect : uint8_t {
    JAMMING = 0,
    SPOOFING,
    DECEPTION
};

struct EWSource {
    float power = 50.0f;
    float range = 3000.0f;
    float bandwidth = 20.0f;
    EWEffect effect = EWEffect::JAMMING;
};

struct EWComponent {
    EWSource source;
    bool active = true;
};

struct EWStatus {
    float link_quality = 1.0f;
    float packet_delay = 0.2f;
    float packet_loss_rate = 0.0f;
    bool is_jammed = false;
    bool gps_suppressed = false;
    bool spoofed = false;
};

class ElectronicWarfareSystem {
public:
    ElectronicWarfareSystem() = default;

    void update(float dt,
                ecs::EntityManager& em,
                physics_core::PhysicsCore* physics,
                const SpatialGrid& spatial_grid,
                uint64_t frame_id);

    void apply_jamming(EntityId source,
                       EntityId target,
                       const EWSource& source_data,
                       EWStatus& status,
                       float distance,
                       uint64_t frame_id) const;

    float get_link_quality(EntityId receiver,
                           EntityId emitter,
                           const EWSource& source,
                           physics_core::PhysicsCore* physics) const;
};

} // namespace combat_systems