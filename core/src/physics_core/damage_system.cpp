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
#include "damage_system.h"
#include "physics_core/physics_core.h"
#include "physics_core.h"
#include "ballistics_system.h"
#include "secondary_effects_system.h"
#include "crew_damage_system.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <cstring>
#include <numeric>

namespace physics_core {

// ============================================================================
// MAIN DAMAGE PROCESSING FUNCTIONS
// ============================================================================

BallisticImpactResult DamageSystem::calculate_ballistic_impact(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    const ImpactParameters& impact_params,
    BallisticImpactResult& result
) {
    // Initialize result
    std::memset(&result, 0, sizeof(result));
    result.impact_position = impact_params.impact_position;
    result.impact_normal = impact_params.impact_normal;
    result.impact_angle_deg = impact_params.impact_angle;
    result.time_to_impact_ms = impact_params.time_to_impact;
    
    // Calculate effective armor thickness based on angle
    float effective_thickness = calculate_effective_armor_thickness(
        armor.thickness_mm,
        impact_params.impact_angle
    );
    result.normalized_impact_angle = effective_thickness / armor.thickness_mm;
    
    // Call ballistics system for penetration calculation
    BallisticsSystem::calculate_ballistic_impact(projectile, armor, impact_params, result);
    
    // If penetrated, calculate exit position
    if (result.is_penetrated) {
        // Estimate exit position based on armor thickness and angle
        float exit_depth = result.penetration_depth_mm;
        __m256 exit_offset = {
            impact_params.impact_normal[0] * exit_depth * 0.001f,
            impact_params.impact_normal[1] * exit_depth * 0.001f,
            impact_params.impact_normal[2] * exit_depth * 0.001f,
            0.0f
        };
        result.exit_position = result.impact_position;
        result.exit_position[0] += exit_offset[0];
        result.exit_position[1] += exit_offset[1];
        result.exit_position[2] += exit_offset[2];
    }
    
    return result;
}

void DamageSystem::apply_damage_to_component(
    EntityID component_entity,
    const BallisticImpactResult& impact,
    ComponentDamageState& component_state
) {
    // Update health based on penetration
    float health_reduction = 0.0f;
    
    if (impact.is_penetrated) {
        // Full penetration deals maximum damage
        health_reduction = std::min(1.0f, impact.damage_energy_joules / 1000000.0f);
    } else if (impact.is_ricocheted) {
        // Ricocheted shot does minimal damage
        health_reduction = 0.1f;
    } else {
        // Partial penetration deals moderate damage
        health_reduction = std::min(0.7f, (impact.damage_energy_joules / 2000000.0f));
    }
    
    component_state.current_health = std::max(0.0f, 
        component_state.current_health - health_reduction);
    
    // Update armor thickness from damage
    if (impact.armor_damage_mm > 0.0f) {
        component_state.armor_thickness = std::max(0.0f,
            component_state.armor_thickness - impact.armor_damage_mm);
    }
    
    // Update structural integrity
    component_state.structural_integrity = std::max(0.0f,
        component_state.structural_integrity - health_reduction);
    
    // Accumulate fatigue damage
    if (impact.damage_energy_joules > 0.0f) {
        component_state.fatigue_damage += 0.05f * (impact.damage_energy_joules / 1000.0f);
    }
    
    // Progressive degradation from fatigue
    if (component_state.fatigue_damage > 100.0f && component_state.structural_integrity > 0.5f) {
        component_state.structural_integrity *= 0.7f; // Sharp drop
        component_state.fatigue_damage = 0.0f; // Reset after failure
    }
    
    // Weld integrity damage from high-energy impacts
    const float WELD_FAILURE_THRESHOLD = 50000.0f; // Joules
    if (impact.damage_energy_joules > WELD_FAILURE_THRESHOLD) {
        component_state.weld_integrity -= 0.2f;
        component_state.weld_integrity = std::max(0.0f, component_state.weld_integrity);
    }
    
    // Set damage flags
    component_state.damage_type_flags |= impact.is_penetrated ? static_cast<uint32_t>(DamageTypeFlag::PENETRATION) : 0u;
    component_state.damage_type_flags |= impact.caused_spalling ? static_cast<uint32_t>(DamageTypeFlag::SPALLING) : 0u;
    
    // Update damage level based on health
    if (component_state.current_health > 0.75f) {
        component_state.damage_level = 0;  // Operational
    } else if (component_state.current_health > 0.5f) {
        component_state.damage_level = 1;  // Light damage
    } else if (component_state.current_health > 0.25f) {
        component_state.damage_level = 2;  // Moderate damage
    } else if (component_state.current_health > 0.0f) {
        component_state.damage_level = 3;  // Heavy damage
    } else {
        component_state.damage_level = 5;  // Destroyed
    }
    
    // Mark as destroyed if health reaches 0
    if (component_state.current_health <= 0.0f) {
        component_state.status_flags |= static_cast<uint32_t>(ComponentStatusFlag::DESTROYED);
    }
    
    // Update timing and statistics
    component_state.last_damage_time = 0.0f; // Would be set to current game time
    component_state.total_damage_received += health_reduction;
    component_state.hit_count++;
    
    // Store impact position and normal
    component_state.damage_position = impact.impact_position;
    component_state.damage_normal = impact.impact_normal;
}

void DamageSystem::apply_damage_to_vehicle(
    EntityID vehicle_entity,
    const BallisticImpactResult& impact,
    VehicleDamageState& vehicle_state
) {
    // Find which component was hit (would need proper component lookup)
    // For now, apply across all components with proximity calculation
    
    for (auto& component : vehicle_state.component_damage_states) {
        // Calculate distance from impact position
        float distance_x = component.damage_position[0] - impact.impact_position[0];
        float distance_y = component.damage_position[1] - impact.impact_position[1];
        float distance_z = component.damage_position[2] - impact.impact_position[2];
        float distance = std::sqrt(distance_x*distance_x + distance_y*distance_y + distance_z*distance_z);
        
        // Apply splash damage if close enough
        if (distance < 5.0f) {  // 5-meter splash radius
            float splash_damage = (1.0f - (distance / 5.0f)) * 0.3f;  // 30% at center
            component.current_health = std::max(0.0f, component.current_health - splash_damage);
        }
    }
    
    // Calculate overall vehicle status
    calculate_vehicle_status(vehicle_state.component_damage_states, vehicle_state);
    
    // Update vehicle cumulative damage
    vehicle_state.cumulative_damage = std::min(1.0f, vehicle_state.cumulative_damage + 
        (impact.damage_energy_joules / 10000000.0f));
    
    // Update center of damage
    vehicle_state.center_of_damage = impact.impact_position;
}

void DamageSystem::process_secondary_effects_cascade(
    const ExplosionEffect& explosion,
    std::vector<DamageEvent>& cascade_events
) {
    // Calculate fragment damage events
    for (uint32_t i = 0; i < explosion.fragmentation_count; ++i) {
        Fragment fragment;
        fragment.mass_kg = explosion.explosive_yield_kg_tnt / explosion.fragmentation_count;
        fragment.impact_energy_joules = (0.5f * fragment.mass_kg * 
            explosion.fragmentation_velocity_ms * explosion.fragmentation_velocity_ms);
        
        DamageEvent fragment_event;
        fragment_event.damage_amount = std::min(1.0f, fragment.impact_energy_joules / 1000000.0f);
        fragment_event.damage_type = static_cast<uint32_t>(DamageTypeFlag::FRAGMENTATION);
        cascade_events.push_back(fragment_event);
    }
}

// ============================================================================
// BALLISTIC CALCULATION FUNCTIONS
// ============================================================================

bool DamageSystem::calculate_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    BallisticImpactResult& result
) {
    // Delegate to ballistics system
    return BallisticsSystem::calculate_penetration(
        projectile, armor, impact_angle_deg, velocity_ms, result
    );
}

bool DamageSystem::simulate_apfsds_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    BallisticImpactResult& result
) {
    return BallisticsSystem::simulate_apfsds_penetration(
        projectile, armor, impact_angle_deg, velocity_ms, result
    );
}

bool DamageSystem::simulate_heat_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float standoff_distance,
    float velocity_ms,
    BallisticImpactResult& result
) {
    return BallisticsSystem::simulate_heat_penetration(
        projectile, armor, standoff_distance, velocity_ms, result
    );
}

bool DamageSystem::simulate_hesh_spalling(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    BallisticImpactResult& result
) {
    return BallisticsSystem::simulate_hesh_spalling(
        projectile, armor, impact_angle_deg, result
    );
}

bool DamageSystem::calculate_ricochet(
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    __m256& ricochet_direction,
    float& ricochet_velocity_ms
) {
    return BallisticsSystem::calculate_ricochet(
        armor, impact_angle_deg, velocity_ms, ricochet_direction, ricochet_velocity_ms
    );
}

void DamageSystem::calculate_spalling_damage(
    const ArmorCharacteristics& armor,
    const BallisticImpactResult& impact,
    std::vector<Fragment>& spall_fragments
) {
    BallisticsSystem::calculate_spalling_damage(armor, impact, spall_fragments);
}

// ============================================================================
// SECONDARY EFFECTS FUNCTIONS
// ============================================================================

void DamageSystem::simulate_fire_propagation(
    const FirePropagationState& fire_state,
    std::vector<FirePropagationNode>& propagation_targets,
    float delta_time
) {
    FireSystem::simulate_fire_propagation(
        fire_state, fire_state.adjacent_components, propagation_targets, delta_time
    );
}

void DamageSystem::calculate_explosion_effects(
    const ExplosionEffect& explosion,
    std::vector<Fragment>& fragments,
    std::vector<DamageEvent>& damage_events
) {
    ExplosionSystem::calculate_explosion_effects(explosion, fragments, damage_events);
}

void DamageSystem::simulate_ammo_cook_off(
    EntityID ammo_storage_entity,
    const ComponentDamageState& ammo_component,
    ExplosionEffect& cookoff_explosion
) {
    std::vector<ExplosionEffect> effects = { cookoff_explosion };
    AmmoCookOffSystem::simulate_ammo_cookoff_cascade(
        ammo_storage_entity,
        ammo_component,
        effects
    );
}

void DamageSystem::simulate_fuel_system_damage(
    EntityID fuel_system_entity,
    const ComponentDamageState& fuel_component,
    const BallisticImpactResult& impact,
    FirePropagationState& fuel_fire
) {
    FireSystem::simulate_fuel_fire(
        fuel_system_entity, fuel_component, impact, fuel_fire
    );
}

// ============================================================================
// COMPONENT AND VEHICLE STATUS FUNCTIONS
// ============================================================================

void DamageSystem::update_component_health(
    ComponentDamageState& component,
    float damage_amount,
    uint32_t damage_type
) {
    float multiplier = calculate_damage_multiplier(damage_type, ComponentType::HULL);
    float actual_damage = damage_amount * multiplier;
    
    component.current_health = std::max(0.0f, component.current_health - actual_damage);
    component.cumulative_stress = std::min(1.0f, component.cumulative_stress + actual_damage);
    
    if (component.current_health <= 0.0f) {
        component.status_flags |= static_cast<uint32_t>(ComponentStatusFlag::DESTROYED);
    }
}

bool DamageSystem::check_critical_failure(
    const ComponentDamageState& component,
    ComponentType type
) {
    if (component.current_health <= 0.0f) {
        return true;
    }
    
    // Component-specific critical thresholds
    switch (type) {
        case ComponentType::ENGINE:
            return component.current_health < 0.2f;
        case ComponentType::TRACK_LEFT:
        case ComponentType::TRACK_RIGHT:
            return component.current_health < 0.1f;
        case ComponentType::GUN_BARREL:
            return component.current_health < 0.3f;
        case ComponentType::AMMO_RACK:
            return component.current_health < 0.5f;  // High sensitivity
        default:
            return component.current_health < 0.0f;
    }
}

void DamageSystem::calculate_vehicle_status(
    const std::vector<ComponentDamageState>& components,
    VehicleDamageState& vehicle_state
) {
    float total_mobility_health = 0.0f;
    int mobility_components = 0;
    float total_firepower_health = 0.0f;
    int firepower_components = 0;
    float total_survivability_health = 0.0f;
    int survivability_components = 0;
    
    vehicle_state.critical_systems_lost = 0;
    vehicle_state.mobility_contributing_damage = 0;
    vehicle_state.firepower_contributing_damage = 0;
    
    for (const auto& comp : components) {
        float health_ratio = comp.current_health;
        
        // Mobility-related components (engine, tracks, suspension)
        if (comp.damage_position[0] != 0 || comp.damage_position[1] != 0 || comp.damage_position[2] != 0) {
            // Simplified component type detection based on naming convention
            // In real implementation, would have component type stored
            total_mobility_health += health_ratio;
            mobility_components++;
            if (health_ratio < 0.5f) {
                vehicle_state.mobility_contributing_damage++;
            }
        }
        
        // Firepower-related (turret, gun)
        if (comp.current_health < 0.5f) {
            total_firepower_health += health_ratio;
            firepower_components++;
            if (health_ratio < 0.2f) {
                vehicle_state.firepower_contributing_damage++;
            }
        }
        
        // Survivability (hull armor, crew compartment)
        total_survivability_health += health_ratio;
        survivability_components++;
        
        // Check for critical failures
        if (comp.status_flags & static_cast<uint32_t>(ComponentStatusFlag::DESTROYED)) {
            vehicle_state.critical_systems_lost++;
        }
    }
    
    // Calculate overall factors
    vehicle_state.overall_mobility = (mobility_components > 0) ? 
        (total_mobility_health / mobility_components) : 1.0f;
    vehicle_state.overall_firepower = (firepower_components > 0) ? 
        (total_firepower_health / firepower_components) : 1.0f;
    vehicle_state.overall_survivability = (survivability_components > 0) ? 
        (total_survivability_health / survivability_components) : 1.0f;
    
    // Update vehicle status flags
    vehicle_state.is_vehicularly_destroyed = (vehicle_state.cumulative_damage >= 1.0f);
    vehicle_state.is_combat_efficient = (vehicle_state.overall_mobility > 0.5f) && 
                                        (vehicle_state.overall_firepower > 0.3f);
    vehicle_state.is_immobilized = (vehicle_state.overall_mobility < 0.1f);
    vehicle_state.is_gun_disabled = (vehicle_state.overall_firepower < 0.1f);
}

// ============================================================================
// CREW DAMAGE FUNCTIONS
// ============================================================================

void DamageSystem::apply_crew_damage_from_vehicle_damage(
    const VehicleDamageState& vehicle_damage,
    std::vector<CrewDamageState>& crew_states
) {
    CrewDamageSystem::apply_crew_damage_from_vehicle_damage(vehicle_damage, crew_states);
}

void DamageSystem::process_crew_injuries(
    CrewDamageState& crew_member,
    float delta_time
) {
    CrewDamageSystem::process_crew_injuries(crew_member, delta_time);
}

// ============================================================================
// SIMD BATCH PROCESSING
// ============================================================================

void DamageSystem::process_ballistic_impacts_batch(
    const std::vector<ProjectileCharacteristics>& projectiles,
    const std::vector<ArmorCharacteristics>& armors,
    const std::vector<ImpactParameters>& impacts,
    std::vector<BallisticImpactResult>& results
) {
    BallisticsSystem::process_ballistic_impacts_batch(
        projectiles, armors, impacts, results
    );
}

// ============================================================================
// PHYSICS INTEGRATION
// ============================================================================

static Vec3 vec3_from_m256(const __m256& v) {
    alignas(32) float temp[8];
    _mm256_store_ps(temp, v);
    return Vec3(static_cast<double>(temp[0]), static_cast<double>(temp[1]), static_cast<double>(temp[2]));
}

void DamageSystem::apply_damage_forces_to_physics(
    EntityID damaged_entity,
    const BallisticImpactResult& impact,
    class PhysicsCore& physics
) {
    PhysicsBody* body = physics.get_body(damaged_entity);
    if (!body) {
        return;
    }

    Vec3 impact_normal = vec3_from_m256(impact.impact_normal);
    double normal_length = impact_normal.magnitude();
    if (normal_length > 1e-6) {
        impact_normal = impact_normal / normal_length;
    } else {
        impact_normal = Vec3(0.0, 0.0, 1.0);
    }

    double impulse_magnitude = std::sqrt(std::max(impact.damage_energy_joules, 0.0f));
    Vec3 impulse = impact_normal * impulse_magnitude;

    Vec3 impact_point = vec3_from_m256(impact.impact_position);
    physics.apply_impulse(damaged_entity, impulse, impact_point);

    if (impact.is_penetrated) {
        double velocity_loss = std::min(0.5, impact.penetration_depth_mm / 1000.0);
        body->velocity = body->velocity * (1.0 - velocity_loss);
    }
}

void DamageSystem::apply_ballistic_impulse(
    const BallisticImpactResult& impact,
    class LocalPhysicsBody& target_body
) {
    Vec3 impact_normal = vec3_from_m256(impact.impact_normal);
    double normal_length = impact_normal.magnitude();
    if (normal_length > 1e-6) {
        impact_normal = impact_normal / normal_length;
    } else {
        impact_normal = Vec3(0.0, 0.0, 1.0);
    }

    float mass_inv = _mm_cvtss_f32(_mm256_castps256_ps128(target_body.aux_data));
    double impulse_magnitude = std::sqrt(std::max(impact.damage_energy_joules, 0.0f));
    Vec3 delta_v = impact_normal * impulse_magnitude * static_cast<double>(mass_inv);

    alignas(32) float current_vel[3];
    local_body_utils::extract_velocity(target_body, current_vel);
    Vec3 new_velocity(current_vel[0] + delta_v.x,
                      current_vel[1] + delta_v.y,
                      current_vel[2] + delta_v.z);

    target_body.vel_vec = _mm256_setr_ps(static_cast<float>(new_velocity.x),
                                         static_cast<float>(new_velocity.y),
                                         static_cast<float>(new_velocity.z),
                                         0.0f,
                                         0.0f, 0.0f, 0.0f, 0.0f);
    local_body_utils::mark_dirty(target_body);
}

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

float DamageSystem::calculate_effective_armor_thickness(
    float base_thickness,
    float impact_angle_deg
) {
    // Apply cosine rule for sloped armor
    float angle_rad = impact_angle_deg * 3.14159f / 180.0f;
    return base_thickness / std::cos(angle_rad);
}

float DamageSystem::calculate_de_marre_penetration(
    float projectile_mass_kg,
    float projectile_velocity_ms,
    float projectile_diameter_mm,
    float armor_hardness_factor
) {
    // De Marre formula: P = m*v^2 / (d*d) / H
    // where P = penetration, m = projectile mass, v = velocity
    // d = diameter, H = armor hardness
    
    float penetration = (projectile_mass_kg * projectile_velocity_ms * projectile_velocity_ms) /
                       (projectile_diameter_mm * projectile_diameter_mm * armor_hardness_factor * 10000.0f);
    
    return penetration;
}

float DamageSystem::calculate_heat_penetration(
    float shaped_charge_diameter_mm,
    float standoff_distance,
    float armor_thickness_mm
) {
    // HEAT penetration increases with standoff distance up to optimal distance
    // Diminishes with excessive standoff
    float optimal_standoff = shaped_charge_diameter_mm * 4.0f;
    
    if (standoff_distance < optimal_standoff) {
        return shaped_charge_diameter_mm * 1.5f;
    } else if (standoff_distance > optimal_standoff * 2.0f) {
        return shaped_charge_diameter_mm * 0.8f;
    } else {
        // Linear reduction beyond optimal standoff
        float reduction = (standoff_distance - optimal_standoff) / optimal_standoff;
        return shaped_charge_diameter_mm * (1.5f - 0.7f * reduction);
    }
}

float DamageSystem::calculate_damage_multiplier(
    uint32_t damage_type,
    ComponentType component_type
) {
    float multiplier = 1.0f;
    
    switch (damage_type) {
        case static_cast<uint32_t>(DamageTypeFlag::PENETRATION):
            multiplier = 1.0f;
            break;
        case static_cast<uint32_t>(DamageTypeFlag::FIRE):
            if (component_type == ComponentType::FUEL_TANK) {
                multiplier = 2.0f;
            } else if (component_type == ComponentType::AMMO_RACK) {
                multiplier = 2.5f;
            }
            break;
        case static_cast<uint32_t>(DamageTypeFlag::EXPLOSION):
            multiplier = 1.8f;
            break;
        case static_cast<uint32_t>(DamageTypeFlag::SPALLING):
            multiplier = 0.7f;
            break;
        default:
            multiplier = 1.0f;
    }
    
    return multiplier;
}

bool DamageSystem::check_fire_initiation(
    const BallisticImpactResult& impact,
    ComponentType component_type
) {
    // Check if impact generates enough heat
    if (impact.heat_generated_joules < 100000.0f) {
        return false;  // Not enough heat
    }
    
    // Component-specific flammability
    switch (component_type) {
        case ComponentType::FUEL_TANK:
            return impact.heat_generated_joules > 50000.0f;
        case ComponentType::AMMO_RACK:
            return impact.heat_generated_joules > 200000.0f;
        default:
            return false;
    }
}

bool DamageSystem::check_ammo_cookoff(
    const ComponentDamageState& ammo_component
) {
    // Cook-off occurs if:
    // 1. Ammo compartment is damaged (health < 0.5)
    // 2. Fire is present
    // 3. Enough time has passed
    
    bool damaged = ammo_component.current_health < 0.5f;
    bool on_fire = (ammo_component.status_flags & static_cast<uint32_t>(ComponentStatusFlag::ON_FIRE)) != 0;
    
    return damaged && on_fire;
}

void DamageSystem::generate_spalling_effects(
    EntityID hit_component_id,
    const BallisticImpactResult& impact,
    std::vector<Fragment>& spall_fragments
) {
    BallisticsSystem::calculate_spalling_damage(
        ArmorCharacteristics{},  // Would pass actual armor data
        impact,
        spall_fragments
    );
}

void DamageSystem::update_component_functionality(
    ComponentDamageState& component,
    ComponentType type
) {
    // Update functionality based on damage level
    switch (component.damage_level) {
        case 0:  // Operational
            component.status_flags &= ~static_cast<uint32_t>(ComponentStatusFlag::DISABLED);
            break;
        case 1:  // Light damage
        case 2:  // Moderate damage
            // Reduced effectiveness but still functional
            break;
        case 3:  // Heavy damage
            component.status_flags |= static_cast<uint32_t>(ComponentStatusFlag::HEAVILY_DAMAGED);
            break;
        case 5:  // Destroyed
            component.status_flags |= static_cast<uint32_t>(ComponentStatusFlag::DESTROYED);
            break;
    }
}

} // namespace physics_core