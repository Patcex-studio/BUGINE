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
#include <functional>
#include <memory>

namespace electrical_system {

// Forward declarations
struct GeneratorComponent;
struct ConsumerComponent;
struct ElectricalGridComponent;

// Entity ID type
using EntityID = uint32_t;

/// Power loss reasons for consumer callbacks
enum class PowerLossReason : uint8_t {
    OVERLOAD = 0,           // System overload - non-critical consumer disconnected
    SHORT_CIRCUIT = 1,      // Short circuit detected
    BATTERY_DEAD = 2,       // Battery fully discharged
    GENERATOR_FAULT = 3,    // Generator malfunction
    MANUAL_SHUTDOWN = 4     // Intentional shutdown
};

/// Damage flags for electrical grid
enum class DamageFlag : uint32_t {
    NONE = 0,
    BATTERY_DEAD = 1 << 0,
    GENERATOR_FAULT = 1 << 1,
    SHORT_CIRCUIT = 1 << 2,
    FIRE_HAZARD = 1 << 3
};

/// Electrical event types for cascading effects
enum class ElectricalEvent : uint8_t {
    POWER_LOST = 0,
    SHORT_CIRCUIT = 1,
    FIRE_START = 2,
    COMPONENT_DAMAGED = 3,
    OVERLOAD = 4
};

/// Event structure for cascade effects
struct ElectricalCascadeEvent {
    EntityID source_entity;      // Where event originated
    EntityID affected_entity;    // What was affected (may be same as source)
    ElectricalEvent event_type;
    float severity;              // 0..1 scale
    uint64_t frame_id;           // For deterministic reproduction
};

/// Consumer priority with deterministic ordering
struct ConsumerPriority {
    uint8_t tier;           // 0 = critical (weapon), 1 = important (comms), 2 = optional (lights)
    uint16_t entity_id;     // For deterministic sorting when tier is equal
    
    bool operator<(const ConsumerPriority& other) const {
        if (tier != other.tier) return tier < other.tier;
        return entity_id < other.entity_id;
    }
};

/// Temporary entry for consumer processing during frame
struct ConsumerEntry {
    EntityID entity_id;
    float required_power_w;
    float priority;
    uint8_t is_active;
    uint8_t is_damaged;
};

/// Network state for serialization (compact representation)
struct NetworkElectricalState {
    EntityID grid_id;
    uint16_t battery_charge_q;    // Quantized: 0..1023 -> charge = x/1023.0f * max_charge
    uint8_t damage_flags;         // First 8 bits of damage flags
    uint8_t active_consumers_mask; // Bitmask: which consumers are active (max 8)
    
    bool needs_update(const NetworkElectricalState& baseline) const {
        return battery_charge_q != baseline.battery_charge_q ||
               damage_flags != baseline.damage_flags ||
               active_consumers_mask != baseline.active_consumers_mask;
    }
};

/// Deterministic RNG for reproducible cascade effects
class DeterministicRNG {
private:
    uint64_t state_;
    
    static constexpr uint64_t multiplier = 6364136223846793005ULL;
    static constexpr uint64_t increment = 1442695040888963407ULL;
    
public:
    explicit DeterministicRNG(uint64_t seed = 0) : state_(seed) {}
    
    static uint64_t make_seed(EntityID entity_id, uint64_t frame_id) {
        return (static_cast<uint64_t>(entity_id) << 32) | (frame_id & 0xFFFFFFFFULL);
    }
    
    uint32_t next() {
        state_ = state_ * multiplier + increment;
        return static_cast<uint32_t>(state_ >> 32);
    }
    
    float next_float() {
        return (next() >> 8) * (1.0f / 16777216.0f);
    }
    
    float next_float_range(float min, float max) {
        return min + next_float() * (max - min);
    }
};

} // namespace electrical_system
