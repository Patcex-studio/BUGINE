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
#include "medical_system/injury_simulation.h"
#include <algorithm>

namespace medical_system {

std::vector<physics_core::Injury> InjurySimulation::calculate_injuries_from_spall(
    const physics_core::Fragment& frag, 
    const physics_core::CrewDamageState& crew,
    uint64_t frame_id
) {
    std::vector<physics_core::Injury> injuries;
    physics_core::DeterministicRNG rng(physics_core::DeterministicRNG::make_seed(crew.crew_member_entity, frame_id));
    
    // Calculate impact severity based on fragment properties
    float velocity = std::sqrt(frag.velocity[0]*frag.velocity[0] + frag.velocity[1]*frag.velocity[1] + frag.velocity[2]*frag.velocity[2]);
    float kinetic_energy = 0.5f * frag.mass_kg * velocity * velocity;
    
    // Hit probability (simplified)
    float hit_probability = std::min(1.0f, frag.penetration_power * 0.8f);
    
    if (rng.next_float() < hit_probability) {
        physics_core::InjuryType injury_type;
        float severity;
        
        // Determine injury type based on fragment properties
        if (frag.mass_kg > 10.0f && velocity > 800.0f) {
            injury_type = physics_core::InjuryType::SHRAPNEL_WOUND;
            severity = std::min(1.0f, (frag.mass_kg * velocity) / 10000.0f);
        } else if (frag.mass_kg > 5.0f && velocity > 500.0f) {
            injury_type = physics_core::InjuryType::SEVERE_LACERATION;
            severity = std::min(1.0f, (frag.mass_kg * velocity) / 5000.0f);
        } else {
            injury_type = physics_core::InjuryType::MINOR_LACERATION;
            severity = std::min(1.0f, (frag.mass_kg * velocity) / 2000.0f);
        }
        
        // Randomize severity slightly
        severity = std::clamp(severity + (rng.next_float() - 0.5f) * 0.2f, 0.1f, 1.0f);
        
        float blood_loss_rate = severity * 50.0f; // 50 ml/s max
        float pain_factor = severity;
        
        injuries.push_back({
            injury_type,
            severity,
            blood_loss_rate,
            pain_factor
        });
    }
    
    return injuries;
}

std::vector<physics_core::Injury> InjurySimulation::calculate_injuries_from_blast(
    float pressure, 
    float impulse, 
    const physics_core::CrewDamageState& crew,
    uint64_t frame_id
) {
    std::vector<physics_core::Injury> injuries;
    physics_core::DeterministicRNG rng(physics_core::DeterministicRNG::make_seed(crew.crew_member_entity, frame_id));
    
    // Blast trauma threshold
    const float blast_threshold_pa = 5000.0f; // 5 kPa
    
    if (pressure > blast_threshold_pa) {
        float severity = std::min(1.0f, (pressure - blast_threshold_pa) / 50000.0f);
        
        // Possible injuries: BLAST_TRAUMA, CONCUSSION, INTERNAL_BLEEDING
        if (rng.next_float() < 0.7f) {
            injuries.push_back({
                physics_core::InjuryType::BLAST_TRAUMA,
                severity,
                severity * 20.0f, // Less blood loss
                severity * 0.8f
            });
        }
        
        if (rng.next_float() < 0.5f) {
            injuries.push_back({
                physics_core::InjuryType::CONCUSSION,
                severity * 0.5f,
                0.0f, // No blood loss
                severity * 0.6f
            });
        }
        
        if (rng.next_float() < 0.3f && pressure > 10000.0f) {
            injuries.push_back({
                physics_core::InjuryType::INTERNAL_BLEEDING,
                severity * 0.8f,
                severity * 30.0f,
                severity * 0.9f
            });
        }
    }
    
    return injuries;
}

std::vector<physics_core::Injury> InjurySimulation::calculate_injuries_from_fire(
    float temperature, 
    float duration, 
    const physics_core::CrewDamageState& crew,
    uint64_t frame_id
) {
    std::vector<physics_core::Injury> injuries;
    physics_core::DeterministicRNG rng(physics_core::DeterministicRNG::make_seed(crew.crew_member_entity, frame_id));
    
    // Burn threshold
    const float burn_threshold_c = 60.0f; // 60°C
    
    if (temperature > burn_threshold_c) {
        float severity = std::min(1.0f, (temperature - burn_threshold_c) / 200.0f * (duration / 10.0f));
        
        // Burn injury
        injuries.push_back({
            physics_core::InjuryType::BURN,
            severity,
            severity * 10.0f, // Burns cause some blood loss
            severity
        });
        
        // Possible thermal shock
        if (temperature > 100.0f && rng.next_float() < 0.4f) {
            injuries.push_back({
                physics_core::InjuryType::THERMAL_SHOCK,
                severity * 0.5f,
                0.0f,
                severity * 0.7f
            });
        }
    }
    
    return injuries;
}

} // namespace medical_system