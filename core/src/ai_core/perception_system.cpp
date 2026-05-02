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
#include "ai_core/perception_system.h"
#include "ai_core/spatial_grid.h"
#include "ai_core/ai_config.h"
#include "ai_core/dynamic_modifiers.h"
#include "physics_core/collision_system.h"
#include "physics_core/types.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <glm/glm.hpp>

namespace {

inline EntityId GetEntityId(const UnitSoA& units, size_t index) noexcept {
    return units.GetEntityId(index);
}

inline std::pair<float, float> MakePositionPair(const std::optional<Vector3>& pos) noexcept {
    return pos ? std::pair<float, float>{pos->x, pos->y} : std::pair<float, float>{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
}

} // namespace

// ============================================================================
// LazyLOSSystem Implementation
// ============================================================================

uint64_t LazyLOSSystem::hash_los_key(Vector3 observer_pos, Vector3 target_pos, uint64_t frame) noexcept {
    // Quantize positions to 0.1m + pack
    const auto qx = static_cast<int32_t>(observer_pos.x * 10.0f);
    const auto qy = static_cast<int32_t>(observer_pos.y * 10.0f);
    const auto qtx = static_cast<int32_t>(target_pos.x * 10.0f);
    const auto qty = static_cast<int32_t>(target_pos.y * 10.0f);
    // Simple hash: combine quantized coords and frame
    uint64_t hash = static_cast<uint64_t>(qx) ^ (static_cast<uint64_t>(qy) << 16) ^
                   (static_cast<uint64_t>(qtx) << 32) ^ (static_cast<uint64_t>(qty) << 48);
    return hash ^ frame;
}

std::optional<bool> LazyLOSSystem::check_cache(Vector3 obs_pos, Vector3 tgt_pos, uint64_t frame) const {
    auto key = hash_los_key(obs_pos, tgt_pos, frame);
    for (const auto& entry : cache_) {
        if (entry.key == key && entry.valid_until_frame >= frame) {
            return entry.visible;
        }
    }
    return std::nullopt;
}

void LazyLOSSystem::cache_result(Vector3 obs_pos, Vector3 tgt_pos, bool visible, uint64_t frame, uint64_t ttl_frames) {
    auto& slot = cache_[cache_index_++ % cache_.size()];
    slot.key = hash_los_key(obs_pos, tgt_pos, frame);
    slot.visible = visible;
    slot.valid_until_frame = frame + ttl_frames;
}

bool LazyLOSSystem::perform_raycast(Vector3 from, Vector3 to, EntityId ignore_entity, const WorldState& world) const {
    if (!world.collision_system) return true;  // Fallback if no collision system

    // Add eye height offset
    from.z += 1.6f;  // Eye height

    physics_core::Vec3 origin{from.x, from.y, from.z};
    physics_core::Vec3 target{to.x, to.y, to.z};
    physics_core::Vec3 direction = target - origin;
    float distance = glm::length(glm::vec3(static_cast<float>(direction.x), static_cast<float>(direction.y), static_cast<float>(direction.z)));
    if (distance == 0.0f) return true;  // Same position
    direction = direction / static_cast<double>(distance);  // Normalize

    physics_core::EntityID hit_entity = 0;
    physics_core::Vec3 hit_point;
    bool hit = world.collision_system->raycast_simd(origin, direction, distance, hit_entity, hit_point);

    // Visible if no hit or hit the target entity
    return !hit || hit_entity == static_cast<physics_core::EntityID>(ignore_entity);  // TODO: proper ignore
}

std::span<const LOSResult> LazyLOSSystem::CheckBatch(Vector3 observer_pos,
                                                     EntityId observer_id,
                                                     std::span<const ThreatRecord> threats,
                                                     const WorldState& world,
                                                     uint64_t frame_id,
                                                     const Config& cfg) {
    results_buffer_.clear();
    results_buffer_.reserve(threats.size());

    for (const auto& threat : threats) {
        const Vector3 tgt_pos = threat.last_known_pos;

        // Check cached LOS result first.
        if (auto cached = check_cache(observer_pos, tgt_pos, frame_id)) {
            results_buffer_.push_back({threat.id, *cached, threat.confidence});
            continue;
        }

        const bool needs_update = (frame_id - threat.last_seen_frame > cfg.stale_threshold_frames);
        if (needs_update) {
            const bool visible = perform_raycast(observer_pos, tgt_pos, observer_id, world);
            cache_result(observer_pos, tgt_pos, visible, frame_id, cfg.cache_ttl_frames);
            results_buffer_.push_back({threat.id, visible, threat.confidence});
        } else {
            results_buffer_.push_back({threat.id, true, threat.confidence * 0.9f});
        }
    }

    return results_buffer_;
}

// ============================================================================
// MemoryDecay Implementation
// ============================================================================

void MemoryDecay::UpdateBatch(std::span<ThreatRecord> threats, uint8_t& count,
                              float stress, uint64_t current_frame, const Config& cfg) {
    // Process from end to remove without shifting array
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        auto& threat = threats[i];

        // If threat confirmed this frame — don't decay
        if (threat.last_seen_frame == current_frame ||
            threat.last_heard_frame == current_frame) {
            continue;
        }

        // Exponential decay with stress retention
        const float decay = cfg.base_decay_rate * (1.0f - stress * cfg.stress_retention_mult);
        threat.confidence *= std::exp(-decay);

        // Hysteresis removal
        if (threat.confidence < cfg.removal_threshold - cfg.hysteresis_window) {
            // Remove: swap with last
            threats[i] = threats[--count];
        }
    }
}

bool MemoryDecay::ShouldPreemptiveDecay(uint8_t count, uint8_t max_count, float urgency) noexcept {
    return count > max_count * 0.9f && urgency > 0.7f;
}

// ============================================================================
// PerceptionSystem Implementation
// ============================================================================

namespace Perception {

// constexpr functions for zero-overhead computations
constexpr float clamp_range(float value, float min_frac = 0.2f, float max_frac = 1.0f) noexcept {
    return std::max(min_frac, std::min(max_frac, value));
}

constexpr float compute_effective_range(float base_range, float world_mult,
                                        float stress, float fatigue) noexcept {
    const float mental_mult = 1.0f - stress * 0.3f - fatigue * 0.1f;
    return base_range * world_mult * clamp_range(mental_mult);
}

constexpr float compute_peripheral_factor(float stress, float fatigue) noexcept {
    const float factor = 1.0f - stress * 0.5f - fatigue * 0.2f;
    return clamp_range(factor, 0.1f);
}

constexpr float compute_hearing_range(float base_range, float precipitation,
                                      float wind_speed, float fatigue) noexcept {
    const float env_mult = 1.0f - precipitation * 0.4f - wind_speed * 0.05f;
    const float mental_mult = 1.0f - fatigue * 0.15f;
    return base_range * clamp_range(env_mult * mental_mult);
}

constexpr float compute_false_positive_chance(float stress, float fatigue,
                                              float fog_density) noexcept {
    return clamp_range(stress * 0.1f + fatigue * 0.05f + fog_density * 0.2f, 0.0f, 0.3f);
}

// Batch update modifiers (cache locality)
inline void UpdateModifiersBatch(std::span<PerceptionState<16>> perceptions,
                                 const WorldState& world,
                                 const std::span<const float> unit_stress,
                                 const std::span<const float> unit_fatigue,
                                 const Modifier& global_mod) {
    for (size_t i = 0; i < perceptions.size(); ++i) {
        auto& p = perceptions[i];
        const float stress = unit_stress[i];
        const float fatigue = unit_fatigue[i];

        p.effective_range = compute_effective_range(p.visual_range, global_mod.perception_range_mult, stress, fatigue);
        p.peripheral_factor = compute_peripheral_factor(stress, fatigue);
        p.hearing_range = compute_hearing_range(p.hearing_range_base, world.precipitation, world.wind_speed, fatigue);
        p.false_positive_chance = compute_false_positive_chance(stress, fatigue, world.fog_density);
    }
}

} // namespace Perception

void PerceptionSystem::BuildEntityIndex(const UnitSoA& units) noexcept {
    units_ = &units;
    const size_t count = units.positions_x.size();
    if (count == 0) {
        entity_to_index_.clear();
        entity_to_index_map_.clear();
        use_sparse_map_ = false;
        return;
    }

    EntityId max_id = 0;
    for (size_t i = 0; i < count; ++i) {
        max_id = std::max(max_id, GetEntityId(units, i));
    }

    constexpr uint64_t kMaxDirectIds = (1u << 24);
    if (max_id <= kMaxDirectIds && max_id <= UINT32_MAX) {
        use_sparse_map_ = false;
        entity_to_index_.assign(static_cast<size_t>(max_id) + 1, UINT32_MAX);
        for (size_t i = 0; i < count; ++i) {
            const EntityId id = GetEntityId(units, i);
            entity_to_index_[static_cast<size_t>(id)] = static_cast<uint32_t>(i);
        }
        entity_to_index_map_.clear();
    } else {
        use_sparse_map_ = true;
        entity_to_index_map_.clear();
        entity_to_index_map_.reserve(count * 2);
        for (size_t i = 0; i < count; ++i) {
            entity_to_index_map_[GetEntityId(units, i)] = static_cast<uint32_t>(i);
        }
        entity_to_index_.clear();
    }
}

std::optional<uint32_t> PerceptionSystem::FindUnitIndex(EntityId id) const noexcept {
    if (!units_) {
        return std::nullopt;
    }

    if (!use_sparse_map_) {
        if (id < entity_to_index_.size()) {
            const uint32_t idx = entity_to_index_[static_cast<size_t>(id)];
            if (idx != UINT32_MAX) {
                return idx;
            }
        }
        return std::nullopt;
    }

    const auto it = entity_to_index_map_.find(id);
    return it != entity_to_index_map_.end() ? std::optional<uint32_t>(it->second) : std::nullopt;
}

std::optional<Vector3> PerceptionSystem::GetPosition(EntityId id) const noexcept {
    const auto index = FindUnitIndex(id);
    if (!index.has_value() || !units_) {
        return std::nullopt;
    }
    const uint32_t idx = *index;
    return Vector3{units_->positions_x[idx], units_->positions_y[idx], units_->positions_z[idx]};
}

void PerceptionSystem::UpdateModifiersBatch(std::span<PerceptionState<16>> perceptions,
                                           const WorldState& world,
                                           const std::span<const float> unit_stress,
                                           const std::span<const float> unit_fatigue,
                                           const Modifier& global_mod) {
    Perception::UpdateModifiersBatch(perceptions, world, unit_stress, unit_fatigue, global_mod);
}

void PerceptionSystem::Update(const UnitSoA& units, SpatialGrid& spatial_grid,
                              const WorldState& world, uint64_t frame_id,
                              const AIConfig& cfg) {
    // 1. Update modifiers (batch, zero allocations)
    UpdateModifiersBatch(units.perception_states, world, units.stress, units.fatigue, Modifier());

    // 2. Build fast EntityId -> index mapping
    BuildEntityIndex(units);

    // 3. Update SpatialGrid with actual EntityIds
    spatial_grid.clear();
    for (size_t i = 0; i < units.count(); ++i) {
        const EntityId id = GetEntityId(units, i);
        spatial_grid.insert(id, units.positions_x[i], units.positions_y[i]);
    }

    // 4. Process units with TickBudget and priorities
    for (size_t i = 0; i < units.perception_states.size(); ++i) {
        auto& perception = units.perception_states[i];
        const Vector3 observer_pos{units.positions_x[i], units.positions_y[i], units.positions_z[i]};
        const EntityId observer_id = GetEntityId(units, i);

        const auto candidates = spatial_grid.query_radius(
            observer_pos.x, observer_pos.y, perception.effective_range,
            [this](EntityId id) -> std::pair<float, float> {
                return MakePositionPair(GetPosition(id));
            });

        threat_candidates_.clear();
        threat_candidates_.reserve(candidates.size());
        for (EntityId target_id : candidates) {
            if (target_id == observer_id) {
                continue;
            }
            if (auto target_pos = GetPosition(target_id)) {
                threat_candidates_.push_back({target_id,
                                              ThreatRecord::Type::Visual,
                                              0.5f,
                                              *target_pos,
                                              frame_id,
                                              frame_id,
                                              0});
            }
        }

        const auto los_results = los_system_.CheckBatch(observer_pos,
                                                        observer_id,
                                                        threat_candidates_,
                                                        world,
                                                        frame_id,
                                                        los_cfg_);

        for (const auto& hit : los_results) {
            if (hit.visible) {
                if (auto target_pos = GetPosition(hit.target)) {
                    perception.add_or_update(hit.target,
                                             ThreatRecord::Type::Visual,
                                             hit.confidence,
                                             *target_pos,
                                             frame_id);
                }
            }
        }

        uint8_t threat_count = perception.threat_count;
        memory_decay_.UpdateBatch(perception.active_threats(), threat_count, units.stress[i], frame_id, decay_cfg_);
        perception.threat_count = threat_count;
    }
}