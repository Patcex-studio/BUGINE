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
#include "combat_systems/electronic_warfare.h"

#include <cmath>
#include <algorithm>

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

float snr_from_distance(float source_power, float distance, float bandwidth) {
    if (distance < 1.0f) {
        distance = 1.0f;
    }

    float path_loss = 20.0f * std::log10(distance) + 20.0f * std::log10(bandwidth / 1e6f);
    float received_power = 10.0f * std::log10(source_power) - path_loss;
    float noise_floor = -120.0f;
    return std::clamp(received_power - noise_floor, -20.0f, 40.0f);
}

} // namespace

void ElectronicWarfareSystem::update(float dt,
                                      ecs::EntityManager& em,
                                      physics_core::PhysicsCore* physics,
                                      const SpatialGrid& spatial_grid,
                                      uint64_t frame_id) {
    if (!physics) {
        return;
    }

    em.query_components<EWComponent>([&](EntityId emitter, EWComponent& ew) {
        if (!ew.active) {
            return;
        }

        auto* emitter_body = physics->get_body(emitter);
        if (!emitter_body) {
            return;
        }

        auto nearby = spatial_grid.query_radius(
            static_cast<float>(emitter_body->position.x),
            static_cast<float>(emitter_body->position.y),
            ew.source.range * 1.2f,
            [&](EntityId id) {
                return get_entity_xy(physics, id);
            });

        for (EntityId receiver : nearby) {
            if (receiver == emitter) {
                continue;
            }

            auto* receiver_body = physics->get_body(receiver);
            if (!receiver_body) {
                continue;
            }

            const float distance = static_cast<float>((receiver_body->position - emitter_body->position).magnitude());
            if (distance > ew.source.range) {
                continue;
            }

            if (auto* ew_status = em.get_component<EWStatus>(receiver)) {
                apply_jamming(emitter, receiver, ew.source, *ew_status, distance, frame_id);
            }
        }
    });
}

void ElectronicWarfareSystem::apply_jamming(EntityId source,
                                             EntityId target,
                                             const EWSource& source_data,
                                             EWStatus& status,
                                             float distance,
                                             uint64_t frame_id) const {
    const float snr = snr_from_distance(source_data.power, distance, source_data.bandwidth);
    status.link_quality = std::clamp(snr / 15.0f, 0.0f, 1.0f);

    if (snr < 0.0f) {
        status.is_jammed = true;
        status.packet_loss_rate = std::clamp(1.0f - status.link_quality, 0.0f, 0.95f);
        status.packet_delay = 0.2f + (1.0f - status.link_quality) * 1.3f;
    } else {
        status.is_jammed = false;
        status.packet_loss_rate = 0.0f;
        status.packet_delay = 0.2f;
    }

    if (source_data.effect == EWEffect::SPOOFING && snr > 5.0f) {
        status.spoofed = true;
    }
    if (source_data.effect == EWEffect::JAMMING && distance < source_data.range * 0.5f) {
        status.gps_suppressed = true;
    }
}

float ElectronicWarfareSystem::get_link_quality(EntityId receiver,
                                                 EntityId emitter,
                                                 const EWSource& source,
                                                 physics_core::PhysicsCore* physics) const {
    if (!physics) {
        return 1.0f;
    }

    auto* receiver_body = physics->get_body(receiver);
    auto* emitter_body = physics->get_body(emitter);
    if (!receiver_body || !emitter_body) {
        return 1.0f;
    }

    const float distance = static_cast<float>((receiver_body->position - emitter_body->position).magnitude());
    const float snr = snr_from_distance(source.power, distance, source.bandwidth);
    return std::clamp(snr / 15.0f, 0.0f, 1.0f);
}

} // namespace combat_systems