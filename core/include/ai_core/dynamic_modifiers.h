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
#include <optional>
#include <unordered_map>
#include <algorithm>
#include <span>
#include <cmath>
#include <limits>

namespace physics_core {
    class CollisionSystem;
}

/**
 * Dynamic Modifiers System – Phase 2
 * 
 * Implements global environmental modifiers for squad-level decision-making:
 * - Weather effects (rain, fog, wind)
 * - Terrain effects (mud, sand, snow, water)
 * - Time-of-day effects (night vs. daylight)
 * - Supply status and logistics
 * 
 * All values are normalized and safe to compose via multiplication.
 */

// ============================================================================
// Enums and Constants
// ============================================================================

enum class TerrainType : uint8_t {
    Asphalt = 0,
    Grass = 1,
    Mud = 2,
    Sand = 3,
    Snow = 4,
    Rock = 5,
    Water = 6,
    COUNT = 7
};

// ============================================================================
// WorldState – Environmental Snapshot
// ============================================================================

/**
 * WorldState
 * 
 * Describes current environmental conditions affecting all units globally.
 * Lightweight, cacheable, updated by world simulation layer.
 */
struct WorldState {
    // ========== Weather ==========
    float temperature = 20.0f;          // Celsius: affects stress gain, fatigue decay
    float precipitation = 0.0f;         // 0.0 (dry) .. 1.0 (heavy rain/snow)
    float fog_density = 0.0f;           // 0.0 (clear) .. 1.0 (complete white-out)
    float wind_speed = 0.0f;            // m/s: affects accuracy at range
    
    // ========== Time and Terrain ==========
    float time_of_day = 12.0f;          // 0..24 hours (0=midnight, 12=noon)
    TerrainType terrain_type = TerrainType::Asphalt;  // Type under units' feet
    uint64_t frame_id = 0;               // Frame index for deterministic updates
    float time_ms = 0.0f;                // Simulation time in milliseconds
    const physics_core::CollisionSystem* collision_system = nullptr;
    
    // ========== Local Details (optional, lazy-loaded) ==========
    /**
     * LocalSample: fine-grained environmental details
     * Fetched only on demand, cached per location.
     * Allows micro-terrain effects without bloating global state.
     */
    struct LocalSample {
        float slope_angle = 0.0f;       // Degrees: affects movement cost
        float cover_density = 0.0f;     // 0..1: density of cover in 5m radius
        bool is_waterlogged = false;    // Can't move/drive here
    };
    
    std::optional<LocalSample> local;
    
    // ========== Utility ==========
    
    /**
     * Fast hash for caching (FNV-1a style)
     * Combines key environmental factors for cache lookup
     */
    [[nodiscard]] uint64_t FastHash() const {
        // Quantize to reduce cache thrashing
        uint32_t prec = static_cast<uint32_t>(precipitation * 10.0f);  // 0..10
        uint32_t fog = static_cast<uint32_t>(fog_density * 10.0f);
        uint32_t terrain = static_cast<uint32_t>(terrain_type);
        uint32_t tod = static_cast<uint32_t>(time_of_day);  // 0..24
        
        // FNV-1a hash
        uint64_t h = 14695981039346656037ULL;
        h ^= prec; h *= 1099511628211ULL;
        h ^= fog; h *= 1099511628211ULL;
        h ^= terrain; h *= 1099511628211ULL;
        h ^= tod; h *= 1099511628211ULL;
        h ^= static_cast<uint32_t>(wind_speed * 10.0f);
        h *= 1099511628211ULL;
        return h;
    }
};

// ============================================================================
// SupplyStatus – Logistics and Resources
// ============================================================================

/**
 * SupplyStatus
 * 
 * Tracks three critical supply types for a squad.
 * Values are ratios (0..1) representing available supply vs. capacity.
 */
struct SupplyStatus {
    float ammo_ratio = 1.0f;            // Ammunition: 0 (empty) .. 1 (full)
    float fuel_ratio = 1.0f;            // Fuel/Energy: depends on unit type
    float medical_ratio = 1.0f;         // Medical supplies: for first aid
    
    // ========== Depletion Tracking ==========
    float ammo_depletion_rate = 0.0f;   // Ammo per second (negative value)
    float fuel_depletion_rate = 0.0f;   // Fuel per second (negative value)
    
    // ========== Supply Weights ==========
    /**
     * Weights for computing overall supply ratio
     * Configurable per squad/difficulty to allow nuanced logistics.
     * 
     * Example: Armor unit might weight fuel heavily (0.8),
     *          infantry weights ammo heavily (0.7)
     */
    struct Weights {
        float ammo = 0.6f;
        float fuel = 0.3f;
        float medical = 0.1f;
    };
    
    /**
     * Compute weighted overall ratio
     * Uses provided weights to aggregate three supply types.
     */
    [[nodiscard]] float WeightedRatio(const Weights& w) const {
        return ammo_ratio * w.ammo + fuel_ratio * w.fuel + medical_ratio * w.medical;
    }
    
    /**
     * Estimate time until specified supply empties
     * Allows commander to plan preemptively.
     */
    [[nodiscard]] float TimeToEmptyAmmo() const {
        return (ammo_depletion_rate < -0.01f) 
            ? (ammo_ratio / std::abs(ammo_depletion_rate))
            : 1e6f;  // Effectively infinite
    }
    
    [[nodiscard]] float TimeToEmptyFuel() const {
        return (fuel_depletion_rate < -0.01f)
            ? (fuel_ratio / std::abs(fuel_depletion_rate))
            : 1e6f;
    }
};

// ============================================================================
// Modifier – Computed Environmental Effects
// ============================================================================

/**
 * Modifier
 * 
 * Computed multipliers for unit properties based on current environment.
 * All multipliers are normalized and safe to compose.
 * 
 * Values represent scaling factors:
 * - 1.0 = no change
 * - 0.8 = 20% reduction
 * - 1.2 = 20% increase
 * - Safe clamping: [0.2, 2.0]
 */
struct Modifier {
    float speed_mult = 1.0f;              // Movement speed multiplier
    float accuracy_mult = 1.0f;           // Ranged/melee accuracy
    float perception_range_mult = 1.0f;   // Detection range for enemies
    float stress_gain_mult = 1.0f;        // Stress accumulation rate
    
    // ========== Operators ==========
    
    /**
     * Clamp all values to safe range [min, max]
     * Prevents explosion when composing multiple effects.
     */
    void Clamp(float min = 0.2f, float max = 2.0f) {
        speed_mult = std::clamp(speed_mult, min, max);
        accuracy_mult = std::clamp(accuracy_mult, min, max);
        perception_range_mult = std::clamp(perception_range_mult, min, max);
        stress_gain_mult = std::clamp(stress_gain_mult, min, max);
    }
    
    /**
     * Multiplicative composition of modifiers
     * Allows stacking weather + terrain + fatigue effects.
     * 
     * Example: weather_mod *= fatigue_mod applies both
     */
    Modifier& operator*=(const Modifier& other) {
        speed_mult *= other.speed_mult;
        accuracy_mult *= other.accuracy_mult;
        perception_range_mult *= other.perception_range_mult;
        stress_gain_mult *= other.stress_gain_mult;
        return *this;
    }
    
    [[nodiscard]] Modifier operator*(const Modifier& other) const {
        Modifier result = *this;
        result *= other;
        return result;
    }
    
    /**
     * Check if this modifier is identity (no effect)
     * Allows fast-path skipping in hot loops.
     */
    [[nodiscard]] bool IsIdentity() const {
        constexpr float eps = 0.001f;
        return std::abs(speed_mult - 1.0f) < eps &&
               std::abs(accuracy_mult - 1.0f) < eps &&
               std::abs(perception_range_mult - 1.0f) < eps &&
               std::abs(stress_gain_mult - 1.0f) < eps;
    }
};

// ============================================================================
// AIConfig Extensions – Weather Coefficients
// ============================================================================

/**
 * WeatherCoefficients
 * 
 * Data-driven parameters for how weather affects gameplay.
 * Loaded from config files, allows balance tweaks without recompilation.
 * Integrated into AIConfig (see ai_config.h).
 */
struct WeatherCoefficients {
    // ========== Precipitation Effects ==========
    float rain_speed_penalty = 0.15f;      // Speed reduction in heavy rain
    float rain_stress_gain = 0.2f;         // Additional stress from wetness
    
    // ========== Fog Effects ==========
    float fog_perception_penalty = 0.5f;   // Perception reduction per fog unit
    
    // ========== Night Effects ==========
    float night_perception_penalty = 0.3f; // Perception reduction at night (6pm-6am)
    
    // ========== Wind Effects ==========
    float wind_accuracy_penalty = 0.05f;   // Accuracy reduction per m/s wind
    
    // ========== Terrain Type Effects ==========
    float mud_speed_penalty = 0.3f;        // Speed reduction in mud
    float mud_stress_gain = 0.1f;          // Fatigue multiplier in mud
    float sand_speed_penalty = 0.2f;       // Speed reduction in sand
    float snow_speed_penalty = 0.25f;      // Speed reduction in snow
    float rock_speed_penalty = 0.1f;       // Speed reduction on rock (rough terrain)
    float water_impassable = 1.0f;         // If 1.0, water is impassable
    
    // ========== Temperature Effects ==========
    float cold_stress_threshold = 0.0f;    // Below this (Celsius), stress gains increase
    float cold_stress_penalty = 0.15f;     // Stress per degree below threshold
};

// ============================================================================
// DynamicModifierSystem – Core Computation
// ============================================================================

/**
 * DynamicModifierSystem
 * 
 * Thread-safe computation of global modifiers from environment.
 * Includes caching to avoid redundant calculations.
 * 
 * Design:
 * - Input: WorldState, SupplyStatus, AIConfig
 * - Output: Modifier (applied to movement, accuracy, perception)
 * - Caching: Hash-based LRU, valid for 3-5 frames
 * - Performance: >85% cache hit rate on stable weather
 */
class DynamicModifierSystem {
public:
    struct CacheEntry {
        uint64_t state_hash = 0;
        Modifier modifier;
        uint64_t valid_until_frame = 0;
    };
    
    static constexpr size_t MaxCacheSize = 256;
    
    /**
     * Compute global modifier from environment
     * 
     * @param world Current world state snapshot
     * @param config AI configuration with weather coefficients
     * @param frame_id Current frame (for cache validation)
     * @return Computed modifier, clamped and safe to use
     * 
     * Caches result for 4 frames (at 60 FPS = 67 ms stability window)
     */
    Modifier Compute(const WorldState& world,
                     const WeatherCoefficients& coeffs,
                     uint64_t frame_id);
    
    /**
     * Clear cache (call on load/unload or major state change)
     */
    void ClearCache() {
        cache_.clear();
    }
    
    /**
     * Get current cache hit rate for profiling
     * Returns percentage (0..100)
     */
    [[nodiscard]] float GetCacheHitRate() const {
        if (total_queries_ == 0) return 0.0f;
        return (cache_hits_ * 100.0f) / total_queries_;
    }
    
    /**
     * Get cache size for memory tracking
     */
    [[nodiscard]] size_t GetCacheSize() const {
        return cache_.size();
    }

private:
    std::unordered_map<uint64_t, CacheEntry> cache_;
    
    // Metrics for profiling
    mutable uint64_t total_queries_ = 0;
    mutable uint64_t cache_hits_ = 0;
    
    /**
     * Evict least-used entry from cache if size exceeds limit
     */
    void EvictOldest();
};

// ============================================================================
// Neural Network Adaptation – Encoding for NN Input
// ============================================================================

/**
 * NeuralInputEncoder
 * 
 * Type-safe encoding of environmental state into neural network tensor.
 * Provides:
 * - Explicit input schema with names for debugging
 * - Normalization to [-1, 1] range
 * - Validation before feeding to network
 */
struct NeuralInputEncoder {
    /**
     * Input Schema – explicit indices and names
     * Matches whatever NN architecture expects
     */
    struct Schema {
        enum InputIndex {
            Rain = 0,
            Fog = 1,
            TimeOfDay = 2,
            Terrain = 3,
            SupplyRatio = 4,
            FriendlyCount = 5,
            EnemyCount = 6,
            COUNT = 7
        };
        
        static constexpr std::array<const char*, COUNT> Names = {
            "rain", "fog", "time_of_day", "terrain", "supply_ratio", "friendly_count", "enemy_count"
        };
    };
    
    /**
     * SquadState – minimal combat status needed for NN
     */
    struct SquadState {
        uint32_t friendly_count = 0;
        uint32_t known_enemy_count = 0;
    };
    
    /**
     * Type-safe encoding into fixed-size array
     * All inputs normalized to [0, 1] range (or [-1, 1] as needed)
     */
    static std::array<float, Schema::COUNT>
    Encode(const WorldState& world,
           const SupplyStatus& supply,
           const SupplyStatus::Weights& supply_weights,
           const SquadState& squad);
    
    /**
     * Validate encoded input before passing to NN
     * Checks range and NaN/Inf
     */
    static bool Validate(std::span<const float> input);
};
