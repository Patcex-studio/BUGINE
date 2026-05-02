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
#include <cmath>
#include "projectile_properties.h"
#include "vehicle_component.h"
#include "armor_materials.h"
#include "types.h"

namespace physics_core {

// Forward declarations
struct ProjectileCharacteristics;
struct ArmorCharacteristics;
struct BallisticImpactResult;
struct Fragment;

// Impact parameters structure
struct ImpactParameters {
    // Firing characteristics
    float impact_angle;                        // Angle of impact (degrees)
    float impact_velocity;                     // Velocity at impact (m/s)
    float distance_traveled;                   // Distance projectile traveled (m)
    
    // HEAT-specific
    float standoff_distance;                   // For HEAT projectiles (mm)
    
    // Position data
    __m256 impact_position;                    // Impact position (world space)
    __m256 impact_normal;                      // Surface normal at impact
    __m256 impact_velocity_vector;             // Velocity vector at impact
    
    // Time data
    float time_to_impact;                      // Time from firing to impact (ms)
    float flight_time;                         // Flight duration (seconds)
    
    // Environmental factors
    float temperature_celsius;                 // Ambient temperature
    float humidity_percent;                    // Humidity percentage
    float air_pressure_pa;                     // Air pressure
};

// Penetration result (old interface maintained)
struct PenetrationResult {
    bool penetrated;                           // Whether armor was penetrated
    float penetration_depth;                   // How deep it penetrated (mm)
    float residual_energy;                     // Remaining kinetic energy (joules)
    float ricochet_probability;                // Chance of ricochet (0.0-1.0)
    float spall_damage;                        // Secondary spalling damage
    float exit_angle;                          // Angle of exit if penetrated (degrees)
};

// Impact data (old interface maintained)
struct ImpactData {
    float impact_angle;                        // Angle of impact (degrees)
    float impact_velocity;                     // Velocity at impact (m/s)
    float distance_traveled;                   // Distance projectile traveled (m)
    float standoff_distance;                   // For HEAT projectiles (mm)
    __m128 impact_position;                    // Impact position (x,y,z)
    __m128 impact_normal;                      // Surface normal at impact
};

// ============================================================================
// BALLISTICS SYSTEM CLASS
// ============================================================================

class BallisticsSystem {
public:
    // ========================================================================
    // MAIN PENETRATION CALCULATION FUNCTIONS
    // ========================================================================
    
    // Legacy interface - penetration calculation
    static PenetrationResult calculate_penetration(
        const ProjectileProperties& projectile,
        const VehicleComponent& target_component,
        const ImpactData& impact
    );

    static bool calculate_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );

    // Modern interface - full ballistic impact calculation (< 100 μs target)
    static bool calculate_ballistic_impact(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        const ImpactParameters& impact,
        BallisticImpactResult& result
    );
    
    // ========================================================================
    // AMMUNITION-TYPE-SPECIFIC PENETRATION FUNCTIONS
    // ========================================================================
    
    // APFSDS (Armor-Piercing Fin-Stabilized Discarding Sabot)
    // Uses kinetic energy with long-rod penetrator
    static bool simulate_apfsds_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );
    
    // Legacy APFSDS
    static PenetrationResult simulate_apfsds_penetration(
        float velocity, float angle, float armor_thickness, const ArmorMaterial& material
    );

    // HEAT (High-Explosive Anti-Tank)
    // Uses shaped charge to create jet
    static bool simulate_heat_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float standoff_distance,
        float velocity_ms,
        BallisticImpactResult& result
    );

    // Legacy HEAT
    static PenetrationResult simulate_heat_penetration(
        float charge_diameter, float standoff_distance, float armor_thickness, const ArmorMaterial& material
    );

    // HESH (High-Explosive Squash Head)
    // Deforms on impact and spalls armor
    static bool simulate_hesh_spalling(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        BallisticImpactResult& result
    );

    // Legacy HESH
    static PenetrationResult simulate_hesh_spalling(
        float explosive_mass, float armor_thickness, const ArmorMaterial& material
    );

    // Kinetic penetration (generic armor-piercing)
    static bool simulate_kinetic_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );

    // APDS (Armor-Piercing Discarding Sabot)
    static bool simulate_apds_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );

    // ========================================================================
    // RICOCHET AND ANGLE EFFECTS
    // ========================================================================
    
    // Calculate ricochet probability
    static bool calculate_ricochet(
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        __m256& ricochet_direction,
        float& ricochet_velocity_ms
    );
    
    // Calculate critical ricochet angle
    static float calculate_ricochet_angle(
        const ArmorCharacteristics& armor,
        float impact_angle_deg
    );
    
    // ========================================================================
    // SPALLING AND SECONDARY EFFECTS
    // ========================================================================
    
    // Calculate spalling damage and fragments
    static void calculate_spalling_damage(
        const ArmorCharacteristics& armor,
        const BallisticImpactResult& impact,
        std::vector<Fragment>& spall_fragments
    );
    
    // Calculate behind-armor effects
    static void calculate_behind_armor_effects(
        const ArmorCharacteristics& armor,
        const BallisticImpactResult& impact,
        std::vector<Fragment>& effects
    );
    
    // ========================================================================
    // ARMOR INTERACTION
    // ========================================================================
    
    // Handle reactive armor (ERA) activation
    static bool calculate_era_activation(
        const ArmorCharacteristics& armor,
        const BallisticImpactResult& impact,
        BallisticImpactResult& modified_impact
    );
    
    // Model composite armor layers
    static bool calculate_composite_armor_interaction(
        const ArmorCharacteristics& armor,
        float impact_velocity_ms,
        float impact_angle_deg,
        BallisticImpactResult& result
    );
    
    // ========================================================================
    // SIMD BATCH PROCESSING (8 impacts per batch)
    // ========================================================================
    
    // Batch process multiple penetration calculations
    static void calculate_penetration_batch(
        const std::vector<ProjectileProperties>& projectiles,
        const std::vector<VehicleComponent>& components,
        const std::vector<ImpactData>& impacts,
        std::vector<PenetrationResult>& results
    );
    
    // Modern batch processing
    static void process_ballistic_impacts_batch(
        const std::vector<ProjectileCharacteristics>& projectiles,
        const std::vector<ArmorCharacteristics>& armors,
        const std::vector<ImpactParameters>& impacts,
        std::vector<BallisticImpactResult>& results
    );
    
    // ========================================================================
    // PHYSICS CALCULATIONS
    // ========================================================================
    
    // Calculate ballistic coefficient for aerodynamic drag
    static float calculate_ballistic_coefficient(
        const ProjectileCharacteristics& projectile
    );
    
    // Calculate drag during flight
    static float calculate_drag_force(
        const ProjectileCharacteristics& projectile,
        float velocity_ms,
        float air_density_kg_m3
    );
    
    // Estimate velocity loss over distance
    static float calculate_velocity_degradation(
        const ProjectileCharacteristics& projectile,
        float distance_m,
        float air_density_kg_m3
    );
    
    // ========================================================================
    // PERFORMANCE TESTING AND VALIDATION
    // ========================================================================
    
    // Run performance comparison between SIMD and scalar implementations
    static void run_performance_comparison_test();

    // ========================================================================
    // PRIVATE SIMD BATCH PROCESSING FUNCTIONS
    // ========================================================================
    
    // APFSDS batch processing
    static void calculate_penetration_batch_apfsds(
        __m256 velocities, __m256 angles, __m256 thicknesses, 
        __m256 material_factors, __m256 lengths, __m256 densities,
        float* penetrated_mask, float* depth, float* residual_energy
    );
    
    // HEAT batch processing
    static void calculate_penetration_batch_heat(
        __m256 velocities, __m256 standoffs, __m256 thicknesses,
        __m256 material_factors, __m256 charges, __m256 densities,
        float* penetrated_mask, float* depth, float* residual_energy
    );
    
    // HESH batch processing
    static void calculate_penetration_batch_hesh(
        __m256 velocities, __m256 angles, __m256 thicknesses,
        __m256 material_factors, __m256 masses, __m256 densities,
        float* penetrated_mask, float* depth, float* residual_energy
    );

private:
    // ========================================================================
    // PRIVATE HELPER FUNCTIONS
    // ========================================================================
    
    // Calculate effective armor thickness considering angle
    static float calculate_effective_armor_thickness(
        float base_thickness,
        float impact_angle_deg
    );

    // Legacy helper calculations
    static float calculate_ricochet_probability(
        float impact_angle_deg,
        float velocity_ms,
        const ArmorMaterial& material
    );

    static float calculate_spalling_damage(
        float penetration_depth_mm,
        const ArmorMaterial& material
    );
    
    // Calculate penetration capability using De Marre formula
    static float calculate_de_marre_penetration(
        float projectile_mass_kg,
        float projectile_velocity_ms,
        float projectile_diameter_mm,
        float armor_hardness_factor
    );
    
    // Calculate normalized penetration depth
    static float calculate_normalized_penetration(
        float ap_constant,
        float projectile_mass_kg,
        float projectile_diameter_mm,
        float velocity_ms,
        float effective_armor_thickness,
        float armor_hardness
    );
    
    // Calculate HEAT jet penetration depth
    static float calculate_heat_jet_penetration(
        float charge_diameter_mm,
        float standoff_distance_mm,
        float jet_velocity_ms
    );
    
    // Calculate HESH deformation and spall
    static float calculate_hesh_spall_depth(
        float explosive_mass_kg,
        float armor_thickness_mm,
        float armor_density_kg_m3
    );
    
    // Estimate fragments and spalling
    static uint32_t calculate_fragment_count(
        const ArmorCharacteristics& armor,
        float penetration_depth_mm,
        float impact_energy_joules
    );
    
    // Calculate fragment velocities
    static void calculate_fragment_velocities(
        const ArmorCharacteristics& armor,
        float penetration_depth_mm,
        float impact_energy_joules,
        std::vector<float>& fragment_velocities
    );
    
    // Calculate energy transfer
    static float calculate_energy_transfer(
        const ArmorCharacteristics& armor,
        float penetration_depth_mm,
        float impact_energy_joules
    );
    
    // Get armor material properties for calculations
    static void get_effective_material_properties(
        const ArmorCharacteristics& armor,
        float& hardness_factor,
        float& density_kg_m3,
        float& yield_strength
    );
    
    // Apply environmental modifiers (temperature, humidity, etc.)
    static float apply_environmental_modifiers(
        const ImpactParameters& impact,
        const ArmorCharacteristics& armor
    );

    // Multi-layered armor penetration calculation
    static float calculate_penetration_depth(
        const ProjectileCharacteristics& proj,
        const ArmorPack& armor_pack,
        float impact_angle_deg,
        float velocity_ms
    );

    // Hydrodynamic penetration for APFSDS
    static float simulate_hydrodynamic_penetration(
        const ProjectileCharacteristics& proj,
        const ArmorLayer& layer,
        float velocity
    );
};

} // namespace physics_core