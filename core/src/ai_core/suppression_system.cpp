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
#include "ai_core/suppression_system.h"
#include <algorithm>
#include <cmath>
#include <numeric>

// ============================================================================
// SuppressionSystem Implementation
// ============================================================================

SuppressionSystem::SuppressionSystem(const SuppressionConfig& config)
    : config_(config) {
}

void SuppressionSystem::CreateSuppressionZone(const Vector3& center,
                                             float radius,
                                             float intensity,
                                             EntityId source_id,
                                             float current_time_s) {
    SuppressionZone zone;
    zone.center = center;
    zone.radius = radius;
    zone.intensity = intensity;
    zone.source_id = source_id;
    zone.creation_time_s = current_time_s;
    zone.decay_rate = config_.decay_rate;
    
    suppression_zones_.push_back(zone);
}

void SuppressionSystem::UpdateSuppressionBatch(
    std::span<UnitTacticalState> units,
    std::span<const Vector3> positions,
    std::span<const float> cover_quality,
    float dt_s,
    float current_time_s) {
    
    // Clean up expired zones first
    CleanupExpiredZones(current_time_s);
    
    // Early exit if no zones
    if (suppression_zones_.empty()) {
        // Just apply decay to existing suppression
        for (auto& unit : units) {
            unit.suppression_level *= std::exp(-config_.decay_rate * dt_s);
            unit.suppression_history = unit.suppression_history * 0.98f + 
                                      unit.suppression_level * 0.02f;
        }
        return;
    }
    
    // Update each unit
    for (size_t i = 0; i < units.size() && i < positions.size(); ++i) {
        float cover = (i < cover_quality.size()) ? cover_quality[i] : 0.0f;
        UpdateUnitSuppression(units[i], positions[i], cover, dt_s, current_time_s);
        ComputeDerivedValues(units[i], config_);
    }
}

void SuppressionSystem::UpdateUnitSuppression(
    UnitTacticalState& unit,
    const Vector3& position,
    float cover_quality,
    float dt_s,
    float current_time_s) {
    
    // Calculate total suppression from all zones
    float incoming_suppression = 0.0f;
    
    for (const auto& zone : suppression_zones_) {
        float intensity = zone.GetIntensity(position, current_time_s);
        if (intensity > 0.001f) {
            // Reduce by cover
            float effective_intensity = intensity * (1.0f - cover_quality);
            incoming_suppression += effective_intensity;
            
            // Track as active source
            SuppressionSource source;
            source.shooter_id = zone.source_id;
            source.direction = glm::normalize(position - zone.center);
            source.intensity = intensity;
            source.last_update_time_s = current_time_s;
            source.age_s = current_time_s - zone.creation_time_s;
            
            unit.AddSuppressionSource(source);
        }
    }
    
    // Cap incoming suppression
    incoming_suppression = std::clamp(incoming_suppression, 0.0f, 1.0f);
    
    // Update suppression level with leaky integrator
    // Ramp up quickly, decay slowly
    if (incoming_suppression > unit.suppression_level) {
        // Ascending: fast response
        unit.suppression_level += (incoming_suppression - unit.suppression_level) * 
                                 (1.0f - std::exp(-5.0f * dt_s));
    } else {
        // Descending: slow decay
        unit.suppression_level *= std::exp(-config_.decay_rate * dt_s);
    }
    
    // Update history for ghistoresis
    unit.suppression_history = unit.suppression_history * 0.7f + 
                               unit.suppression_level * 0.3f;
    
    // Age sources
    for (auto& src : unit.active_sources) {
        if (src) {
            src->age_s += dt_s;
        }
    }
}

void SuppressionSystem::ComputeDerivedValues(UnitTacticalState& unit,
                                            const SuppressionConfig& config) {
    // Compute accuracy multiplier
    unit.accuracy_mult = GetEffectiveAccuracy(unit, config);
    
    // Compute stress gain multiplier
    unit.stress_gain_mult = 1.0f + (unit.suppression_level * config.stress_gain_multiplier);
    
    // Check behavioral flags
    unit.should_break_formation = ShouldBreakFormation(unit, config);
    unit.refuses_to_move = RefusesMove(unit, config);
    
    // Extract primary threat direction
    unit.primary_threat_direction = unit.GetPrimaryThreatDirection();
}

void SuppressionSystem::CleanupExpiredZones(float current_time_s) {
    // Remove zones that have decayed to negligible intensity
    auto new_end = std::remove_if(
        suppression_zones_.begin(),
        suppression_zones_.end(),
        [this, current_time_s](const SuppressionZone& zone) {
            float age = current_time_s - zone.creation_time_s;
            float intensity = zone.intensity * std::exp(-zone.decay_rate * age);
            return intensity < 0.01f;  // Negligible threshold
        }
    );
    
    suppression_zones_.erase(new_end, suppression_zones_.end());
}

float SuppressionUtils::ComputeSuppressionFromZone(
    const SuppressionZone& zone,
    const Vector3& unit_pos,
    float cover_quality,
    float current_time_s) {
    
    float intensity = zone.GetIntensity(unit_pos, current_time_s);
    if (intensity < 0.001f) return 0.0f;
    
    // Cover reduces suppression effectiveness
    return intensity * (1.0f - cover_quality);
}
