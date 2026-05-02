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
#include <array>
#include <algorithm>
#include <span>
#include <optional>
#include <glm/vec3.hpp>

using EntityId = uint64_t;
using Vector3 = glm::vec3;

// ============================================================================
// Perception Component Types
// ============================================================================

/**
 * Threat record for perception system
 */
struct ThreatRecord {
    EntityId id;
    enum class Type : uint8_t { Visual, Sound, Memory } type;
    float confidence;           // 0..1
    Vector3 last_known_pos;
    uint64_t last_seen_frame;   // instead of float time — determinism
    uint64_t last_heard_frame;
    uint16_t los_hash;          // hash of last LOS result (for cache)

    // Default constructor
    ThreatRecord() = default;

    // Constructor
    ThreatRecord(EntityId id_, Type type_, float confidence_, Vector3 pos_,
                 uint64_t seen_frame, uint64_t heard_frame, uint16_t hash_)
        : id(id_), type(type_), confidence(confidence_), last_known_pos(pos_),
          last_seen_frame(seen_frame), last_heard_frame(heard_frame), los_hash(hash_) {}
};

// Ensure ThreatRecord is trivially copyable for performance
static_assert(std::is_trivially_copyable_v<ThreatRecord>, "ThreatRecord must be trivially copyable");

/**
 * Fixed-size perception state with zero allocations in hot path
 */
template<size_t MaxThreats = 16>
struct PerceptionState {
    std::array<ThreatRecord, MaxThreats> threats;
    uint8_t threat_count;  // actual number of threats

    uint64_t last_los_update_frame;
    Vector3 last_los_position;

    // Computed modifiers (not stored, recalculated each frame)
    float effective_range;
    float peripheral_factor;
    float hearing_range;
    float false_positive_chance;

    // Base ranges (from AIConfig)
    float visual_range;
    float hearing_range_base;

    // Constructor
    PerceptionState(float visual_range_ = 50.0f, float hearing_range_ = 30.0f)
        : threat_count(0), last_los_update_frame(0), last_los_position(0,0,0),
          effective_range(visual_range_), peripheral_factor(1.0f),
          hearing_range(hearing_range_), false_positive_chance(0.0f),
          visual_range(visual_range_), hearing_range_base(hearing_range_) {}

    // Find threat by id (O(N) linear search, but N=16 is constant)
    ThreatRecord* find(EntityId id) noexcept {
        for (uint8_t i = 0; i < threat_count; ++i) {
            if (threats[i].id == id) return &threats[i];
        }
        return nullptr;
    }

    // Add or update threat with eviction of least important
    ThreatRecord* add_or_update(EntityId id, ThreatRecord::Type type,
                                float confidence, Vector3 pos, uint64_t frame) {
        if (auto* existing = find(id)) {
            existing->confidence = std::min(1.0f, existing->confidence + confidence * 0.2f);
            existing->last_known_pos = pos;
            existing->last_seen_frame = (type == ThreatRecord::Type::Visual) ? frame : existing->last_seen_frame;
            existing->last_heard_frame = (type == ThreatRecord::Type::Sound) ? frame : existing->last_heard_frame;
            return existing;
        }
        if (threat_count < MaxThreats) {
            auto& slot = threats[threat_count++];
            slot = {id, type, confidence, pos, frame, frame, 0};
            return &slot;
        }
        // Evict threat with lowest confidence
        auto* weakest = std::min_element(threats.begin(), threats.begin() + threat_count,
            [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
        if (weakest->confidence < 0.3f) {  // only if weak
            *weakest = {id, type, confidence, pos, frame, frame, 0};
            return weakest;
        }
        return nullptr;  // buffer full, no room for new threat
    }

    // Remove threat by index (used in decay)
    void remove_at(uint8_t index) {
        if (index < threat_count) {
            threats[index] = threats[--threat_count];
        }
    }

    // Get span of active threats
    std::span<ThreatRecord> active_threats() {
        return {threats.data(), threat_count};
    }
};

/**
 * LOS result for batch processing
 */
struct LOSResult {
    EntityId target;
    bool visible;
    float confidence;
};

/**
 * Deterministic RNG for perception sampling
 */
class PerceptionDeterministicRNG {
    uint64_t state;
public:
    explicit PerceptionDeterministicRNG(uint64_t seed) : state(seed) {}

    float next_float() noexcept {
        // LCG with fixed parameters (deterministic)
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>(state >> 32) / 4294967296.0f;
    }

    // Seed = frame_id ^ entity_id for reproducibility
    static uint64_t make_seed(uint64_t frame_id, EntityId entity_id) {
        return frame_id ^ (static_cast<uint64_t>(entity_id) << 16);
    }
};
