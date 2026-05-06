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
#include <immintrin.h>
#include <memory>
#include "damage_system.h"
#include "types.h"

// Aligned allocator for SIMD types
template <typename T, std::size_t Alignment>
struct AlignedAllocator {
    using value_type = T;
    AlignedAllocator() = default;
    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) {}
    T* allocate(std::size_t n) {
        void* p = std::aligned_alloc(Alignment, n * sizeof(T));
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T* p, std::size_t) { std::free(p); }
};

// Typedef for aligned __m256
using AlignedM256 = __attribute__((aligned(32))) float[8];
using AlignedM256Vector = std::vector<AlignedM256, AlignedAllocator<AlignedM256, 32>>;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

namespace physics_core {

// Forward declarations
struct DamageEvent;
struct ComponentDamageState;
struct VehicleDamageState;
struct Fragment;

// ============================================================================
// EXPLOSION SYSTEM
// ============================================================================

class ExplosionSystem {
public:
    // ========================================================================
    // EXPLOSION CALCULATION FUNCTIONS
    // ========================================================================
    
    // Calculate explosion effects (< 5 ms target for major explosion)
    static void calculate_explosion_effects(
        const ExplosionEffect& explosion,
        std::vector<Fragment>& fragments,
        std::vector<DamageEvent>& damage_events
    );
    
    // Calculate blast wave propagation
    static void calculate_blast_wave(
        const ExplosionEffect& explosion,
        std::vector<DamageEvent>& blast_events,
        uint32_t max_affected_components
    );
    
    // Calculate fragmentation pattern
    static void calculate_fragmentation(
        const ExplosionEffect& explosion,
        std::vector<Fragment>& fragments
    );
    
    // Calculate blast pressure
    static float calculate_blast_pressure(
        float distance_m,
        float explosive_yield_kg_tnt
    );
    
    // Calculate blast impulse
    static float calculate_blast_impulse(
        float distance_m,
        float explosive_yield_kg_tnt
    );
    
    // Calculate damage to component from blast
    static float calculate_blast_damage_to_component(
        float blast_pressure_pa,
        float distance_m,
        float component_surface_area_m2
    );
    
    // Calculate fragment trajectories
    static void calculate_fragment_trajectories(
        const ExplosionEffect& explosion,
        std::vector<Fragment>& fragments,
        uint32_t fragment_count
    );
    
    // ========================================================================
    // AMMUNITION-SPECIFIC EXPLOSIONS
    // ========================================================================
    
    // Ammunition rack explosion
    static void calculate_ammo_explosion(
        uint32_t ammo_count,
        float explosive_energy_per_round,
        __m256 explosion_center,
        ExplosionEffect& result
    );
    
    // Fuel tank explosion
    static void calculate_fuel_explosion(
        float fuel_mass_kg,
        __m256 explosion_center,
        ExplosionEffect& result
    );
    
    // Shaped charge detonation
    static void calculate_shaped_charge_explosion(
        float charge_diameter_mm,
        float charge_mass_kg,
        __m256 explosion_center,
        ExplosionEffect& result
    );
    
    // ========================================================================
    // FRAGMENT SIMULATION
    // ========================================================================
    
    // Advanced fragment modeling
    static void simulate_fragment_propagation(
        const std::vector<Fragment>& fragments,
        std::vector<DamageEvent>& collision_events,
        float delta_time
    );
    
    // Calculate fragment penetration power
    static float calculate_fragment_penetration(
        const Fragment& fragment,
        float armor_thickness_mm,
        float armor_hardness
    );
    
    // Estimate fragment damage
    static float estimate_fragment_damage(
        const Fragment& fragment
    );
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Kingery method for blast calculation
    static float kingery_blast_pressure(
        float distance_m,
        float explosive_mass_kg
    );
    
    // Calculate fragment velocity distribution
    static void calculate_fragment_velocity_distribution(
        float total_kinetic_energy,
        uint32_t fragment_count,
        std::vector<float>& velocities
    );
    
    // Estimate fragment mass distribution
    static void calculate_fragment_mass_distribution(
        float total_mass,
        uint32_t fragment_count,
        std::vector<float>& masses
    );
    
    // Calculate fragment ejection angles
    static void calculate_fragment_angles(
        uint32_t fragment_count,
        AlignedM256Vector& direction_vectors
    );
};

// ============================================================================
// FIRE SYSTEM
// ============================================================================

class FireSystem {
public:
    // ========================================================================
    // FIRE INITIATION
    // ========================================================================
    
    // Check if damage initiates fire
    static bool check_fire_ignition(
        const BallisticImpactResult& impact,
        ComponentType component_type,
        const ComponentDamageState& component
    );
    
    // Calculate fire ignition probability
    static float calculate_ignition_probability(
        float impact_energy_joules,
        float component_flammability,
        float ambient_temperature_k
    );
    
    // ========================================================================
    // FIRE PROPAGATION (< 5 ms per complex cascade)
    // ========================================================================
    
    // Simulate fire propagation between components
    static void simulate_fire_propagation(
        const FirePropagationState& fire_state,
        const std::vector<EntityID>& component_ids,
        std::vector<FirePropagationNode>& propagation_targets,
        float delta_time
    );
    
    // Calculate heat transfer between components
    static float calculate_heat_transfer(
        float source_temperature_k,
        float source_intensity,
        float distance_m,
        float thermal_conductivity
    );
    
    // Calculate component ignition from heat
    static bool calculate_component_ignition(
        const FirePropagationNode& propagation_node,
        float delta_time
    );
    
    // ========================================================================
    // FUEL SYSTEM DAMAGE
    // ========================================================================
    
    // Simulate fuel leak
    static float calculate_fuel_leak_rate(
        const ComponentDamageState& fuel_tank,
        const BallisticImpactResult& impact
    );
    
    // Simulate fuel system fire
    static void simulate_fuel_fire(
        EntityID fuel_system_entity,
        const ComponentDamageState& fuel_component,
        const BallisticImpactResult& impact,
        FirePropagationState& fuel_fire
    );
    
    // Calculate fuel fire intensity
    static float calculate_fuel_fire_intensity(
        float fuel_leak_rate_kg_s,
        float ambient_oxygen_percent
    );
    
    // ========================================================================
    // FIRE SUPPRESSION
    // ========================================================================
    
    // Calculate suppression effectiveness
    static float calculate_suppression_effectiveness(
        float suppression_agent_mass_kg,
        float suppression_agent_type,
        const FirePropagationState& fire_state
    );
    
    // Simulate active fire suppression system
    static void simulate_fire_suppression(
        FirePropagationState& fire_state,
        float suppression_effectiveness,
        float delta_time
    );
    
    // ========================================================================
    // FIRE EFFECTS
    // ========================================================================
    
    // Calculate heat damage to components
    static float calculate_heat_damage(
        float fire_intensity,
        float component_heat_resistance,
        float delta_time
    );
    
    // Calculate smoke generation
    static float calculate_smoke_generation(
        float fire_intensity,
        float fuel_burn_rate_kg_s
    );
    
    // Calculate temperature increase
    static float calculate_temperature_increase(
        float fire_intensity,
        float component_volume_m3,
        float delta_time
    );
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Calculate fuel flammability
    static float get_fuel_flammability(
        uint32_t fuel_type
    );
    
    // Calculate component thermal properties
    static void get_component_thermal_properties(
        ComponentType type,
        float& heat_capacity,
        float& thermal_conductivity,
        float& heat_resistance
    );
    
    // Calculate fire spread rate
    static float calculate_fire_spread_rate(
        float fire_intensity,
        float fuel_availability,
        float oxygen_availability
    );
};

// ============================================================================
// AMMUNITION COOK-OFF SYSTEM
// ============================================================================

class AmmoCookOffSystem {
public:
    // ========================================================================
    // COOK-OFF DETECTION
    // ========================================================================
    
    // Check if ammunition cook-off occurs
    static bool check_ammo_cookoff(
        const ComponentDamageState& ammo_component,
        float current_temperature_k,
        float heat_rate_k_per_s
    );
    
    // Calculate cook-off probability
    static float calculate_cookoff_probability(
        float temperature_k,
        float ammo_count,
        float ammo_type,
        float exposure_time_seconds
    );
    
    // ========================================================================
    // COOK-OFF SIMULATION
    // ========================================================================
    
    // Simulate ammunition cook-off cascade
    static void simulate_ammo_cookoff_cascade(
        EntityID ammo_storage_entity,
        const ComponentDamageState& ammo_component,
        std::vector<ExplosionEffect>& cookoff_explosions
    );
    
    // Calculate cook-off explosion characteristics
    static void calculate_cookoff_explosion(
        uint32_t ammo_count,
        float ammo_type,
        float ammo_storage_temperature_k,
        ExplosionEffect& explosion
    );
    
    // Calculate ammo cook-off delay
    static float calculate_cookoff_delay(
        float ammunition_temperature_k,
        float ammo_type
    );
    
    // ========================================================================
    // AMMUNITION-SPECIFIC COOK-OFF
    // ========================================================================
    
    // APFSDS cook-off
    static void simulate_apfsds_cookoff(
        uint32_t round_count,
        float temperature_k,
        ExplosionEffect& explosion
    );
    
    // HEAT charge cook-off
    static void simulate_heat_cookoff(
        uint32_t round_count,
        float temperature_k,
        ExplosionEffect& explosion
    );
    
    // Projectile cook-off
    static void simulate_projectile_cookoff(
        uint32_t round_count,
        float temperature_k,
        ExplosionEffect& explosion
    );
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Get ammunition thermal properties
    static void get_ammunition_properties(
        float ammo_type,
        float& cook_off_temperature_k,
        float& explosive_mass_kg,
        float& thermal_sensitivity
    );
    
    // Calculate thermal runaway probability
    static float calculate_thermal_runaway_probability(
        float temperature_k,
        float heat_rate_k_per_s
    );
};

// ============================================================================
// CASCADING DAMAGE SYSTEM
// ============================================================================

class CascadingDamageSystem {
public:
    // ========================================================================
    // CASCADE PROCESSING
    // ========================================================================
    
    // Process secondary effects cascade (< 5 ms per major explosion)
    static void process_secondary_effects_cascade(
        const ExplosionEffect& explosion,
        const std::vector<ComponentDamageState>& components,
        std::vector<DamageEvent>& cascade_events
    );
    
    // Calculate cascade amplitude
    static float calculate_cascade_amplitude(
        const ExplosionEffect& explosion,
        uint32_t cascade_depth
    );
    
    // ========================================================================
    // DOMINO EFFECTS
    // ========================================================================
    
    // Detect potential domino effect sequences
    static void detect_domino_effects(
        const std::vector<ComponentDamageState>& components,
        const ExplosionEffect& initial_explosion,
        std::vector<uint32_t>& potential_cascade_sequence
    );
    
    // Calculate domino chain probability
    static float calculate_domino_probability(
        const ComponentDamageState& source,
        const ComponentDamageState& target,
        float blast_pressure_pa
    );
    
    // ========================================================================
    // STRUCTURAL FAILURE CASCADE
    // ========================================================================
    
    // Detect structural failure conditions
    static bool detect_structural_failure(
        const std::vector<ComponentDamageState>& components,
        float total_blast_energy_joules
    );
    
    // Calculate structural failure probability
    static float calculate_structural_failure_probability(
        float cumulative_structural_damage,
        float total_blast_pressure_pa,
        uint32_t critical_components_damaged
    );
    
    // Simulate vehicle destruction cascade
    static void simulate_vehicle_destruction_cascade(
        const std::vector<ComponentDamageState>& components,
        std::vector<DamageEvent>& destruction_events
    );
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Calculate component interaction effects
    static float calculate_component_interaction_amplification(
        const ComponentDamageState& source,
        const ComponentDamageState& target
    );
    
    // Compute cascade propagation speed
    static float calculate_cascade_propagation_speed(
        float explosion_pressure_pa,
        float distance_m
    );
};

#pragma GCC diagnostic pop

} // namespace physics_core
