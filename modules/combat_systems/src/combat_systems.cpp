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
#include "combat_systems/combat_systems.h"
#include "combat_systems/active_protection.h"
#include "combat_systems/thermal_vision.h"
#include "combat_systems/electronic_warfare.h"
#include "combat_systems/nbc_warfare.h"

namespace combat_systems {

class CombatSystemsImpl {
public:
    CombatSystemsImpl() = default;

    void initialize(ecs::EntityManager& em,
                    physics_core::PhysicsCore* physics,
                    const SpatialGrid& spatial_grid) {
        entity_manager_ = &em;
        physics_core_ = physics;
        spatial_grid_ = &spatial_grid;
    }

    void update(float dt, const WorldState& world_state, uint64_t frame_id) {
        if (!entity_manager_ || !physics_core_) {
            return;
        }

        aps_.update(dt, *entity_manager_, physics_core_, *spatial_grid_, frame_id);
        thermal_.update(dt, *entity_manager_, physics_core_, *spatial_grid_, world_state);
        ew_.update(dt, *entity_manager_, physics_core_, *spatial_grid_, frame_id);
        nbc_.update(dt, *entity_manager_, physics_core_, *spatial_grid_, world_state);
    }

private:
    ecs::EntityManager* entity_manager_ = nullptr;
    physics_core::PhysicsCore* physics_core_ = nullptr;
    const SpatialGrid* spatial_grid_ = nullptr;

    ActiveProtectionSystem aps_;
    ThermalVisionSystem thermal_;
    ElectronicWarfareSystem ew_;
    NBCWarfareSystem nbc_;
};

static CombatSystemsImpl* g_combat_systems = nullptr;

void CombatSystems::initialize(ecs::EntityManager& em,
                               physics_core::PhysicsCore* physics,
                               const SpatialGrid& spatial_grid) {
    if (g_combat_systems) {
        delete g_combat_systems;
    }
    g_combat_systems = new CombatSystemsImpl();
    g_combat_systems->initialize(em, physics, spatial_grid);
}

void CombatSystems::shutdown() {
    if (g_combat_systems) {
        delete g_combat_systems;
        g_combat_systems = nullptr;
    }
}

void CombatSystems::update(float dt, const WorldState& world_state, uint64_t frame_id) {
    if (g_combat_systems) {
        g_combat_systems->update(dt, world_state, frame_id);
    }
}

} // namespace combat_systems