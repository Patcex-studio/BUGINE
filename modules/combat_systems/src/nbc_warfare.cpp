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
#include "combat_systems/nbc_warfare.h"

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

} // namespace

void NBCWarfareSystem::update(float dt,
                              ecs::EntityManager& em,
                              physics_core::PhysicsCore* physics,
                              const SpatialGrid& spatial_grid,
                              const WorldState& world_state) {
    for (auto& zone : active_zones_) {
        zone.duration -= dt;
        zone.decay_rate = std::clamp(zone.decay_rate, 0.01f, 1.0f);
        zone.intensity *= std::exp(-zone.decay_rate * dt);
    }

    active_zones_.erase(
        std::remove_if(active_zones_.begin(), active_zones_.end(),
                      [](const NBCZone& z) { return z.duration < 0.0f || z.intensity < 0.01f; }),
        active_zones_.end());

    if (!physics) {
        return;
    }

    em.query_components<NBCExposure>([&](EntityId unit, NBCExposure& exposure) {
        auto* unit_body = physics->get_body(unit);
        if (!unit_body) {
            return;
        }

        const physics_core::Vec3& unit_pos = unit_body->position;
        auto* protection = em.get_component<NBCProtection>(unit);

        float total_damage = 0.0f;

        for (const NBCZone& zone : active_zones_) {
            const float dist = static_cast<float>((unit_pos - zone.center).magnitude());
            if (dist > zone.radius) {
                continue;
            }

            float damage_rate = get_damage_rate(unit, zone, unit_pos);
            if (protection) {
                damage_rate = apply_protection(*protection, damage_rate);
            }
            total_damage += damage_rate * dt;
        }

        exposure.cumulative_dose += total_damage;
        exposure.last_damage_rate = total_damage / dt;
    });
}

void NBCWarfareSystem::create_zone(const NBCZone& zone) {
    active_zones_.push_back(zone);
}

float NBCWarfareSystem::get_damage_rate(EntityId /*unit*/,
                                        const NBCZone& zone,
                                        const physics_core::Vec3& unit_position) const {
    const float dist = static_cast<float>((unit_position - zone.center).magnitude());
    if (dist > zone.radius) {
        return 0.0f;
    }

    const float profile = std::exp(-2.0f * dist * dist / (zone.radius * zone.radius));
    return zone.intensity * profile;
}

float NBCWarfareSystem::apply_protection(const NBCProtection& protection,
                                         float raw_dose) const {
    float factor = 1.0f;
    if (protection.gas_mask) {
        factor *= 0.05f;
    }
    if (protection.protective_suit) {
        factor *= 0.10f;
    }
    if (protection.vehicle_filtration) {
        factor *= 0.20f;
    }
    return raw_dose * std::clamp(factor, 0.001f, 1.0f);
}

} // namespace combat_systems