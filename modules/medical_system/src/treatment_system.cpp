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
#include "medical_system/treatment_system.h"
#include <algorithm>

namespace medical_system {

bool TreatmentSystem::apply_first_aid(physics_core::CrewDamageState& crew, float medic_skill, float dt) {
    if (!crew.is_wounded || crew.treatment_progress >= 1.0f) return false;
    
    // Base time for first aid: 2 seconds
    float base_time = 2.0f;
    float effective_time = base_time / medic_skill;
    if (crew.has_medical_kit) effective_time *= 0.5f; // Kit speeds up
    
    // Progress treatment
    crew.treatment_progress = std::min(1.0f, crew.treatment_progress + dt / effective_time);
    
    if (crew.treatment_progress >= 1.0f) {
        // Stop bleeding
        for (auto& injury : crew.injuries) {
            if (injury.blood_loss_rate > 0.0f) {
                injury.blood_loss_rate = 0.0f; // Stop bleeding
            }
        }
        crew.blood_loss_rate = 0.0f;
        
        // Reduce pain
        crew.pain_level = std::max(0.0f, crew.pain_level - 0.3f);
        
        crew.treatment_progress = 0.0f; // Reset for next treatment
        return true;
    }
    
    return false;
}

bool TreatmentSystem::stabilize(physics_core::CrewDamageState& crew, float medic_skill) {
    if (!crew.is_wounded) return false;
    
    // Stabilization prevents worsening for 60 seconds
    crew.time_until_death = std::max(crew.time_until_death, 60.0f);
    
    // Reduce pain further
    crew.pain_level = std::max(0.0f, crew.pain_level - 0.2f);
    
    return true;
}

bool TreatmentSystem::request_evacuation(const physics_core::CrewDamageState& crew) {
    return crew.injury_severity > 0.7f || crew.consciousness_level < 0.5f;
}

} // namespace medical_system