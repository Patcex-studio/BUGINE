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

#include <vector>
#include <cstdint>
#include "damage_system.h"
#include "types.h"

namespace physics_core {

// Forward declarations
struct CrewDamageState;
struct VehicleDamageState;
struct ComponentDamageState;

// Crew positions enumeration
enum class CrewPosition : uint32_t {
    COMMANDER = 0,
    GUNNER = 1,
    DRIVER = 2,
    LOADER = 3,
    RADIO_OPERATOR = 4,
    MECHANICAL_ENGINEER = 5,
    COUNT
};

// ============================================================================
// CREW DAMAGE SYSTEM
// ============================================================================

class CrewDamageSystem {
public:
    // ========================================================================
    // INJURY ASSESSMENT AND APPLICATION
    // ========================================================================
    
    // Apply vehicle damage to crew members
    static void apply_crew_damage_from_vehicle_damage(
        const VehicleDamageState& vehicle_damage,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Calculate crew injuries from penetrating hit
    static void calculate_penetration_crew_injuries(
        const BallisticImpactResult& impact,
        const std::vector<ComponentDamageState>& components,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Calculate crew injuries from blast effect
    static void calculate_blast_crew_injuries(
        const ExplosionEffect& explosion,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Calculate crew injuries from fire
    static void calculate_fire_crew_injuries(
        const FirePropagationState& fire_state,
        std::vector<CrewDamageState>& crew_states,
        float delta_time
    );
    
    // ========================================================================
    // SHOCK AND TRAUMA
    // ========================================================================
    
    // Calculate shock damage from impact
    static float calculate_shock_damage(
        float impact_force_g,
        float impact_duration_ms,
        float crew_protection_factor
    );
    
    // Calculate blast trauma
    static float calculate_blast_trauma(
        float blast_pressure_pa,
        float exposure_time_ms,
        float crew_protection_factor
    );
    
    // Calculate thermal shock
    static float calculate_thermal_shock(
        float temperature_change_k,
        float thermal_protection_factor
    );
    
    // ========================================================================
    // INJURY PROGRESSION
    // ========================================================================
    
    // Process crew injuries over time
    static void process_crew_injuries(
        CrewDamageState& crew_member,
        float delta_time
    );
    
    // Calculate blood loss
    static float calculate_blood_loss(
        const CrewDamageState& crew_member,
        float delta_time
    );
    
    // Update consciousness level
    static void update_consciousness_level(
        CrewDamageState& crew_member,
        float delta_time
    );
    
    // Calculate survival probability
    static float calculate_survival_probability(
        const CrewDamageState& crew_member
    );
    
    // ========================================================================
    // CREW CAPABILITY ASSESSMENT
    // ========================================================================
    
    // Calculate crew member effectiveness
    static float calculate_crew_effectiveness(
        const CrewDamageState& crew_member,
        CrewPosition position
    );
    
    // Determine if crew member can continue duties
    static bool can_perform_duties(
        const CrewDamageState& crew_member,
        CrewPosition position
    );
    
    // Calculate vehicle crew combat effectiveness
    static void calculate_vehicle_crew_effectiveness(
        const std::vector<CrewDamageState>& crew_states,
        float& overall_effectiveness,
        uint32_t& effective_crew_count
    );
    
    // ========================================================================
    // SPECIFIC INJURY TYPES
    // ========================================================================
    
    // Lacerations
    static void apply_laceration_injury(
        CrewDamageState& crew_member,
        float severity,
        float blood_loss_factor
    );
    
    // Fractures
    static void apply_fracture_injury(
        CrewDamageState& crew_member,
        float severity
    );
    
    // Internal bleeding
    static void apply_internal_bleeding_injury(
        CrewDamageState& crew_member,
        float severity
    );
    
    // Burns
    static void apply_burn_injury(
        CrewDamageState& crew_member,
        float severity,
        float burn_area_percent
    );
    
    // Concussion
    static void apply_concussion_injury(
        CrewDamageState& crew_member,
        float severity
    );
    
    // Shrapnel wounds
    static void apply_shrapnel_injury(
        CrewDamageState& crew_member,
        float severity,
        uint32_t fragment_count
    );
    
    // Crushing injuries
    static void apply_crushing_injury(
        CrewDamageState& crew_member,
        float severity
    );
    
    // ========================================================================
    // MEDICAL INTERVENTION
    // ========================================================================
    
    // Apply first aid
    static void apply_first_aid(
        CrewDamageState& crew_member,
        float treatment_effectiveness
    );
    
    // Stabilize wounded crew member
    static void stabilize_crew_member(
        CrewDamageState& crew_member
    );
    
    // Calculate evacuation necessity
    static bool needs_evacuation(
        const CrewDamageState& crew_member
    );
    
    // ========================================================================
    // COMPARTMENT-SPECIFIC EFFECTS
    // ========================================================================
    
    // Apply driver compartment damage
    static void apply_driver_compartment_damage(
        const BallisticImpactResult& impact,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Apply turret compartment damage
    static void apply_turret_compartment_damage(
        const BallisticImpactResult& impact,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Apply engine compartment damage
    static void apply_engine_compartment_damage(
        const BallisticImpactResult& impact,
        const FirePropagationState& fire_state,
        std::vector<CrewDamageState>& crew_states
    );
    
    // ========================================================================
    // ARMOR AND PROTECTION
    // ========================================================================
    
    // Calculate crew protection factor
    static float calculate_crew_protection_factor(
        const std::vector<ComponentDamageState>& components,
        CrewPosition position
    );
    
    // Apply armor protection to injury
    static float apply_armor_protection(
        float injury_severity,
        float protection_factor
    );

private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Get crew member compartment
    static ComponentType get_crew_compartment(
        CrewPosition position
    );
    
    // Calculate proximity damage
    static float calculate_proximity_damage(
        const BallisticImpactResult& impact,
        const std::vector<ComponentDamageState>& components,
        CrewPosition crew_position
    );
    
    // Calculate injury severity from damage
    static float calculate_injury_severity(
        float damage_energy,
        float protection_factor
    );
    
    // Estimate injury types from damage
    static void estimate_injury_distribution(
        float damage_energy,
        uint32_t damage_type,
        std::vector<InjuryType>& injuries,
        std::vector<float>& injury_severities
    );
    
    // Calculate bleeding rate from injury
    static float calculate_bleeding_rate(
        const std::vector<InjuryType>& injuries
    );
    
    // Calculate consciousness change
    static float calculate_consciousness_change(
        const CrewDamageState& crew_member,
        float delta_time
    );
    
    // Update injury progression
    static void update_injury_progression(
        InjuryType injury,
        CrewDamageState& crew_member,
        float delta_time
    );
};

} // namespace physics_core
