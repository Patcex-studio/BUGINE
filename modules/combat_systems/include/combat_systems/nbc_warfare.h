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
#include <cmath>
#include "ecs/entity_manager.h"
#include "ai_core/spatial_grid.h"
#include "ai_core/dynamic_modifiers.h"
#include "physics_core/physics_core.h"

namespace combat_systems {

enum class NBCType : uint8_t {
    NUCLEAR = 0,
    BIOLOGICAL,
    CHEMICAL
};

struct NBCZone {
    physics_core::Vec3 center;
    float radius = 100.0f;
    float intensity = 1.0f;
    float duration = 600.0f;
    NBCType type = NBCType::CHEMICAL;
    float spread_rate = 5.0f;
    physics_core::Vec3 wind = {};
    float decay_rate = 0.1f;
};

struct NBCProtection {
    bool gas_mask = false;
    bool protective_suit = false;
    bool vehicle_filtration = false;
};

struct NBCExposure {
    float cumulative_dose = 0.0f;
    float last_damage_rate = 0.0f;
};

class NBCWarfareSystem {
public:
    NBCWarfareSystem() = default;

    void update(float dt,
                ecs::EntityManager& em,
                physics_core::PhysicsCore* physics,
                const SpatialGrid& spatial_grid,
                const WorldState& world_state);

    void create_zone(const NBCZone& zone);

    float get_damage_rate(EntityId unit,
                          const NBCZone& zone,
                          const physics_core::Vec3& unit_position) const;

    float apply_protection(const NBCProtection& protection,
                           float raw_dose) const;

private:
    std::vector<NBCZone> active_zones_;
};

} // namespace combat_systems