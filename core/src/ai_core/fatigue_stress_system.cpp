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
#include "ai_core/fatigue_stress_system.h"
#include "ai_core/decision_engine.h" // For UnitSoA

void FatigueStressSystem::update(UnitSoA& units, float dt, const AIConfig& cfg) {
    for (size_t i = 0; i < units.fatigue.size(); ++i) {
        // Calculate speed ratio
        float speed = std::sqrt(units.velocities_x[i] * units.velocities_x[i] + 
                               units.velocities_y[i] * units.velocities_y[i]);
        float max_speed = 5.0f; // Placeholder, assume max speed
        float speed_ratio = speed / max_speed;
        
        // Load factor: placeholder, assume 0.5
        float load_factor = 0.5f;
        
        // Is resting: if speed < 0.1 and stance prone/crouch, but since not defined, assume false
        bool is_resting = (speed < 0.1f);
        
        // Fatigue delta
        float fatigue_delta = speed_ratio * (1.0f + load_factor);
        if (is_resting) fatigue_delta = -0.2f;
        units.fatigue[i] += dt * fatigue_delta;
        units.fatigue[i] = std::clamp(units.fatigue[i], 0.0f, 1.0f);
        
        // Stress
        // Suppression: placeholder 0.0
        float suppression = 0.0f;
        // Has threat: placeholder false
        bool has_threat = false;
        // No threat timer: placeholder 0.0
        float no_threat_timer = 0.0f;
        
        float stress_delta = suppression * 0.3f;
        if (!has_threat && no_threat_timer > 5.0f) stress_delta = -0.1f;
        units.stress[i] += dt * stress_delta;
        units.stress[i] = std::clamp(units.stress[i], 0.0f, 1.0f);
        
        // Update no_threat_timer
        if (has_threat) {
            no_threat_timer = 0.0f;
        } else {
            no_threat_timer += dt;
        }
        // Store back, but since not in UnitSoA, skip
    }
}