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
#include <span>
#include <array>
#include <optional>
#include <algorithm>
#include <glm/glm.hpp>

using Vector3 = glm::vec3;
using EntityId = uint64_t;

// ============================================================================
// Suppression System – Phase 3: Cover & Suppression Model
// ============================================================================

/**
 * SuppressionConfig
 * 
 * Configuration for suppression mechanics.
 */
struct SuppressionConfig {
    // Mechanics
    float accuracy_penalty = 0.8f;      // How much suppression reduces accuracy
    float min_accuracy_mult = 0.2f;     // Floor for accuracy multiplier
    float stress_gain_multiplier = 0.5f; // Stress gain from suppression
    
    // Decay
    float decay_rate = 2.0f;            // Suppression decay rate (exp(-decay_rate * dt))
    
    // Thresholds
    float break_formation_threshold = 0.7f;  // Above this, units may break formation
    float refuse_move_threshold = 0.7f;      // Above this, unit won't move under fire
};

/**
 * SuppressionSource
 * 
 * Represents active source of suppressive fire.
 * Used for tactical decisions (IA can see where fire is coming from).
 */
struct SuppressionSource {
    EntityId shooter_id;                // Who is shooting
    Vector3 direction;                  // Direction of fire (unit vector or position)
    float intensity = 1.0f;             // Suppression power (0..1)
    float last_update_time_s = 0.0f;    // Last time this source was active
    
    // Derived
    float age_s = 0.0f;                 // How long this source has been active (for decay)
    
    /**
     * Check if this source is still "fresh" (recently updated)
     */
    bool IsActive(float current_time_s, float fade_threshold_s = 2.0f) const {
        return (current_time_s - last_update_time_s) < fade_threshold_s;
    }
};

/**
 * SuppressionZone
 * 
 * Spatial region affected by suppressive fire.
 * Used for spatial queries to avoid O(N) per-unit checks.
 */
struct SuppressionZone {
    Vector3 center;                     // Center of zone
    float radius = 10.0f;               // Radius of effect (meters)
    float intensity = 1.0f;             // Peak intensity at center
    EntityId source_id;                 // Source of suppression
    float creation_time_s = 0.0f;
    float decay_rate = 2.0f;
    
    /**
     * Get intensity at a given position
     * Decays with distance and time
     */
    float GetIntensity(const Vector3& pos, float current_time_s) const {
        float dist = glm::distance(pos, center);
        float dist_falloff = std::max(0.0f, 1.0f - (dist / radius));
        
        float age_s = current_time_s - creation_time_s;
        float time_decay = std::exp(-decay_rate * age_s);
        
        return intensity * dist_falloff * time_decay;
    }
};

/**
 * UnitTacticalState
 * 
 * Tactical state added to UnitSoA for each unit.
 * Tracks suppression, accuracy, and active fire sources.
 */
struct UnitTacticalState {
    // ========== Suppression ==========
    float suppression_level = 0.0f;     // 0..1, current suppression
    float suppression_history = 0.0f;   // Smoothed history for ghistoresis
    float accuracy_mult = 1.0f;         // Derived from suppression
    float stress_gain_mult = 1.0f;      // Stress multiplier from suppression
    
    // ========== Fire Sources (limited to 3 for performance) ==========
    static constexpr size_t MAX_SOURCES = 3;
    std::array<std::optional<SuppressionSource>, MAX_SOURCES> active_sources;
    
    // ========== Behavioral Flags ==========
    bool should_break_formation = false; // Suppression exceeds threshold
    bool refuses_to_move = false;        // Suppression too high to move
    
    // ========== Derived (computed each frame) ==========
    Vector3 primary_threat_direction = Vector3(0, 0, 1);  // Direction of strongest threat
    
    /**
     * Add or update a suppression source
     * If sources are full, replaces oldest
     */
    void AddSuppressionSource(const SuppressionSource& source) {
        // Try to find empty slot
        for (auto& slot : active_sources) {
            if (!slot) {
                slot = source;
                return;
            }
        }
        
        // All full, replace oldest
        int oldest_idx = 0;
        float oldest_age = -1.0f;
        for (size_t i = 0; i < MAX_SOURCES; ++i) {
            if (active_sources[i]) {
                float age = active_sources[i]->age_s;
                if (age > oldest_age) {
                    oldest_age = age;
                    oldest_idx = i;
                }
            }
        }
        active_sources[oldest_idx] = source;
    }
    
    /**
     * Get dominant threat direction from all sources
     */
    Vector3 GetPrimaryThreatDirection() const {
        Vector3 combined = Vector3(0);
        float total_intensity = 0.0f;
        
        for (const auto& src : active_sources) {
            if (src && src->age_s < 2.0f) {  // Only active, recent sources
                combined += src->direction * src->intensity;
                total_intensity += src->intensity;
            }
        }
        
        if (total_intensity > 0.001f) {
            return glm::normalize(combined);
        }
        return Vector3(0, 0, 1);  // Default forward
    }
};

// ============================================================================
// SuppressionSystem
// 
// Core system for calculating and updating suppression effects.
// ============================================================================

class SuppressionSystem {
public:
    explicit SuppressionSystem(const SuppressionConfig& config);
    
    ~SuppressionSystem() = default;
    
    /**
     * Create a suppression zone at a position
     * Called when a weapon fires in an area
     */
    void CreateSuppressionZone(const Vector3& center,
                              float radius,
                              float intensity,
                              EntityId source_id,
                              float current_time_s);
    
    /**
     * Update all units' suppression levels based on active zones
     * Batch operation for performance
     */
    void UpdateSuppressionBatch(
        std::span<UnitTacticalState> units,
        std::span<const Vector3> positions,
        std::span<const float> cover_quality,
        float dt_s,
        float current_time_s
    );
    
    /**
     * Update a single unit's suppression level
     */
    void UpdateUnitSuppression(
        UnitTacticalState& unit,
        const Vector3& position,
        float cover_quality,
        float dt_s,
        float current_time_s
    );
    
    /**
     * Compute derived values (accuracy_mult, stress_gain_mult, behavioral flags)
     * from suppression_level (usually done after UpdateSuppression)
     */
    static void ComputeDerivedValues(UnitTacticalState& unit,
                                    const SuppressionConfig& config);
    
    /**
     * Check if a unit should break formation due to suppression
     */
    static bool ShouldBreakFormation(const UnitTacticalState& unit,
                                    const SuppressionConfig& config) {
        return unit.suppression_level > config.break_formation_threshold;
    }
    
    /**
     * Check if a unit refuses to move (suppressed too much)
     */
    static bool RefusesMove(const UnitTacticalState& unit,
                           const SuppressionConfig& config) {
        return unit.suppression_level > config.refuse_move_threshold;
    }
    
    /**
     * Get effective accuracy multiplier
     * Takes suppression and ghistoresis into account
     */
    static float GetEffectiveAccuracy(const UnitTacticalState& unit,
                                     const SuppressionConfig& config) {
        // Ghistoresis: 70% current, 30% history for smooth transitions
        float effective = unit.suppression_level * 0.7f + unit.suppression_history * 0.3f;
        return std::clamp(1.0f - effective * config.accuracy_penalty,
                         config.min_accuracy_mult, 1.0f);
    }
    
    /**
     * Clear all zones (e.g., for scenario reset)
     */
    void ClearZones() { suppression_zones_.clear(); }
    
    /**
     * Get diagnostics (for debugging)
     */
    size_t GetActiveZoneCount() const { return suppression_zones_.size(); }
    
private:
    SuppressionConfig config_;
    std::vector<SuppressionZone> suppression_zones_;
    
    /**
     * Clean up expired zones
     */
    void CleanupExpiredZones(float current_time_s);
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace SuppressionUtils {
    /**
     * Compute suppression from a single zone
     * Takes into account distance falloff and intensity
     */
    float ComputeSuppressionFromZone(
        const SuppressionZone& zone,
        const Vector3& unit_pos,
        float cover_quality,
        float current_time_s
    );
}
