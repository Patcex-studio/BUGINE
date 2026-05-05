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
// Crew Damage System Implementation
// Injury assessment, medical effects, and crew capability degradation

#include "crew_damage_system.h"
#include "damage_system.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace physics_core {

// ============================================================================
// INJURY ASSESSMENT AND APPLICATION
// ============================================================================

void CrewDamageSystem::apply_crew_damage_from_vehicle_damage(
    const VehicleDamageState& vehicle_damage,
    std::vector<CrewDamageState>& crew_states
) {
    for (auto& crew : crew_states) {
        // Apply damage based on crew position
        float protection = calculate_crew_protection_factor(
            vehicle_damage.component_damage_states,
            static_cast<CrewPosition>(crew.crew_position)
        );
        
        // Apply cumulative vehicle damage
        float injury_severity = vehicle_damage.cumulative_damage * (1.0f - protection);
        crew.injury_severity += injury_severity;
        crew.injury_severity = std::min(1.0f, crew.injury_severity);
        
        // Determine if crew member survives
        if (crew.injury_severity > 0.8f) {
            crew.is_killed = true;
        } else if (crew.injury_severity > 0.5f) {
            crew.is_wounded = true;
        }
    }
}

void CrewDamageSystem::calculate_penetration_crew_injuries(
    const BallisticImpactResult& impact,
    const std::vector<ComponentDamageState>& components,
    std::vector<CrewDamageState>& crew_states
) {
    if (!impact.is_penetrated) return;
    
    for (auto& crew : crew_states) {
        // Check if crew member is in hit compartment
        ComponentType crew_compartment = get_crew_compartment(
            static_cast<CrewPosition>(crew.crew_position)
        );
        
        // Find compartment health
        for (const auto& comp : components) {
            // Simplified component lookup
            float proximity_damage = calculate_proximity_damage(
                impact, components, static_cast<CrewPosition>(crew.crew_position)
            );
            
            if (proximity_damage > 0.1f) {
                apply_laceration_injury(crew, proximity_damage, 0.5f);
            }
        }
    }
}

void CrewDamageSystem::calculate_blast_crew_injuries(
    const ExplosionEffect& explosion,
    std::vector<CrewDamageState>& crew_states
) {
    for (auto& crew : crew_states) {
        // Calculate blast trauma
        float blast_trauma = calculate_blast_trauma(
            explosion.blast_pressure_pa,
            10.0f,  // ms exposure
            0.8f    // protection factor
        );
        
        crew.shock_damage = blast_trauma;
        crew.pain_level = std::min(1.0f, crew.pain_level + blast_trauma);
        
        if (blast_trauma > 0.7f) {
            crew.is_unconscious = true;
            crew.consciousness_level = 0.0f;
        }
    }
}

void CrewDamageSystem::calculate_fire_crew_injuries(
    const FirePropagationState& fire_state,
    std::vector<CrewDamageState>& crew_states,
    float delta_time
) {
    for (auto& crew : crew_states) {
        // Calculate burn injuries from fire
        float burn_severity = fire_state.fire_intensity * delta_time * 10.0f;
        
        if (burn_severity > 0.1f) {
            apply_burn_injury(crew, burn_severity, 50.0f);
        }
        
        // Heat damage increases blood loss
        crew.blood_loss_rate += fire_state.fire_intensity * 0.1f;
    }
}

// ============================================================================
// SHOCK AND TRAUMA CALCULATIONS
// ============================================================================

float CrewDamageSystem::calculate_shock_damage(
    float impact_force_g,
    float impact_duration_ms,
    float crew_protection_factor
) {
    float g_factor = impact_force_g / 20.0f;  // 20G is severe
    float duration_factor = impact_duration_ms / 100.0f;  // 100ms exposure
    float protection_factor = 1.0f - crew_protection_factor;
    
    return std::min(1.0f, g_factor * duration_factor * protection_factor);
}

float CrewDamageSystem::calculate_blast_trauma(
    float blast_pressure_pa,
    float exposure_time_ms,
    float crew_protection_factor
) {
    // Blast overpressure causes injury
    // Fatal: > 100 psi (689 kPa)
    // Severe: 30-100 psi (207-689 kPa)
    // Moderate: 10-30 psi (69-207 kPa)
    
    float pressure_psi = blast_pressure_pa / 6894.76f;
    float severity = 0.0f;
    
    if (pressure_psi > 100.0f) severity = 1.0f;
    else if (pressure_psi > 30.0f) severity = (pressure_psi - 30.0f) / 70.0f;
    else if (pressure_psi > 10.0f) severity = (pressure_psi - 10.0f) / 20.0f * 0.5f;
    
    severity *= (exposure_time_ms / 100.0f);
    severity *= (1.0f - crew_protection_factor);
    
    return std::min(1.0f, severity);
}

float CrewDamageSystem::calculate_thermal_shock(
    float temperature_change_k,
    float thermal_protection_factor
) {
    // Adaptation limit: +/- 50K causes stress
    float temp_factor = std::abs(temperature_change_k) / 50.0f;
    float protection = 1.0f - thermal_protection_factor;
    
    return std::min(1.0f, temp_factor * protection * 0.3f);  // 30% of temp factor
}

// ============================================================================
// INJURY PROGRESSION
// ============================================================================

void CrewDamageSystem::process_crew_injuries(
    CrewDamageState& crew_member,
    float delta_time
) {
    if (crew_member.is_killed) return;
    
    // Calculate blood loss
    float blood_loss = calculate_blood_loss(crew_member, delta_time);
    crew_member.injury_severity += blood_loss;
    crew_member.injury_severity = std::min(1.0f, crew_member.injury_severity);
    
    // Update consciousness
    update_consciousness_level(crew_member, delta_time);
    
    // Update injuries over time
    for (auto& injury : crew_member.injuries) {
        update_injury_progression(injury, crew_member, delta_time);
    }
    
    // Check for death
    if (crew_member.injury_severity >= 1.0f || crew_member.blood_loss_rate > 1.0f) {
        crew_member.is_killed = true;
    }
}

float CrewDamageSystem::calculate_blood_loss(
    const CrewDamageState& crew_member,
    float delta_time
) {
    // Blood loss directly affects injury severity
    float blood_loss_rate = crew_member.blood_loss_rate;
    float blood_lost = blood_loss_rate * delta_time;
    
    // Human body has ~5.5L of blood, critical below 2L
    float severity_from_blood_loss = std::min(0.5f, blood_lost / 5.5f);
    
    return severity_from_blood_loss;
}

void CrewDamageSystem::update_consciousness_level(
    CrewDamageState& crew_member,
    float delta_time
) {
    // Consciousness affected by blood loss and injuries
    float consciousness_reduction = (1.0f - crew_member.injury_severity) * 0.1f * delta_time;
    crew_member.consciousness_level -= consciousness_reduction;
    crew_member.consciousness_level = std::max(0.0f, crew_member.consciousness_level);
    
    if (crew_member.consciousness_level < 0.3f) {
        crew_member.is_unconscious = true;
    } else {
        crew_member.is_unconscious = false;
    }
}

float CrewDamageSystem::calculate_survival_probability(
    const CrewDamageState& crew_member
) {
    if (crew_member.is_killed) return 0.0f;
    
    // Survival based on injury severity and time to medical attention
    float medical_factor = 1.0f - crew_member.injury_severity;
    float time_factor = 1.0f;  // Would factor in time since injury
    
    return medical_factor * time_factor;
}

// ============================================================================
// CREW CAPABILITY ASSESSMENT
// ============================================================================

float CrewDamageSystem::calculate_crew_effectiveness(
    const CrewDamageState& crew_member,
    CrewPosition position
) {
    if (crew_member.is_killed || crew_member.is_unconscious) return 0.0f;
    
    float base_effectiveness = 1.0f;
    
    // Apply injury penalties
    base_effectiveness *= (1.0f - crew_member.injury_severity);
    
    // Pain reduces effectiveness
    base_effectiveness *= (1.0f - crew_member.pain_level * 0.5f);
    
    // Position-specific penalties
    switch (position) {
        case CrewPosition::COMMANDER:
            // More affected by concussions
            base_effectiveness *= std::max(0.0f, 1.0f - crew_member.shock_damage);
            break;
        case CrewPosition::GUNNER:
            // Requires precision, hand injuries critical
            for (const auto& injury : crew_member.injuries) {
                if (injury == InjuryType::BONE_FRACTURE) {
                if (injury.type == InjuryType::BONE_FRACTURE) {
                    base_effectiveness *= 0.5f;
                }
            }
            break;
        case CrewPosition::DRIVER:
            // Requires mobility
            for (const auto& injury : crew_member.injuries) {
                if (injury == InjuryType::BONE_FRACTURE || 
                    injury == InjuryType::CRUSHING_INJURY) {
                if (injury.type == InjuryType::BONE_FRACTURE || 
                    injury.type == InjuryType::CRUSHING_INJURY) {
                    base_effectiveness *= 0.6f;
                }
            }
            break;
        default:
            break;
    }
    
    return std::max(0.0f, base_effectiveness);
}

bool CrewDamageSystem::can_perform_duties(
    const CrewDamageState& crew_member,
    CrewPosition position
) {
    float effectiveness = calculate_crew_effectiveness(crew_member, position);
    return effectiveness > 0.4f;  // 40% effectiveness threshold
}

void CrewDamageSystem::calculate_vehicle_crew_effectiveness(
    const std::vector<CrewDamageState>& crew_states,
    float& overall_effectiveness,
    uint32_t& effective_crew_count
) {
    if (crew_states.empty()) {
        overall_effectiveness = 0.0f;
        effective_crew_count = 0;
        return;
    }
    
    float total_effectiveness = 0.0f;
    effective_crew_count = 0;
    
    for (const auto& crew : crew_states) {
        float effectiveness = calculate_crew_effectiveness(
            crew, static_cast<CrewPosition>(crew.crew_position)
        );
        total_effectiveness += effectiveness;
        if (effectiveness > 0.4f) {
            effective_crew_count++;
        }
    }
    
    overall_effectiveness = total_effectiveness / crew_states.size();
}

// ============================================================================
// SPECIFIC INJURY TYPES
// ============================================================================

void CrewDamageSystem::apply_laceration_injury(
    CrewDamageState& crew_member,
    float severity,
    float blood_loss_factor
) {
    crew_member.injuries.push_back(InjuryType::SEVERE_LACERATION);
    Injury inj{InjuryType::SEVERE_LACERATION, severity, severity * blood_loss_factor * 0.2f, severity * 0.5f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * blood_loss_factor * 0.2f;
    crew_member.pain_level += severity * 0.5f;
}

void CrewDamageSystem::apply_fracture_injury(
    CrewDamageState& crew_member,
    float severity
) {
    crew_member.injuries.push_back(InjuryType::BONE_FRACTURE);
    Injury inj{InjuryType::BONE_FRACTURE, severity, severity * 0.05f, severity * 0.8f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * 0.05f;  // Fractures bleed less
    crew_member.pain_level += severity * 0.8f;  // Very painful
}

void CrewDamageSystem::apply_internal_bleeding_injury(
    CrewDamageState& crew_member,
    float severity
) {
    crew_member.injuries.push_back(InjuryType::INTERNAL_BLEEDING);
    Injury inj{InjuryType::INTERNAL_BLEEDING, severity, severity * 0.5f, severity * 0.3f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * 0.5f;
    crew_member.injury_severity += severity * 0.3f;
}

void CrewDamageSystem::apply_burn_injury(
    CrewDamageState& crew_member,
    float severity,
    float burn_area_percent
) {
    crew_member.injuries.push_back(InjuryType::BURN);
    Injury inj{InjuryType::BURN, severity, severity * 0.3f, severity * 0.9f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * 0.3f;
    crew_member.injury_severity += severity * burn_area_percent / 100.0f;
    crew_member.pain_level += severity * 0.9f;  // Burns are extremely painful
}

void CrewDamageSystem::apply_concussion_injury(
    CrewDamageState& crew_member,
    float severity
) {
    crew_member.injuries.push_back(InjuryType::CONCUSSION);
    Injury inj{InjuryType::CONCUSSION, severity, 0.0f, severity * 0.6f};
    crew_member.injuries.push_back(inj);
    crew_member.consciousness_level -= severity * 0.4f;
    crew_member.pain_level += severity * 0.6f;
}

void CrewDamageSystem::apply_shrapnel_injury(
    CrewDamageState& crew_member,
    float severity,
    uint32_t fragment_count
) {
    crew_member.injuries.push_back(InjuryType::SHRAPNEL_WOUND);
    Injury inj{InjuryType::SHRAPNEL_WOUND, severity, severity * 0.4f * fragment_count, severity * 0.7f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * 0.4f * fragment_count;
    crew_member.pain_level += severity * 0.7f;
}

void CrewDamageSystem::apply_crushing_injury(
    CrewDamageState& crew_member,
    float severity
) {
    crew_member.injuries.push_back(InjuryType::CRUSHING_INJURY);
    Injury inj{InjuryType::CRUSHING_INJURY, severity, severity * 0.6f, severity * 0.8f};
    crew_member.injuries.push_back(inj);
    crew_member.blood_loss_rate += severity * 0.6f;  // Heavy bleeding
    crew_member.pain_level += severity * 0.8f;
    crew_member.injury_severity += severity * 0.4f;
}

// ============================================================================
// MEDICAL INTERVENTION
// ============================================================================

void CrewDamageSystem::apply_first_aid(
    CrewDamageState& crew_member,
    float treatment_effectiveness
) {
    // Stop bleeding
    crew_member.blood_loss_rate *= (1.0f - treatment_effectiveness * 0.8f);
    
    // Reduce pain
    crew_member.pain_level *= (1.0f - treatment_effectiveness * 0.5f);
    
    // Improve survival chances
    crew_member.survival_chance = std::min(1.0f, 
        crew_member.survival_chance + treatment_effectiveness * 0.2f);
}

void CrewDamageSystem::stabilize_crew_member(
    CrewDamageState& crew_member
) {
    // Stop progressive deterioration
    crew_member.blood_loss_rate *= 0.5f;
    crew_member.injury_severity += 0.1f;  // Slight increase from stabilization process
}

bool CrewDamageSystem::needs_evacuation(
    const CrewDamageState& crew_member
) {
    return crew_member.is_killed || 
           crew_member.injury_severity > 0.7f ||
           crew_member.blood_loss_rate > 0.5f;
}

// ============================================================================
// COMPARTMENT-SPECIFIC EFFECTS
// ============================================================================

void CrewDamageSystem::apply_driver_compartment_damage(
    const BallisticImpactResult& impact,
    std::vector<CrewDamageState>& crew_states
) {
    for (auto& crew : crew_states) {
        if (crew.crew_position == static_cast<uint32_t>(CrewPosition::DRIVER)) {
            float damage = calculate_proximity_damage(
                impact, std::vector<ComponentDamageState>{}, 
                CrewPosition::DRIVER
            );
            
            if (damage > 0.3f) {
                apply_laceration_injury(crew, damage, 0.8f);
                if (damage > 0.7f) {
                    crew.is_killed = true;
                }
            }
        }
    }
}

void CrewDamageSystem::apply_turret_compartment_damage(
    const BallisticImpactResult& impact,
    std::vector<CrewDamageState>& crew_states
) {
    for (auto& crew : crew_states) {
        if (crew.crew_position == static_cast<uint32_t>(CrewPosition::COMMANDER) ||
            crew.crew_position == static_cast<uint32_t>(CrewPosition::GUNNER)) {
            
            float damage = calculate_proximity_damage(
                impact, std::vector<ComponentDamageState>{},
                static_cast<CrewPosition>(crew.crew_position)
            );
            
            if (damage > 0.2f) {
                apply_shrapnel_injury(crew, damage, 5);
                apply_concussion_injury(crew, damage * 0.5f);
            }
        }
    }
}

void CrewDamageSystem::apply_engine_compartment_damage(
    const BallisticImpactResult& impact,
    const FirePropagationState& fire_state,
    std::vector<CrewDamageState>& crew_states
) {
    // Engine compartment has some crew (e.g., engineer)
    // Less common but still affected by fires
    for (auto& crew : crew_states) {
        if (fire_state.fire_intensity > 0.5f) {
            apply_burn_injury(crew, fire_state.fire_intensity, 30.0f);
        }
    }
}

// ============================================================================
// ARMOR AND PROTECTION
// ============================================================================

float CrewDamageSystem::calculate_crew_protection_factor(
    const std::vector<ComponentDamageState>& components,
    CrewPosition position
) {
    // Protection based on compartment armor integrity
    ComponentType compartment = get_crew_compartment(position);
    
    float total_armor = 0.0f;
    for (const auto& comp : components) {
        total_armor += comp.armor_thickness * comp.current_health;
    }
    
    float average_armor = total_armor / (components.size() + 1.0f);
    return average_armor / 100.0f;  // Normalize to 0-1
}

float CrewDamageSystem::apply_armor_protection(
    float injury_severity,
    float protection_factor
) {
    return injury_severity * (1.0f - protection_factor);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

ComponentType CrewDamageSystem::get_crew_compartment(
    CrewPosition position
) {
    switch (position) {
        case CrewPosition::DRIVER:
            return ComponentType::CREW_COMPARTMENT;
        case CrewPosition::COMMANDER:
        case CrewPosition::GUNNER:
        case CrewPosition::LOADER:
            return ComponentType::TURRET;
        case CrewPosition::RADIO_OPERATOR:
        case CrewPosition::MECHANICAL_ENGINEER:
            return ComponentType::HULL;
        default:
            return ComponentType::HULL;
    }
}

float CrewDamageSystem::calculate_proximity_damage(
    const BallisticImpactResult& impact,
    const std::vector<ComponentDamageState>& components,
    CrewPosition crew_position
) {
    float damage = 0.0f;
    
    if (impact.is_penetrated) {
        damage = std::min(1.0f, impact.damage_energy_joules / 2000000.0f);
    } else if (impact.caused_spalling) {
        damage = std::min(0.5f, impact.damage_energy_joules / 5000000.0f);
    }
    
    return damage;
}

float CrewDamageSystem::calculate_injury_severity(
    float damage_energy,
    float protection_factor
) {
    float base_severity = std::min(1.0f, damage_energy / 1000000.0f);
    return base_severity * (1.0f - protection_factor);
}

void CrewDamageSystem::estimate_injury_distribution(
    float damage_energy,
    uint32_t damage_type,
    std::vector<InjuryType>& injuries,
    std::vector<float>& injury_severities
) {
    injuries.clear();
    injury_severities.clear();
    
    if (damage_type == static_cast<uint32_t>(DamageTypeFlag::PENETRATION)) {
        injuries.push_back(InjuryType::LACERATION);
        injury_severities.push_back(0.8f);
        injuries.push_back(InjuryType::INTERNAL_BLEEDING);
        injury_severities.push_back(0.6f);
    } else if (damage_type == static_cast<uint32_t>(DamageTypeFlag::EXPLOSION)) {
        injuries.push_back(InjuryType::BLAST_TRAUMA);
        injury_severities.push_back(0.7f);
        injuries.push_back(InjuryType::SHRAPNEL_WOUND);
        injury_severities.push_back(0.5f);
    } else if (damage_type == static_cast<uint32_t>(DamageTypeFlag::FIRE)) {
        injuries.push_back(InjuryType::BURN);
        injury_severities.push_back(0.9f);
        injuries.push_back(InjuryType::THERMAL_SHOCK);
        injury_severities.push_back(0.4f);
    }
}

float CrewDamageSystem::calculate_bleeding_rate(
    const std::vector<InjuryType>& injuries
) {
    float rate = 0.0f;
    
    for (const auto& injury : injuries) {
        switch (injury) {
            case InjuryType::SEVERE_LACERATION:
                rate += 0.3f;
                break;
            case InjuryType::INTERNAL_BLEEDING:
                rate += 0.2f;
                break;
            case InjuryType::SHRAPNEL_WOUND:
                rate += 0.25f;
                break;
            default:
                break;
        }
    }
    
    return rate;
}

float CrewDamageSystem::calculate_consciousness_change(
    const CrewDamageState& crew_member,
    float delta_time
) {
    float change = 0.0f;
    
    // Blood loss reduces consciousness
    change -= crew_member.blood_loss_rate * delta_time * 0.1f;
    
    // Injuries reduce consciousness  
    change -= crew_member.injury_severity * delta_time * 0.05f;
    
    // Pain affects consciousness
    change -= crew_member.pain_level * delta_time * 0.02f;
    
    return change;
}

void CrewDamageSystem::update_injury_progression(
    InjuryType injury,
    CrewDamageState& crew_member,
    float delta_time
) {
    // Injuries progress over time if untreated
    switch (injury) {
        case InjuryType::INTERNAL_BLEEDING:
            crew_member.blood_loss_rate += delta_time * 0.01f;
            crew_member.injury_severity += delta_time * 0.02f;
            break;
        case InjuryType::BURN:
            crew_member.injury_severity += delta_time * 0.01f;  // Slow progression
            break;
        case InjuryType::BONE_FRACTURE:
            // Fractures don't progress but immobilize
            break;
        default:
            break;
    }
}

} // namespace physics_core
