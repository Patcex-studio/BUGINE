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
#include "medical_system/crew_status.h"
#include <algorithm>
#include <cmath>

namespace medical_system {

void CrewStatus::update_crew_state(physics_core::CrewDamageState& crew, float dt) {
    // Initialize blood volume if not set
    if (crew.blood_volume <= 0.0f) {
        crew.blood_volume = 5000.0f; // Max blood volume
    }
    
    // 1. Sum blood loss from all injuries
    float total_blood_loss_rate = 0.0f;
    float total_pain = 0.0f;
    for (const auto& injury : crew.injuries) {
        total_blood_loss_rate += injury.blood_loss_rate;
        total_pain += injury.pain_factor * injury.severity;
    }
    
    // Assume max blood volume ~5000 ml
    const float max_blood = 5000.0f;
    crew.blood_volume = std::max(0.0f, crew.blood_volume - total_blood_loss_rate * dt);
    
    // 2. Consciousness: nonlinear dependence + smoothing
    float blood_ratio = crew.blood_volume / max_blood;
    float target_consciousness = std::clamp(1.0f - std::pow(1.0f - blood_ratio, 1.5f), 0.0f, 1.0f);
    
    // Exponential smoothing
    crew.consciousness_level = crew.consciousness_level * 0.95f + target_consciousness * 0.05f;
    
    // 3. Pain level with damping
    crew.pain_level = std::clamp(crew.pain_level * 0.98f + total_pain * 0.02f, 0.0f, 1.0f);
    
    // 4. Unconscious if consciousness < 0.2
    crew.is_unconscious = crew.consciousness_level < 0.2f;
    
    // 5. Incapacitated if unconscious or pain > 0.8
    crew.is_incapacitated = crew.is_unconscious || crew.pain_level > 0.8f;
    
    // 6. Death timer
    if (crew.blood_volume < max_blood * 0.1f && crew.consciousness_level < 0.05f) {
        crew.time_until_death -= dt;
        if (crew.time_until_death <= 0.0f) {
            crew.is_killed = true;
        }
    } else {
        crew.time_until_death = 30.0f; // Reset
    }
    
    // Update injury severity
    crew.injury_severity = 0.0f;
    for (const auto& injury : crew.injuries) {
        crew.injury_severity = std::max(crew.injury_severity, injury.severity);
    }
    
    crew.injury_count = crew.injuries.size();
    crew.is_wounded = !crew.injuries.empty();
}

float CrewStatus::calculate_consciousness(const physics_core::CrewDamageState& crew) {
    return crew.consciousness_level;
}

float CrewStatus::calculate_survival_chance(const physics_core::CrewDamageState& crew) {
    if (crew.is_killed) return 0.0f;
    
    float blood_ratio = crew.blood_volume / 5000.0f;
    float injury_penalty = crew.injury_severity;
    float time_factor = std::max(0.0f, crew.time_until_death / 300.0f); // 5 min base
    
    return std::clamp(blood_ratio * (1.0f - injury_penalty) * time_factor, 0.0f, 1.0f);
}

bool CrewStatus::is_incapacitated(const physics_core::CrewDamageState& crew) {
    return crew.is_incapacitated;
}

} // namespace medical_system