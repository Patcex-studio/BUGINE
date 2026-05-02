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

#include <span>
#include <vector>
#include <array>
#include <optional>
#include <unordered_map>
#include "ai_core/perception_component.h"
#include "ai_core/decision_engine.h"

// Forward declarations
struct SpatialGrid;
struct WorldState;
struct AIConfig;
struct Modifier;

/**
 * Lazy LOS System with hash cache and batch processing
 */
class LazyLOSSystem {
public:
    // LOS cache entry
    struct LOSCache {
        uint64_t key;  // hash(entity_pos, target_pos, frame_id)
        bool visible;
        uint64_t valid_until_frame;
    };

    // Configuration for LOS
    struct Config {
        float move_threshold = 0.5f;      // min. displacement for update
        float stale_threshold_frames = 120; // 2 sec at 60 FPS
        float extrapolation_speed = 1.5f; // m/s for position prediction
        uint64_t cache_ttl_frames = 60;   // cache validity duration
    };

    LazyLOSSystem() = default;

    // Batch check LOS: returns span of results (zero allocations)
    std::span<const LOSResult> CheckBatch(Vector3 observer_pos,
                                          EntityId observer_id,
                                          std::span<const ThreatRecord> threats,
                                          const WorldState& world,
                                          uint64_t frame_id,
                                          const Config& cfg);

private:
    // Fixed cache pool
    mutable std::array<LOSCache, 256> cache_;
    mutable uint8_t cache_index_ = 0;

    // Temporary results buffer (reused)
    mutable std::vector<LOSResult> results_buffer_;

    // Fast hash for cache
    static uint64_t hash_los_key(Vector3 observer_pos, Vector3 target_pos, uint64_t frame) noexcept;

    // Check cache before expensive raycast
    std::optional<bool> check_cache(Vector3 obs_pos, Vector3 tgt_pos, uint64_t frame) const;

    // Write to cache (LRU-style, no allocations)
    void cache_result(Vector3 obs_pos, Vector3 tgt_pos, bool visible, uint64_t frame, uint64_t ttl_frames);

    // Perform actual raycast (placeholder - integrate with occlusion system)
    bool perform_raycast(Vector3 from, Vector3 to, EntityId ignore_entity, const WorldState& world) const;
};

/**
 * Memory Decay system with deterministic decay and hysteresis
 */
class MemoryDecay {
public:
    struct Config {
        float base_decay_rate = 0.15f;    // per frame at 60 FPS
        float stress_retention_mult = 0.5f; // stress slows forgetting
        float removal_threshold = 0.15f;   // threshold for removal
        float hysteresis_window = 0.05f;   // buffer around threshold
    };

    // Batch update with deterministic order
    void UpdateBatch(std::span<ThreatRecord> threats, uint8_t& count,
                     float stress, uint64_t current_frame, const Config& cfg);

    // Preemptive decay check (if buffer is almost full)
    bool ShouldPreemptiveDecay(uint8_t count, uint8_t max_count, float urgency) noexcept;
};

/**
 * Main Perception System
 */
class PerceptionSystem {
public:
    PerceptionSystem() = default;

    // Update perception modifiers (batch, zero allocations)
    void UpdateModifiersBatch(std::span<PerceptionState<16>> perceptions,
                              const WorldState& world,
                              const std::span<const float> unit_stress,
                              const std::span<const float> unit_fatigue,
                              const Modifier& global_mod);

    // Main update function
    void Update(const UnitSoA& units, SpatialGrid& spatial_grid,
                const WorldState& world, uint64_t frame_id,
                const AIConfig& cfg);

    // Fast O(1) unit position lookup by EntityId
    [[nodiscard]] std::optional<Vector3> GetPosition(EntityId id) const noexcept;

private:
    void BuildEntityIndex(const UnitSoA& units) noexcept;
    std::optional<uint32_t> FindUnitIndex(EntityId id) const noexcept;

    const UnitSoA* units_ = nullptr;
    std::vector<uint32_t> entity_to_index_;
    std::unordered_map<EntityId, uint32_t> entity_to_index_map_;
    bool use_sparse_map_ = false;

private:
    LazyLOSSystem los_system_;
    MemoryDecay memory_decay_;

    // Configs
    LazyLOSSystem::Config los_cfg_;
    MemoryDecay::Config decay_cfg_;

    // Temporary buffers
    std::vector<EntityId> observer_ids_;
    std::vector<ThreatRecord> threat_candidates_;
};

