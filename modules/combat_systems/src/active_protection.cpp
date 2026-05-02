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
#include "combat_systems/active_protection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

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

float projectile_base_priority(physics_core::ProjectileType type) {
    switch (type) {
        case physics_core::ProjectileType::ATGM: return 1.0f;
        case physics_core::ProjectileType::HEAT: return 0.85f;
        case physics_core::ProjectileType::APFSDS: return 0.60f;
        case physics_core::ProjectileType::APDS: return 0.55f;
        default: return 0.25f;
    }
}

} // namespace

void ActiveProtectionSystem::update(float dt,
                                    ecs::EntityManager& em,
                                    physics_core::PhysicsCore* physics,
                                    const SpatialGrid& spatial_grid,
                                    uint64_t frame_id) {
    if (!physics) {
        return;
    }

    em.query_components<APSComponent>([&](EntityId vehicle, APSComponent& aps) {
        if (aps.mode == APSMode::OFF) {
            return;
        }

        auto* vehicle_body = physics->get_body(vehicle);
        if (!vehicle_body) {
            return;
        }

        if (aps.reload_timer > 0.0f) {
            aps.reload_timer -= dt;
            if (aps.reload_timer < 0.0f) {
                aps.reload_timer = 0.0f;
            }
        }

        std::vector<ThreatScore> candidates;
        candidates.reserve(16);

        auto nearby = spatial_grid.query_radius(
            static_cast<float>(vehicle_body->position.x),
            static_cast<float>(vehicle_body->position.y),
            aps.config.detection_range,
            [&](EntityId id) {
                return get_entity_xy(physics, id);
            });

        for (EntityId threat_id : nearby) {
            if (threat_id == vehicle) {
                continue;
            }

            auto* proj = em.get_component<ProjectileComponent>(threat_id);
            if (!proj || !proj->active || proj->owner_id == vehicle) {
                continue;
            }

            auto* threat_body = physics->get_body(threat_id);
            if (!threat_body) {
                continue;
            }

            ThreatScore score;
            if (!detect_threat(*proj,
                               threat_body->position,
                               threat_body->velocity,
                               vehicle_body->position,
                               aps.config.detection_range,
                               score)) {
                continue;
            }
            candidates.push_back(score);
        }

        if (candidates.empty()) {
            return;
        }

        std::sort(candidates.begin(), candidates.end(), [](const ThreatScore& a, const ThreatScore& b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.threat_id < b.threat_id;
        });

        aps.active_intercepts = 0;

        for (const ThreatScore& score : candidates) {
            if (aps.active_intercepts >= aps.config.max_threats) {
                break;
            }
            if (aps.reload_timer > 0.0f) {
                break;
            }

            uint64_t seed = static_cast<uint64_t>(vehicle) ^ (static_cast<uint64_t>(score.threat_id) << 16) ^ frame_id;
            std::minstd_rand rng(static_cast<uint32_t>(seed ^ (seed >> 32)));
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float roll = dist(rng);

            if (roll < aps.config.intercept_probability) {
                if (intercept(score.threat_id, vehicle, em, physics)) {
                    aps.active_intercepts += 1;
                    aps.reload_timer = aps.config.reload_time;
                }
            }
        }
    });
}

bool ActiveProtectionSystem::detect_threat(const ProjectileComponent& projectile,
                                           const physics_core::Vec3& projectile_position,
                                           const physics_core::Vec3& projectile_velocity,
                                           const physics_core::Vec3& vehicle_position,
                                           float detection_range,
                                           ThreatScore& out_score) const {
    if (!projectile.active) {
        return false;
    }

    const float speed = static_cast<float>(projectile_velocity.magnitude());
    if (speed < 1e-3f) {
        return false;
    }

    physics_core::Vec3 delta = {
        vehicle_position.x - projectile_position.x,
        vehicle_position.y - projectile_position.y,
        vehicle_position.z - projectile_position.z
    };

    const float distance = static_cast<float>(delta.magnitude());
    if (distance > detection_range || distance < 1.0f) {
        return false;
    }

    const float closing = static_cast<float>(projectile_velocity.dot(delta));
    if (closing >= 0.0f) {
        return false;
    }

    const float time_to_impact = distance / speed;
    if (time_to_impact > 10.0f) {
        return false;
    }

    const float base = projectile_base_priority(projectile.projectile.projectile_type);
    const float time_factor = std::clamp(1.0f - time_to_impact / 8.0f, 0.0f, 1.0f);
    const float dist_factor = 1.0f - std::clamp(distance / detection_range, 0.0f, 1.0f);
    const float priority = base * 0.5f + time_factor * 0.3f + dist_factor * 0.2f;

    if (priority < 0.25f) {
        return false;
    }

    out_score.threat_id = 0;
    out_score.priority = priority;
    out_score.intercept_time = time_to_impact;
    return true;
}

bool ActiveProtectionSystem::intercept(EntityId threat,
                                       EntityId /*vehicle*/,
                                       ecs::EntityManager& em,
                                       physics_core::PhysicsCore* physics) const {
    if (!physics) {
        return false;
    }

    if (auto* projectile = em.get_component<ProjectileComponent>(threat)) {
        projectile->active = false;
    }

    auto* body = physics->get_body(threat);
    if (!body) {
        return false;
    }

    physics->destroy_body(threat);
    return true;
}

} // namespace combat_systems