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
#include "medical_system/medical_system.h"
#include "medical_system/crew_status.h"
#include "medical_system/treatment_system.h"
#include "medical_system/injury_simulation.h"

namespace medical_system {

void MedicalSystem::update(float dt, uint64_t frame_id) {
    current_frame_ = frame_id;
    
    for (auto& [id, crew] : crew_states_) {
        // Update crew status
        CrewStatus::update_crew_state(crew, dt);
        
        // If treatment in progress, apply first aid
        if (crew.is_wounded && crew.treatment_progress < 1.0f) {
            // Assume self-treatment or automatic
            TreatmentSystem::apply_first_aid(crew, 1.0f, dt); // Basic skill
        }
        
        // Check for evacuation
        if (TreatmentSystem::request_evacuation(crew)) {
            // TODO: Signal evacuation
        }
    }
}

void MedicalSystem::treat_wounded(physics_core::EntityID medic, physics_core::EntityID patient, float dt) {
    auto medic_it = crew_states_.find(medic);
    auto patient_it = crew_states_.find(patient);
    
    if (medic_it == crew_states_.end() || patient_it == crew_states_.end()) return;
    
    // Medic skill (simplified)
    float medic_skill = 1.0f; // TODO: from crew state
    
    TreatmentSystem::apply_first_aid(patient_it->second, medic_skill, dt);
}

void MedicalSystem::on_damage_event(const physics_core::DamageEvent& event, physics_core::CrewDamageState& crew) {
    // Generate injuries based on damage type
    std::vector<physics_core::Injury> new_injuries;

    const uint32_t damage_flags = event.damage_type;
    const float blast_pressure = event.damage_amount * 100000.0f;
    const float blast_impulse = event.impulse_magnitude;
    const float fire_temperature = event.damage_amount * 500.0f;
    const float fire_duration = 1.0f;

    if (damage_flags & static_cast<uint32_t>(physics_core::DamageTypeFlag::PENETRATION)) {
        physics_core::Fragment frag;
        frag.velocity = event.impact_velocity;
        frag.mass_kg = std::max(0.001f, event.damage_amount * 0.1f);
        frag.penetration_power = event.damage_amount;
        frag.target_component = event.component_id;
        frag.time_to_target = 0.0f;
        frag.impact_energy_joules = event.impulse_magnitude;
        new_injuries = InjurySimulation::calculate_injuries_from_spall(frag, crew, current_frame_);
    } else if (damage_flags & static_cast<uint32_t>(physics_core::DamageTypeFlag::EXPLOSION)) {
        new_injuries = InjurySimulation::calculate_injuries_from_blast(blast_pressure, blast_impulse, crew, current_frame_);
    } else if (damage_flags & static_cast<uint32_t>(physics_core::DamageTypeFlag::THERMAL)) {
        new_injuries = InjurySimulation::calculate_injuries_from_fire(fire_temperature, fire_duration, crew, current_frame_);
    }

    // Add new injuries
    crew.injuries.insert(crew.injuries.end(), new_injuries.begin(), new_injuries.end());
}

std::unordered_map<physics_core::EntityID, physics_core::CrewDamageState>& MedicalSystem::get_crew_states() {
    return crew_states_;
}

} // namespace medical_system