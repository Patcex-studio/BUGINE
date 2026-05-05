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
// Secondary Effects System Implementation
// Explosions, Fire Propagation, Ammunition Cook-off, and Cascading Damage

#include "physics_core/secondary_effects_system.h"
#include "physics_core/damage_system.h"
#include <algorithm>
#include <cmath>
#include <immintrin.h>
#include <random>

namespace physics_core {

// ============================================================================
// EXPLOSION SYSTEM IMPLEMENTATION
// ============================================================================

void ExplosionSystem::calculate_explosion_effects(
    const ExplosionEffect& explosion,
    std::vector<Fragment>& fragments,
    std::vector<DamageEvent>& damage_events
) {
    // Calculate blast pressure at various distances
    const uint32_t num_blast_samples = 8;
    for (uint32_t i = 1; i <= num_blast_samples; ++i) {
        float distance = (explosion.explosion_radius_m / num_blast_samples) * i;
        float pressure = calculate_blast_pressure(distance, explosion.explosive_yield_kg_tnt);
        float impulse = calculate_blast_impulse(distance, explosion.explosive_yield_kg_tnt);
        
        DamageEvent blast_event;
        blast_event.damage_amount = pressure / 100000.0f;  // Normalize to 0-1
        blast_event.impulse_magnitude = impulse;
        blast_event.damage_type = static_cast<uint32_t>(DamageTypeFlag::EXPLOSION);
        damage_events.push_back(blast_event);
    }
    
    // Calculate fragmentation
    calculate_fragmentation(explosion, fragments);
}

void ExplosionSystem::calculate_blast_wave(
    const ExplosionEffect& explosion,
    std::vector<DamageEvent>& blast_events,
    uint32_t max_affected_components
) {
    // Kingery blast calculation
    float peak_pressure = kingery_blast_pressure(1.0f, explosion.explosive_yield_kg_tnt);
    
    // Generate blast events for affected components
    for (uint32_t i = 0; i < max_affected_components; ++i) {
        DamageEvent event;
        event.explosion_source = explosion.explosion_source;
        event.damage_type = static_cast<uint32_t>(DamageTypeFlag::EXPLOSION);
        event.impact_position = explosion.explosion_center;
        blast_events.push_back(event);
    }
}

void ExplosionSystem::calculate_fragmentation(
    const ExplosionEffect& explosion,
    std::vector<Fragment>& fragments
) {
    // Calculate fragment velocities based on explosive type
    std::vector<float> velocities;
    calculate_fragment_velocity_distribution(
        explosion.yield_joules, 
        explosion.fragmentation_count, 
        velocities
    );
    
    // Calculate fragment masses
    std::vector<float> masses;
    calculate_fragment_mass_distribution(
        explosion.explosive_yield_kg_tnt,
        explosion.fragmentation_count,
        masses
    );
    
    // Calculate ejection angles
    std::vector<__m256> angles;
    calculate_fragment_angles(explosion.fragmentation_count, angles);
    
    // Create fragment structures
    fragments.reserve(explosion.fragmentation_count);
    for (uint32_t i = 0; i < explosion.fragmentation_count; ++i) {
        Fragment frag;
        frag.mass_kg = masses[i];
        frag.velocity[0] = angles[i][0] * velocities[i];
        frag.velocity[1] = angles[i][1] * velocities[i];
        frag.velocity[2] = angles[i][2] * velocities[i];
        frag.impact_energy_joules = 0.5f * frag.mass_kg * velocities[i] * velocities[i];
        fragments.push_back(frag);
    }
}

float ExplosionSystem::calculate_blast_pressure(
    float distance_m,
    float explosive_yield_kg_tnt
) {
    // Pressure decreases with distance cubed from point source
    // P(r) = P0 * (r0/r)^3 where P0 is at reference distance
    if (distance_m < 0.1f) distance_m = 0.1f;  // Avoid division issues
    
    float reference_pressure = 100000.0f;  // 100 kPa at 1m
    float pressure = reference_pressure * explosive_yield_kg_tnt / (distance_m * distance_m * distance_m);
    
    return std::min(pressure, 10000000.0f);  // Cap at 10 MPa
}

float ExplosionSystem::calculate_blast_impulse(
    float distance_m,
    float explosive_yield_kg_tnt
) {
    // Impulse calculation
    float pressure = calculate_blast_pressure(distance_m, explosive_yield_kg_tnt);
    float duration_ms = 1.0f + distance_m;  // Duration increases with distance
    
    return pressure * duration_ms / 1000.0f;
}

float ExplosionSystem::calculate_blast_damage_to_component(
    float blast_pressure_pa,
    float distance_m,
    float component_surface_area_m2
) {
    // Damage based on pressure * area * distance factor
    float pressure_factor = std::min(1.0f, blast_pressure_pa / 1000000.0f);  // Normalize to MPa scale
    float distance_factor = std::max(0.0f, 1.0f - (distance_m / 100.0f));
    
    return pressure_factor * component_surface_area_m2 * distance_factor;
}

void ExplosionSystem::calculate_fragment_trajectories(
    const ExplosionEffect& explosion,
    std::vector<Fragment>& fragments,
    uint32_t fragment_count
) {
    // Similar to calculate_fragmentation
    std::vector<float> velocities;
    calculate_fragment_velocity_distribution(
        explosion.yield_joules,
        fragment_count,
        velocities
    );
}

// Ammunition explosion functions
void ExplosionSystem::calculate_ammo_explosion(
    uint32_t ammo_count,
    float explosive_energy_per_round,
    __m256 explosion_center,
    ExplosionEffect& result
) {
    result.explosion_center = explosion_center;
    result.explosive_yield_kg_tnt = (ammo_count * explosive_energy_per_round) / 4184000.0f;
    result.fragmentation_count = ammo_count * 10;  // ~10 fragments per round
    result.explosion_type = 0;  // AMMO type
}

void ExplosionSystem::calculate_fuel_explosion(
    float fuel_mass_kg,
    __m256 explosion_center,
    ExplosionEffect& result
) {
    result.explosion_center = explosion_center;
    // Fuel explosions: ~44 MJ/kg for typical diesel
    result.explosive_yield_kg_tnt = (fuel_mass_kg * 44000000.0f) / 4184000.0f;
    result.fragmentation_count = 50 + static_cast<uint32_t>(fuel_mass_kg);
    result.explosion_type = 1;  // FUEL type
}

void ExplosionSystem::calculate_shaped_charge_explosion(
    float charge_diameter_mm,
    float charge_mass_kg,
    __m256 explosion_center,
    ExplosionEffect& result
) {
    result.explosion_center = explosion_center;
    result.explosive_yield_kg_tnt = charge_mass_kg * 0.95f;  // Shaped charges ~95% efficiency
    result.fragmentation_count = static_cast<uint32_t>(charge_diameter_mm / 10.0f);
    result.explosion_type = 2;  // SHAPED_CHARGE type
}

void ExplosionSystem::simulate_fragment_propagation(
    const std::vector<Fragment>& fragments,
    std::vector<DamageEvent>& collision_events,
    float delta_time
) {
    for (const auto& fragment : fragments) {
        DamageEvent event;
        event.damage_amount = std::min(1.0f, fragment.impact_energy_joules / 10000000.0f);
        event.damage_type = static_cast<uint32_t>(DamageTypeFlag::FRAGMENTATION);
        collision_events.push_back(event);
    }
}

float ExplosionSystem::calculate_fragment_penetration(
    const Fragment& fragment,
    float armor_thickness_mm,
    float armor_hardness
) {
    // Kinetic energy penetration
    float penetration = (fragment.mass_kg * fragment.penetration_power) / armor_hardness;
    return std::max(0.0f, penetration - armor_thickness_mm);
}

float ExplosionSystem::estimate_fragment_damage(
    const Fragment& fragment
) {
    return std::min(1.0f, fragment.impact_energy_joules / 5000000.0f);
}

// Helper functions
float ExplosionSystem::kingery_blast_pressure(
    float distance_m,
    float explosive_mass_kg
) {
    // Kingery scaling law for blast pressure
    float scaled_distance = distance_m / std::pow(explosive_mass_kg, 1.0f/3.0f);
    
    // Polynomial approximation for pressure coefficient
    float z = scaled_distance;
    if (z > 5.0f) return 0.0f;  // Negligible pressure at large distances
    
    float pressure_psi = 16.4f / (z + 0.1f); // Simplified Kingery formula
    return pressure_psi * 6894.76f;  // Convert PSI to Pa
}

void ExplosionSystem::calculate_fragment_velocity_distribution(
    float total_kinetic_energy,
    uint32_t fragment_count,
    std::vector<float>& velocities
) {
    velocities.resize(fragment_count);
    float average_mass = 1.0f / fragment_count;
    float average_velocity = std::sqrt(2.0f * total_kinetic_energy / average_mass) / fragment_count;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(average_velocity, average_velocity * 0.2f);
    
    for (uint32_t i = 0; i < fragment_count; ++i) {
        velocities[i] = dist(gen);
    }
}

void ExplosionSystem::calculate_fragment_mass_distribution(
    float total_mass,
    uint32_t fragment_count,
    std::vector<float>& masses
) {
    masses.resize(fragment_count);
    float base_mass = total_mass / fragment_count;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(base_mass * 0.5f, base_mass * 1.5f);
    
    float accumulated_mass = 0.0f;
    for (uint32_t i = 0; i < fragment_count - 1; ++i) {
        masses[i] = dist(gen);
        accumulated_mass += masses[i];
    }
    masses[fragment_count - 1] = total_mass - accumulated_mass;
}

void ExplosionSystem::calculate_fragment_angles(
    uint32_t fragment_count,
    std::vector<__m256>& direction_vectors
) {
    direction_vectors.resize(fragment_count);
    
    for (uint32_t i = 0; i < fragment_count; ++i) {
        float theta = (2.0f * 3.14159f * i) / fragment_count;  // Azimuth
        float phi = (3.14159f * (i % 5)) / 5.0f;  // Elevation
        
        direction_vectors[i][0] = std::sin(phi) * std::cos(theta);
        direction_vectors[i][1] = std::sin(phi) * std::sin(theta);
        direction_vectors[i][2] = std::cos(phi);
        direction_vectors[i][3] = 0.0f;
    }
}

// ============================================================================
// FIRE SYSTEM IMPLEMENTATION
// ============================================================================

bool FireSystem::check_fire_ignition(
    const BallisticImpactResult& impact,
    ComponentType component_type,
    const ComponentDamageState& component
) {
    float ignition_prob = calculate_ignition_probability(
        impact.heat_generated_joules,
        1.0f,  // Component flammability
        300.0f  // Ambient temp K
    );
    
    return ignition_prob > 0.5f;
}

float FireSystem::calculate_ignition_probability(
    float impact_energy_joules,
    float component_flammability,
    float ambient_temperature_k
) {
    // Temperature rise from impact energy
    float energy_factor = impact_energy_joules / 1000000.0f;
    float flammability_factor = component_flammability;
    
    // Autoignition if temperature exceeds ignition temperature
    float ignition_prob = (energy_factor * flammability_factor) * 
                         ((373.0f - ambient_temperature_k) / 373.0f);  // Celsius effect
    
    return std::min(1.0f, std::max(0.0f, ignition_prob));
}

void FireSystem::simulate_fire_propagation(
    const FirePropagationState& fire_state,
    const std::vector<EntityID>& component_ids,
    std::vector<FirePropagationNode>& propagation_targets,
    float delta_time
) {
    // Fire spreads to adjacent components
    for (const auto& target_id : component_ids) {
        FirePropagationNode node;
        node.target_component = target_id;
        node.burn_duration = 10.0f + delta_time;
        node.ignition_probability = calculate_ignition_probability(
            fire_state.fire_intensity * 1000000.0f,
            1.0f,
            fire_state.temperature_kelvin
        );
        propagation_targets.push_back(node);
    }
}

float FireSystem::calculate_heat_transfer(
    float source_temperature_k,
    float source_intensity,
    float distance_m,
    float thermal_conductivity
) {
    // Heat transfer by radiation and conduction
    // Q = h * A * (T_hot - T_cold)
    float heat_transfer = source_intensity * source_temperature_k / 
                         (distance_m * distance_m + 0.1f) * thermal_conductivity;
    return heat_transfer;
}

bool FireSystem::calculate_component_ignition(
    const FirePropagationNode& propagation_node,
    float delta_time
) {
    // Simple probabilistic ignition
    return propagation_node.ignition_probability > 0.5f;
}

float FireSystem::calculate_fuel_leak_rate(
    const ComponentDamageState& fuel_tank,
    const BallisticImpactResult& impact
) {
    // Leak rate based on damage and penetration depth
    if (!impact.is_penetrated) return 0.0f;
    
    float damage_factor = 1.0f - fuel_tank.current_health;
    float penetration_area = impact.penetration_depth_mm;
    
    return damage_factor * penetration_area * 0.005f;  // kg/s leak rate
}

void FireSystem::simulate_fuel_fire(
    EntityID fuel_system_entity,
    const ComponentDamageState& fuel_component,
    const BallisticImpactResult& impact,
    FirePropagationState& fuel_fire
) {
    fuel_fire.fire_source = fuel_system_entity;
    fuel_fire.fire_intensity = 0.8f + (impact.heat_generated_joules / 10000000.0f);
    fuel_fire.temperature_kelvin = 800.0f + impact.heat_generated_joules / 1000000.0f;
    fuel_fire.burn_rate = 0.1f;  // m/s spread rate
}

float FireSystem::calculate_fuel_fire_intensity(
    float fuel_leak_rate_kg_s,
    float ambient_oxygen_percent
) {
    float burn_rate = fuel_leak_rate_kg_s * 44.0f;  // 44 MJ/kg fuel
    float oxygen_factor = ambient_oxygen_percent / 21.0f;  // 21% O2 in air
    
    return std::min(1.0f, (burn_rate * oxygen_factor) / 50000.0f);
}

float FireSystem::calculate_suppression_effectiveness(
    float suppression_agent_mass_kg,
    float suppression_agent_type,
    const FirePropagationState& fire_state
) {
    // Effectiveness based on agent mass and fire intensity
    float effectiveness = (suppression_agent_mass_kg * 10.0f) / 
                         (fire_state.fire_intensity * 1000.0f + 1.0f);
    return std::min(1.0f, effectiveness);
}

void FireSystem::simulate_fire_suppression(
    FirePropagationState& fire_state,
    float suppression_effectiveness,
    float delta_time
) {
    float reduction = fire_state.fire_intensity * suppression_effectiveness * delta_time;
    fire_state.fire_intensity = std::max(0.0f, fire_state.fire_intensity - reduction);
}

float FireSystem::calculate_heat_damage(
    float fire_intensity,
    float component_heat_resistance,
    float delta_time
) {
    return fire_intensity * (1.0f / component_heat_resistance) * delta_time * 0.5f;
}

float FireSystem::calculate_smoke_generation(
    float fire_intensity,
    float fuel_burn_rate_kg_s
) {
    // Smoke generation from combustion
    return fire_intensity * fuel_burn_rate_kg_s * 0.1f;
}

float FireSystem::calculate_temperature_increase(
    float fire_intensity,
    float component_volume_m3,
    float delta_time
) {
    // Temperature increase based on fire intensity and volume
    float temp_per_volume = fire_intensity * 1000.0f / (component_volume_m3 + 1.0f);
    return temp_per_volume * delta_time;
}

float FireSystem::get_fuel_flammability(
    uint32_t fuel_type
) {
    switch (fuel_type) {
        case 0:  // Diesel
            return 0.7f;
        case 1:  // Gasoline
            return 0.95f;
        case 2:  // Jet fuel
            return 0.8f;
        default:
            return 0.5f;
    }
}

void FireSystem::get_component_thermal_properties(
    ComponentType type,
    float& heat_capacity,
    float& thermal_conductivity,
    float& heat_resistance
) {
    switch (type) {
        case ComponentType::FUEL_TANK:
            heat_capacity = 2000.0f;
            thermal_conductivity = 50.0f;
            heat_resistance = 0.5f;
            break;
        case ComponentType::AMMO_RACK:
            heat_capacity = 3000.0f;
            thermal_conductivity = 100.0f;
            heat_resistance = 0.3f;
            break;
        default:
            heat_capacity = 1000.0f;
            thermal_conductivity = 30.0f;
            heat_resistance = 1.0f;
    }
}

float FireSystem::calculate_fire_spread_rate(
    float fire_intensity,
    float fuel_availability,
    float oxygen_availability
) {
    return fire_intensity * fuel_availability * oxygen_availability * 0.05f;
}

// ============================================================================
// AMMUNITION COOK-OFF SYSTEM IMPLEMENTATION
// ============================================================================

bool AmmoCookOffSystem::check_ammo_cookoff(
    const ComponentDamageState& ammo_component,
    float current_temperature_k,
    float heat_rate_k_per_s
) {
    if (ammo_component.current_health > 0.7f) return false;  // Not damaged enough
    
    float cookoff_prob = calculate_cookoff_probability(
        current_temperature_k,
        100.0f,  // Assume 100 rounds
        0.0f,    // Generic ammo type
        1.0f     // 1 second exposure
    );
    
    return cookoff_prob > 0.5f;
}

float AmmoCookOffSystem::calculate_cookoff_probability(
    float temperature_k,
    float ammo_count,
    float ammo_type,
    float exposure_time_seconds
) {
    // Initiation temperature for various ammunition
    float init_temp = 450.0f + 50.0f * ammo_type;  // K
    
    if (temperature_k < init_temp) return 0.0f;
    
    float excess_temp = (temperature_k - init_temp) / 100.0f;
    float probability = (excess_temp * excess_temp) * exposure_time_seconds;
    
    return std::min(1.0f, probability * (ammo_count / 100.0f));
}

void AmmoCookOffSystem::simulate_ammo_cookoff_cascade(
    EntityID ammo_storage_entity,
    const ComponentDamageState& ammo_component,
    std::vector<ExplosionEffect>& cookoff_explosions
) {
    // Cascade primary to secondary cook-off
    uint32_t round_count = static_cast<uint32_t>(ammo_component.max_health * 50.0f);
    float current_temp = 300.0f;  // Would get actual component temperature
    
    ExplosionEffect first_explosion;
    calculate_cookoff_explosion(round_count, 0.0f, current_temp, first_explosion);
    cookoff_explosions.push_back(first_explosion);
    
    // Secondary explosions follow
    if (round_count > 50) {
        for (uint32_t i = 1; i < 3; ++i) {
            ExplosionEffect secondary;
            secondary.explosion_delay_ms = 100.0f * i;
            cookoff_explosions.push_back(secondary);
        }
    }
}

void AmmoCookOffSystem::calculate_cookoff_explosion(
    uint32_t ammo_count,
    float ammo_type,
    float ammo_storage_temperature_k,
    ExplosionEffect& explosion
) {
    explosion.explosive_yield_kg_tnt = ammo_count * 0.5f;  // ~0.5 kg TNT per round average
    explosion.fragmentation_count = ammo_count * 8;
    explosion.fragmentation_velocity_ms = 500.0f + (ammo_storage_temperature_k - 300.0f) * 0.1f;
    explosion.explosion_type = 0;  // AMMO type
}

float AmmoCookOffSystem::calculate_cookoff_delay(
    float ammunition_temperature_k,
    float ammo_type
) {
    float init_temp = 450.0f + 50.0f * ammo_type;
    if (ammunition_temperature_k <= init_temp) return 999999.0f;
    
    float excess = ammunition_temperature_k - init_temp;
    return std::max(0.1f, 10.0f - (excess / 100.0f));
}

void AmmoCookOffSystem::simulate_apfsds_cookoff(
    uint32_t round_count,
    float temperature_k,
    ExplosionEffect& explosion
) {
    explosion.explosive_yield_kg_tnt = round_count * 0.6f;
    explosion.fragmentation_count = round_count * 12;
    explosion.fragmentation_velocity_ms = 600.0f;
}

void AmmoCookOffSystem::simulate_heat_cookoff(
    uint32_t round_count,
    float temperature_k,
    ExplosionEffect& explosion
) {
    explosion.explosive_yield_kg_tnt = round_count * 0.8f;
    explosion.fragmentation_count = round_count * 10;
    explosion.fragmentation_velocity_ms = 700.0f;
}

void AmmoCookOffSystem::simulate_projectile_cookoff(
    uint32_t round_count,
    float temperature_k,
    ExplosionEffect& explosion
) {
    explosion.explosive_yield_kg_tnt = round_count * 0.4f;
    explosion.fragmentation_count = round_count * 6;
    explosion.fragmentation_velocity_ms = 400.0f;
}

void AmmoCookOffSystem::get_ammunition_properties(
    float ammo_type,
    float& cook_off_temperature_k,
    float& explosive_mass_kg,
    float& thermal_sensitivity
) {
    cook_off_temperature_k = 450.0f + 50.0f * ammo_type;
    explosive_mass_kg = 0.5f + 0.2f * ammo_type;
    thermal_sensitivity = 0.3f + 0.1f * ammo_type;
}

float AmmoCookOffSystem::calculate_thermal_runaway_probability(
    float temperature_k,
    float heat_rate_k_per_s
) {
    float current_prob = (temperature_k - 300.0f) / 200.0f;
    float rate_factor = heat_rate_k_per_s / 10.0f;
    return std::min(1.0f, current_prob * rate_factor);
}

// ============================================================================
// CASCADING DAMAGE SYSTEM IMPLEMENTATION
// ============================================================================

void CascadingDamageSystem::process_secondary_effects_cascade(
    const ExplosionEffect& explosion,
    const std::vector<ComponentDamageState>& components,
    std::vector<DamageEvent>& cascade_events
) {
    for (const auto& comp : components) {
        float distance_x = comp.damage_position[0] - explosion.explosion_center[0];
        float distance_y = comp.damage_position[1] - explosion.explosion_center[1];
        float distance_z = comp.damage_position[2] - explosion.explosion_center[2];
        alignas(32) float comp_pos[8];
        alignas(32) float explosion_center[8];
        _mm256_store_ps(comp_pos, comp.damage_position);
        _mm256_store_ps(explosion_center, explosion.explosion_center);

        float distance_x = comp_pos[0] - explosion_center[0];
        float distance_y = comp_pos[1] - explosion_center[1];
        float distance_z = comp_pos[2] - explosion_center[2];
        float distance = std::sqrt(distance_x*distance_x + distance_y*distance_y + distance_z*distance_z);
        
        if (distance > explosion.explosion_radius_m) continue;
        
        float damage_factor = 1.0f - (distance / explosion.explosion_radius_m);
        float damage_energy = explosion.explosive_yield_kg_tnt * 4184000.0f * damage_factor;
        
        DamageEvent event;
        event.damage_amount = std::min(1.0f, damage_energy / 10000000.0f);
        event.damage_type = static_cast<uint32_t>(DamageTypeFlag::EXPLOSION);
        cascade_events.push_back(event);
    }
}

float CascadingDamageSystem::calculate_cascade_amplitude(
    const ExplosionEffect& explosion,
    uint32_t cascade_depth
) {
    // Amplitude decreases exponentially with cascade depth
    return explosion.explosive_yield_kg_tnt * std::pow(0.5f, cascade_depth);
}

void CascadingDamageSystem::detect_domino_effects(
    const std::vector<ComponentDamageState>& components,
    const ExplosionEffect& initial_explosion,
    std::vector<uint32_t>& potential_cascade_sequence
) {
    for (uint32_t i = 0; i < components.size(); ++i) {
        float probability = calculate_domino_probability(
            components[0], components[i], initial_explosion.blast_pressure_pa
        );
        
        if (probability > 0.3f) {
            potential_cascade_sequence.push_back(i);
        }
    }
}

float CascadingDamageSystem::calculate_domino_probability(
    const ComponentDamageState& source,
    const ComponentDamageState& target,
    float blast_pressure_pa
) {
    float proximity_factor = 1.0f / (1.0f + 0.1f);  // Would calculate actual distance
    float blast_factor = blast_pressure_pa / 1000000.0f;  // Normalize
    float damage_factor = (1.0f - source.current_health) * (1.0f - target.current_health);
    
    return proximity_factor * blast_factor * damage_factor;
}

bool CascadingDamageSystem::detect_structural_failure(
    const std::vector<ComponentDamageState>& components,
    float total_blast_energy_joules
) {
    float total_structural_damage = 0.0f;
    for (const auto& comp : components) {
        total_structural_damage += 1.0f - comp.structural_integrity;
    }
    
    float average_structural_damage = total_structural_damage / components.size();
    return average_structural_damage > 0.7f;
}

float CascadingDamageSystem::calculate_structural_failure_probability(
    float cumulative_structural_damage,
    float total_blast_pressure_pa,
    uint32_t critical_components_damaged
) {
    float damage_factor = cumulative_structural_damage;
    float pressure_factor = total_blast_pressure_pa / 1000000.0f;
    float critical_factor = critical_components_damaged / 3.0f;
    
    return std::min(1.0f, damage_factor * (1.0f + pressure_factor) * (1.0f + critical_factor));
}

void CascadingDamageSystem::simulate_vehicle_destruction_cascade(
    const std::vector<ComponentDamageState>& components,
    std::vector<DamageEvent>& destruction_events
) {
    for (const auto& comp : components) {
        if (comp.status_flags & static_cast<uint32_t>(ComponentStatusFlag::DESTROYED)) {
            DamageEvent event;
            event.component_id = comp.component_entity;
            event.damage_amount = 1.0f;
            event.damage_type = static_cast<uint32_t>(DamageTypeFlag::EXPLOSION);
            destruction_events.push_back(event);
        }
    }
}

float CascadingDamageSystem::calculate_component_interaction_amplification(
    const ComponentDamageState& source,
    const ComponentDamageState& target
) {
    float source_severity = 1.0f - source.current_health;
    float target_vulnerability = 1.0f - target.current_health;
    
    return source_severity * target_vulnerability * 2.0f;
}

float CascadingDamageSystem::calculate_cascade_propagation_speed(
    float explosion_pressure_pa,
    float distance_m
) {
    float pressure_factor = explosion_pressure_pa / 1000000.0f;
    float propagation_speed = 300.0f * pressure_factor / (distance_m + 1.0f);
    
    return std::max(10.0f, propagation_speed);
}

} // namespace physics_core
