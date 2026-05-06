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
#include "physics_core/ballistics_system.h"
#include "physics_core/damage_system.h"
#include "physics_core/simd_config.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace physics_core {

PenetrationResult BallisticsSystem::calculate_penetration(
    const ProjectileProperties& projectile,
    const VehicleComponent& target_component,
    const ImpactData& impact
) {
    PenetrationResult result = {};

    const ArmorMaterial& material = get_armor_material(target_component.material_type);

    // Calculate effective armor thickness
    float effective_thickness = calculate_effective_armor_thickness(
        target_component.armor_thickness, impact.impact_angle
    );

    // Route to specific simulation based on projectile type
    switch (projectile.projectile_type) {
        case ProjectileType::APFSDS:
            result = simulate_apfsds_penetration(
                impact.impact_velocity, impact.impact_angle,
                effective_thickness, material
            );
            break;
        case ProjectileType::HEAT:
            result = simulate_heat_penetration(
                projectile.shaped_charge_diameter, impact.standoff_distance,
                effective_thickness, material
            );
            break;
        case ProjectileType::HESH:
            result = simulate_hesh_spalling(
                projectile.explosive_mass, effective_thickness, material
            );
            break;
        default:
            // Simplified AP penetration for other types
            result = simulate_apfsds_penetration(
                impact.impact_velocity, impact.impact_angle,
                effective_thickness, material
            );
            break;
    }

    // Calculate ricochet probability
    result.ricochet_probability = calculate_ricochet_probability(
        impact.impact_angle, impact.impact_velocity, material
    );

    // If not penetrated, check for ricochet
    if (!result.penetrated && result.ricochet_probability > 0.5f) {
        result.exit_angle = impact.impact_angle + 30.0f; // Simplified bounce
    }

    return result;
}

bool BallisticsSystem::calculate_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_angle_deg = impact_angle_deg;
    result.time_to_impact_ms = 0.0f;

    const float effective_thickness = calculate_effective_armor_thickness(
        armor.thickness_mm, impact_angle_deg
    );

    bool penetrated = false;
    switch (static_cast<ProjectileType>(projectile.projectile_type)) {
        case ProjectileType::APFSDS:
            penetrated = simulate_apfsds_penetration(
                projectile, armor, impact_angle_deg, velocity_ms, result
            );
            break;
        case ProjectileType::HEAT:
            penetrated = simulate_heat_penetration(
                projectile, armor, projectile.shaped_charge_diameter_mm,
                velocity_ms, result
            );
            break;
        case ProjectileType::HESH:
            penetrated = simulate_hesh_spalling(
                projectile, armor, impact_angle_deg, result
            );
            break;
        default:
        {
            float capability = calculate_de_marre_penetration(
                projectile.mass_kg,
                velocity_ms,
                projectile.caliber_mm,
                armor.hardness_rha
            );
            capability *= std::max(0.0f, std::cos(impact_angle_deg * 3.14159f / 180.0f));
            result.penetration_depth_mm = std::min(capability, effective_thickness);
            result.is_penetrated = capability >= effective_thickness;
            result.residual_velocity_ms = result.is_penetrated ? velocity_ms * 0.35f : 0.0f;
            result.damage_energy_joules = 0.5f * projectile.mass_kg * velocity_ms * velocity_ms * 0.001f;
            result.armor_damage_mm = result.penetration_depth_mm * 0.3f;
            result.caused_spalling = result.penetration_depth_mm > 0.0f;
            result.fragment_count = calculate_fragment_count(
                armor, result.penetration_depth_mm, result.damage_energy_joules
            );
            result.fragment_kinetic_energy = result.damage_energy_joules * 0.2f;
            result.fire_probability = 0.05f;
            penetrated = result.is_penetrated;
            break;
        }
    }

    return penetrated;
}

bool BallisticsSystem::calculate_ballistic_impact(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    const ImpactParameters& impact,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_position = impact.impact_position;
    result.impact_normal = impact.impact_normal;
    result.impact_angle_deg = impact.impact_angle;
    result.time_to_impact_ms = impact.time_to_impact;

    bool penetrated = calculate_penetration(
        projectile, armor, impact.impact_angle, impact.impact_velocity, result
    );

    if (!penetrated) {
        result.is_ricocheted = calculate_ricochet(
            armor, impact.impact_angle, impact.impact_velocity,
            result.exit_normal, result.residual_velocity_ms
        );
    }

    return penetrated;
}

bool BallisticsSystem::simulate_apfsds_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_angle_deg = impact_angle_deg;
    result.time_to_impact_ms = 0.0f;

    float effective_thickness = calculate_effective_armor_thickness(
        armor.thickness_mm, impact_angle_deg
    );
    float penetration_capability = calculate_de_marre_penetration(
        projectile.mass_kg,
        velocity_ms,
        projectile.caliber_mm,
        armor.hardness_rha
    );
    penetration_capability *= std::max(0.0f, std::cos(impact_angle_deg * 3.14159f / 180.0f));

    result.penetration_depth_mm = std::min(penetration_capability, effective_thickness);
    result.is_penetrated = penetration_capability >= effective_thickness;
    result.residual_velocity_ms = result.is_penetrated ? velocity_ms * 0.35f : 0.0f;
    result.damage_energy_joules = 0.5f * projectile.mass_kg * velocity_ms * velocity_ms * 0.001f;
    result.armor_damage_mm = result.penetration_depth_mm * 0.4f;
    result.caused_spalling = result.penetration_depth_mm > 0.0f;
    result.fragment_count = calculate_fragment_count(
        armor, result.penetration_depth_mm, result.damage_energy_joules
    );
    result.fragment_kinetic_energy = result.damage_energy_joules * 0.2f;
    result.fire_probability = 0.02f;

    return result.is_penetrated;
}

bool BallisticsSystem::simulate_heat_penetration(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float standoff_distance,
    float velocity_ms,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_angle_deg = 0.0f;
    result.time_to_impact_ms = 0.0f;

    float penetration_capability = calculate_heat_jet_penetration(
        projectile.shaped_charge_diameter_mm,
        standoff_distance,
        velocity_ms
    );
    float effective_thickness = calculate_effective_armor_thickness(
        armor.thickness_mm, 0.0f
    );

    result.penetration_depth_mm = std::min(penetration_capability, effective_thickness);
    result.is_penetrated = penetration_capability >= effective_thickness;
    result.residual_velocity_ms = result.is_penetrated ? velocity_ms * 0.15f : 0.0f;
    result.damage_energy_joules = projectile.explosive_mass_kg * 4184.0f;
    result.armor_damage_mm = result.penetration_depth_mm * 0.3f;
    result.caused_spalling = false;
    result.fragment_count = calculate_fragment_count(
        armor, result.penetration_depth_mm, result.damage_energy_joules
    );
    result.fragment_kinetic_energy = result.damage_energy_joules * 0.1f;
    result.fire_probability = 0.08f;

    return result.is_penetrated;
}

bool BallisticsSystem::simulate_hesh_spalling(
    const ProjectileCharacteristics& projectile,
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_angle_deg = impact_angle_deg;
    result.time_to_impact_ms = 0.0f;

    result.penetration_depth_mm = calculate_hesh_spall_depth(
        projectile.explosive_mass_kg,
        armor.thickness_mm,
        armor.density_kg_m3
    );
    result.is_penetrated = false;
    result.caused_spalling = true;
    result.spall_velocity_ms = std::sqrt(std::max(0.0f, projectile.explosive_mass_kg * 1000.0f)) * 12.0f;
    result.damage_energy_joules = projectile.explosive_mass_kg * 4184.0f;
    result.armor_damage_mm = result.penetration_depth_mm * 0.1f;
    result.fragment_count = calculate_fragment_count(
        armor, result.penetration_depth_mm, result.damage_energy_joules
    );
    result.fragment_kinetic_energy = result.damage_energy_joules * 0.12f;
    result.fire_probability = 0.12f;

    return result.caused_spalling;
}

bool BallisticsSystem::calculate_ricochet(
    const ArmorCharacteristics& armor,
    float impact_angle_deg,
    float velocity_ms,
    __m256& ricochet_direction,
    float& ricochet_velocity_ms
) {
    const ArmorMaterial& material = get_armor_material(
        static_cast<ArmorType>(armor.armor_type)
    );

    float probability = calculate_ricochet_probability(
        impact_angle_deg, velocity_ms, material
    );
    bool ricochet = probability > 0.5f;

    ricochet_direction = _mm256_set_ps(
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    );
    ricochet_velocity_ms = ricochet ? velocity_ms * 0.25f : 0.0f;

    return ricochet;
}

void BallisticsSystem::process_ballistic_impacts_batch(
    const std::vector<ProjectileCharacteristics>& projectiles,
    const std::vector<ArmorCharacteristics>& armors,
    const std::vector<ImpactParameters>& impacts,
    std::vector<BallisticImpactResult>& results
) {
    size_t count = std::min({projectiles.size(), armors.size(), impacts.size()});
    results.resize(count);

    for (size_t i = 0; i < count; ++i) {
        calculate_ballistic_impact(projectiles[i], armors[i], impacts[i], results[i]);
    }
}

PenetrationResult BallisticsSystem::simulate_apfsds_penetration(
    float velocity, float angle, float armor_thickness, const ArmorMaterial& material
) {
    PenetrationResult result = {};

    // Simplified De Marre formula for long-rod penetration
    // P = (L/D) * sqrt(mass) * velocity / (K * armor_hardness)
    float l_over_d = 10.0f; // Assume L/D ratio for modern APFSDS
    float k_factor = 2400.0f; // Empirical constant

    float penetration_capability = (l_over_d * sqrt(4.0f) * velocity) /
                                   (k_factor * material.hardness_rha);

    // Apply angle effects
    float angle_factor = cos(angle * 3.14159f / 180.0f);
    penetration_capability *= angle_factor;

    result.penetrated = penetration_capability >= armor_thickness;
    result.penetration_depth = std::min(penetration_capability, armor_thickness);
    result.residual_energy = result.penetrated ?
        (penetration_capability - armor_thickness) / penetration_capability : 0.0f;

    result.spall_damage = calculate_spalling_damage(result.penetration_depth, material);

    return result;
}

PenetrationResult BallisticsSystem::simulate_heat_penetration(
    float charge_diameter, float standoff_distance, float armor_thickness, const ArmorMaterial& material
) {
    PenetrationResult result = {};

    // Simplified HEAT penetration formula
    // P = K * D^(2/3) / (1 + (S/D)^2)^(1/3)
    // Where K is empirical constant, D is diameter, S is standoff
    float k_factor = 2.5f; // Empirical constant for copper liner
    float s_over_d = standoff_distance / charge_diameter;

    float penetration_capability = k_factor * pow(charge_diameter, 2.0f/3.0f) /
                                   pow(1.0f + s_over_d * s_over_d, 1.0f/3.0f);

    // HEAT less affected by armor hardness but more by reactive armor
    if (material.heat_resistance > 1.0f) {
        penetration_capability *= (1.0f / material.heat_resistance);
    }

    result.penetrated = penetration_capability >= armor_thickness;
    result.penetration_depth = std::min(penetration_capability, armor_thickness);
    result.residual_energy = 0.0f; // HEAT doesn't have residual KE

    return result;
}

PenetrationResult BallisticsSystem::simulate_hesh_spalling(
    float explosive_mass, float armor_thickness, const ArmorMaterial& material
) {
    PenetrationResult result = {};

    // HESH creates spalling rather than direct penetration
    // Effectiveness based on explosive mass and armor properties
    float spall_factor = explosive_mass * material.spall_coefficient * 1000.0f;

    result.penetrated = spall_factor >= armor_thickness;
    result.penetration_depth = spall_factor;
    result.spall_damage = spall_factor * 2.0f; // Heavy spalling damage

    return result;
}

void BallisticsSystem::calculate_penetration_batch(
    const std::vector<ProjectileProperties>& projectiles,
    const std::vector<VehicleComponent>& components,
    const std::vector<ImpactData>& impacts,
    std::vector<PenetrationResult>& results
) {
    // True AVX2 batch processing for 8 simultaneous calculations
    const size_t batch_size = 8;
    results.resize(projectiles.size());

    // SoA structures for SIMD processing (32-byte aligned)
    alignas(32) float velocity[8];
    alignas(32) float angle[8];
    alignas(32) float armor_thickness[8];
    alignas(32) float hardness[8];
    alignas(32) float spall_coeff[8];
    alignas(32) float projectile_type[8]; // 0=APFSDS, 1=HEAT, 2=HESH, 3=other
    // New: projectile-specific parameters
    alignas(32) float proj_mass[8];
    alignas(32) float proj_caliber[8];
    alignas(32) float proj_length[8];
    alignas(32) float proj_charge_diam[8];
    alignas(32) float proj_explosive_mass[8];

    for (size_t i = 0; i < projectiles.size(); i += batch_size) {
        size_t current_batch = std::min(batch_size, projectiles.size() - i);

        // Prepare SoA data for this batch
        for (size_t j = 0; j < current_batch; ++j) {
            size_t idx = i + j;
            velocity[j] = impacts[idx].impact_velocity;
            angle[j] = impacts[idx].impact_angle;
            armor_thickness[j] = calculate_effective_armor_thickness(
                components[idx].armor_thickness, impacts[idx].impact_angle);
            
            const ArmorMaterial& mat = get_armor_material(components[idx].material_type);
            hardness[j] = mat.hardness_rha;
            spall_coeff[j] = mat.spall_coefficient;
            projectile_type[j] = static_cast<float>(projectiles[idx].projectile_type);
            proj_mass[j] = projectiles[idx].mass;
            proj_caliber[j] = projectiles[idx].caliber;
            proj_length[j] = projectiles[idx].penetrator_length;
            proj_charge_diam[j] = projectiles[idx].shaped_charge_diameter;
            proj_explosive_mass[j] = projectiles[idx].explosive_mass;
        }

        // Fill remaining slots with realistic defaults for SIMD processing
        for (size_t j = current_batch; j < batch_size; ++j) {
            velocity[j] = 0.0f;
            angle[j] = 0.0f;
            armor_thickness[j] = 1.0f;
            hardness[j] = 2400.0f;
            spall_coeff[j] = 0.5f;
            projectile_type[j] = 0.0f;
            proj_mass[j] = 4.0f;
            proj_caliber[j] = 120.0f;
            proj_length[j] = 600.0f;
            proj_charge_diam[j] = 100.0f;
            proj_explosive_mass[j] = 1.5f;
        }

        // Load into AVX2 registers
        __m256 vel_v = _mm256_load_ps(velocity);
        __m256 ang_v = _mm256_load_ps(angle);
        __m256 armor_v = _mm256_load_ps(armor_thickness);
        __m256 hard_v = _mm256_load_ps(hardness);
        __m256 spall_v = _mm256_load_ps(spall_coeff);
        __m256 type_v = _mm256_load_ps(projectile_type);
        __m256 mass_v = _mm256_load_ps(proj_mass);
        __m256 cal_v = _mm256_load_ps(proj_caliber);
        __m256 len_v = _mm256_load_ps(proj_length);
        __m256 charge_v = _mm256_load_ps(proj_charge_diam);
        __m256 exp_v = _mm256_load_ps(proj_explosive_mass);

        // Output arrays
        alignas(32) float penetrated_mask[8];
        alignas(32) float depth[8];
        alignas(32) float residual_energy[8];

        // Initialize output arrays for this batch
        for (size_t j = 0; j < 8; ++j) {
            penetrated_mask[j] = 0.0f;
            depth[j] = 0.0f;
            residual_energy[j] = 0.0f;
        }

        // Process APFSDS projectiles (type == 0)
        __m256 apfsds_mask = _mm256_cmp_ps(type_v, _mm256_set1_ps(0.0f), _CMP_EQ_OQ);
        if (_mm256_movemask_ps(apfsds_mask) != 0) {
            calculate_penetration_batch_apfsds(
                vel_v, ang_v, armor_v, hard_v, spall_v, apfsds_mask,
                penetrated_mask, depth, residual_energy);
        }

        // Process HEAT projectiles (type == 1)
        __m256 heat_mask = _mm256_cmp_ps(type_v, _mm256_set1_ps(1.0f), _CMP_EQ_OQ);
        if (_mm256_movemask_ps(heat_mask) != 0) {
            calculate_penetration_batch_heat(
                vel_v, ang_v, armor_v, hard_v, spall_v, heat_mask,
                penetrated_mask, depth, residual_energy);
        }

        // Process HESH projectiles (type == 2)
        __m256 hesh_mask = _mm256_cmp_ps(type_v, _mm256_set1_ps(2.0f), _CMP_EQ_OQ);
        if (_mm256_movemask_ps(hesh_mask) != 0) {
            calculate_penetration_batch_hesh(
                vel_v, ang_v, armor_v, hard_v, spall_v, hesh_mask,
                penetrated_mask, depth, residual_energy);
        }

        // Store results back to output vector
        for (size_t j = 0; j < current_batch; ++j) {
            size_t idx = i + j;
            results[idx].penetrated = penetrated_mask[j] > 0.5f;
            results[idx].penetration_depth = depth[j];
            results[idx].residual_energy = residual_energy[j];
            results[idx].spall_damage = depth[j] * spall_coeff[j] * 0.5f;
            
            // Ricochet calculation
            results[idx].ricochet_probability = calculate_ricochet_probability(
                impacts[idx].impact_angle, impacts[idx].impact_velocity, 
                get_armor_material(components[idx].material_type));
        }
    }
}

void BallisticsSystem::calculate_penetration_batch_apfsds(
    __m256 velocity, __m256 angle, __m256 armor, __m256 hardness, __m256 spall_coeff, __m256 mask,
    float* penetrated_mask, float* depth, float* residual_energy
) {
    // Simple SIMD APFSDS model: penetration ∝ velocity / hardness
    __m256 base_penetration = _mm256_div_ps(
        _mm256_mul_ps(_mm256_set1_ps(10.0f), _mm256_sqrt_ps(velocity)),
        _mm256_mul_ps(_mm256_set1_ps(1.0f), hardness)
    );

    __m256 angle_rad = _mm256_mul_ps(angle, _mm256_set1_ps(3.1415927f / 180.0f));
    __m256 angle_factor = _mm256_sub_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(angle_rad, angle_rad));
    angle_factor = _mm256_max_ps(angle_factor, _mm256_set1_ps(0.25f));

    __m256 penetration = _mm256_mul_ps(base_penetration, angle_factor);
    __m256 penetrated = _mm256_cmp_ps(penetration, armor, _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    __m256 calc_depth = _mm256_min_ps(penetration, armor);
    __m256 calc_residual = _mm256_max_ps(_mm256_sub_ps(penetration, armor), _mm256_set1_ps(0.0f));

    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_depth, mask));
    _mm256_store_ps(residual_energy, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_residual, mask));
}

void BallisticsSystem::calculate_penetration_batch_heat(
    __m256 velocity, __m256 angle, __m256 armor, __m256 hardness, __m256 spall_coeff, __m256 mask,
    float* penetrated_mask, float* depth, float* residual_energy
) {
    // Simplified HEAT: penetration depends on charge effectiveness and angle
    __m256 k_factor = _mm256_set1_ps(2.5f);
    __m256 charge_diameter = _mm256_set1_ps(0.08f); // 80mm effective charge diameter
    __m256 standoff = _mm256_set1_ps(0.3f);        // 300mm standoff

    __m256 d_power = _mm256_mul_ps(charge_diameter, _mm256_sqrt_ps(charge_diameter));
    __m256 s_over_d = _mm256_div_ps(standoff, _mm256_add_ps(charge_diameter, _mm256_set1_ps(1e-6f)));
    __m256 denom = _mm256_add_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(s_over_d, s_over_d));
    __m256 denom_root = _mm256_sqrt_ps(denom);
    __m256 penetration = _mm256_div_ps(_mm256_mul_ps(k_factor, d_power), _mm256_mul_ps(denom_root, _mm256_set1_ps(1.0f)));

    __m256 pi = _mm256_set1_ps(3.141592653589793f);
    __m256 angle_rad = _mm256_mul_ps(angle, _mm256_div_ps(pi, _mm256_set1_ps(180.0f)));
    __m256 cos_approx = _mm256_sub_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(angle_rad, angle_rad));
    __m256 angle_factor = _mm256_max_ps(cos_approx, _mm256_set1_ps(0.25f));
    penetration = _mm256_mul_ps(penetration, angle_factor);

    __m256 penetrated = _mm256_cmp_ps(penetration, armor, _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    __m256 calc_depth = _mm256_min_ps(penetration, armor);
    __m256 calc_residual = _mm256_mul_ps(_mm256_sub_ps(penetration, armor), _mm256_set1_ps(0.2f));
    calc_residual = _mm256_max_ps(calc_residual, _mm256_set1_ps(0.0f));

    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_depth, mask));
    _mm256_store_ps(residual_energy, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_residual, mask));
}

void BallisticsSystem::calculate_penetration_batch_hesh(
    __m256 velocity, __m256 angle, __m256 armor, __m256 hardness, __m256 spall_coeff, __m256 mask,
    float* penetrated_mask, float* depth, float* residual_energy
) {
    // Simplified HESH handling: spalling and low penetration
    __m256 explosive_mass = _mm256_set1_ps(1.0f); // Assume 1kg explosive
    __m256 spall_factor = _mm256_mul_ps(explosive_mass, spall_coeff);

    __m256 spall_depth = _mm256_mul_ps(spall_factor, _mm256_set1_ps(0.2f));
    __m256 penetrated = _mm256_cmp_ps(spall_depth, _mm256_mul_ps(armor, _mm256_set1_ps(0.5f)), _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    __m256 calc_depth = _mm256_min_ps(spall_depth, armor);
    __m256 calc_residual = _mm256_set1_ps(0.0f);

    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_depth, mask));
    _mm256_store_ps(residual_energy, _mm256_blendv_ps(_mm256_set1_ps(0.0f), calc_residual, mask));
}

void BallisticsSystem::run_performance_comparison_test() {
    // Generate test data
    const size_t test_size = 1000;
    std::vector<ProjectileProperties> projectiles(test_size);
    std::vector<VehicleComponent> components(test_size);
    std::vector<ImpactData> impacts(test_size);
    
    // Initialize test data with random values
    std::srand(42); // Fixed seed for reproducible results
    for (size_t i = 0; i < test_size; ++i) {
        projectiles[i].projectile_type = static_cast<ProjectileType>(std::rand() % 4);
        components[i].armor_thickness = 50.0f + std::rand() % 200; // 50-250mm
        components[i].material_type = ArmorType::RHA; // Steel
        impacts[i].impact_velocity = 800.0f + std::rand() % 1200; // 800-2000 m/s
        impacts[i].impact_angle = 0.0f + (std::rand() % 60); // 0-60 degrees
    }
    
    // Time scalar version
    auto start_scalar = std::chrono::high_resolution_clock::now();
    std::vector<PenetrationResult> scalar_results(test_size);
    for (size_t i = 0; i < test_size; ++i) {
        scalar_results[i] = calculate_penetration(projectiles[i], components[i], impacts[i]);
    }
    auto end_scalar = std::chrono::high_resolution_clock::now();
    auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end_scalar - start_scalar).count();
    
    // Time SIMD version
    auto start_simd = std::chrono::high_resolution_clock::now();
    std::vector<PenetrationResult> simd_results(test_size);
    calculate_penetration_batch(projectiles, components, impacts, simd_results);
    auto end_simd = std::chrono::high_resolution_clock::now();
    auto simd_time = std::chrono::duration_cast<std::chrono::microseconds>(end_simd - start_simd).count();
    
    // Compare results for accuracy
    float max_error = 0.0f;
    for (size_t i = 0; i < test_size; ++i) {
        float depth_error = std::abs(scalar_results[i].penetration_depth - simd_results[i].penetration_depth);
        max_error = std::max(max_error, depth_error);
        
        // Check if penetration results match
        if (scalar_results[i].penetrated != simd_results[i].penetrated) {
            std::cout << "Penetration mismatch at index " << i << std::endl;
        }
    }
    
    // Output results
    std::cout << "Ballistics Performance Comparison:" << std::endl;
    std::cout << "Test size: " << test_size << " calculations" << std::endl;
    std::cout << "Scalar time: " << scalar_time << " microseconds" << std::endl;
    std::cout << "SIMD time: " << simd_time << " microseconds" << std::endl;
    std::cout << "Speedup: " << static_cast<float>(scalar_time) / simd_time << "x" << std::endl;
    std::cout << "Max depth error: " << max_error << " mm" << std::endl;
    std::cout << "Accuracy: " << (max_error < 0.1f ? "PASS" : "FAIL") << std::endl;
}

float BallisticsSystem::calculate_effective_armor_thickness(float base_thickness, float angle) {
    return base_thickness / cos(angle * 3.14159f / 180.0f);
}

float BallisticsSystem::calculate_ricochet_probability(float angle, float velocity, const ArmorMaterial& material) {
    if (angle >= material.ricochet_threshold) {
        return 1.0f - (angle - material.ricochet_threshold) / 30.0f;
    }
    return 0.0f;
}

float BallisticsSystem::calculate_spalling_damage(float penetration_depth, const ArmorMaterial& material) {
    return penetration_depth * material.spall_coefficient * 0.5f;
}

void BallisticsSystem::calculate_spalling_damage(
    const ArmorCharacteristics& armor,
    const BallisticImpactResult& impact,
    std::vector<Fragment>& spall_fragments
) {
    if (!impact.caused_spalling) {
        return;
    }

    Fragment frag{};
    frag.velocity = _mm256_set1_ps(impact.spall_velocity_ms);
    frag.mass_kg = std::max(0.01f, impact.penetration_depth_mm * 0.001f);
    frag.penetration_power = std::min(1.0f, impact.damage_energy_joules / 10000.0f);
    frag.target_component = 0;
    frag.time_to_target = std::max(1.0f, impact.time_to_impact_ms * 0.1f);
    frag.impact_energy_joules = impact.damage_energy_joules * 0.2f;

    spall_fragments.push_back(frag);
}

float BallisticsSystem::calculate_de_marre_penetration(
    float projectile_mass_kg,
    float projectile_velocity_ms,
    float projectile_diameter_mm,
    float armor_hardness_factor
) {
    const float l_over_d = 10.0f; // typical L/D for APFSDS
    const float k_constant = 2400.0f;
    return (l_over_d * std::sqrt(projectile_mass_kg) * projectile_velocity_ms) /
           (k_constant * armor_hardness_factor);
}

uint32_t BallisticsSystem::calculate_fragment_count(
    const ArmorCharacteristics& armor,
    float penetration_depth_mm,
    float impact_energy_joules
) {
    if (impact_energy_joules <= 0.0f) {
        return 0;
    }

    const float energy_per_fragment = 500.0f; // joules
    return static_cast<uint32_t>(impact_energy_joules / energy_per_fragment) + 1;
}

float BallisticsSystem::calculate_heat_jet_penetration(
    float charge_diameter_mm,
    float standoff_distance_mm,
    float jet_velocity_ms
) {
    const float k_factor = 2.5f;
    float s_over_d = standoff_distance_mm / charge_diameter_mm;
    float penetration = k_factor * std::pow(charge_diameter_mm, 2.0f / 3.0f) /
                        std::pow(1.0f + s_over_d * s_over_d, 1.0f / 3.0f);
    return penetration;
}

float BallisticsSystem::calculate_hesh_spall_depth(
    float explosive_mass_kg,
    float armor_thickness_mm,
    float armor_density_kg_m3
) {
    const float k_spall = 0.15f;
    return k_spall * explosive_mass_kg * std::sqrt(armor_thickness_mm / armor_density_kg_m3) * 1000.0f;
}

float BallisticsSystem::calculate_normalized_penetration(
    float ap_constant,
    float projectile_mass_kg,
    float projectile_diameter_mm,
    float velocity_ms,
    float effective_armor_thickness,
    float armor_hardness
) {
    if (effective_armor_thickness <= 0.0f) {
        return 0.0f;
    }
    float P = ap_constant * (std::sqrt(projectile_mass_kg) * velocity_ms) /
              (projectile_diameter_mm * std::sqrt(armor_hardness));
    return P / effective_armor_thickness;
}

float BallisticsSystem::calculate_ricochet_angle(
    const ArmorCharacteristics& armor,
    float impact_angle_deg
) {
    float critical = armor.ricochet_threshold + 10.0f * (1.0f - armor.hardness_rha / 4000.0f);
    return (impact_angle_deg >= critical) ? impact_angle_deg : 0.0f;
}

void BallisticsSystem::calculate_behind_armor_effects(
    const ArmorCharacteristics& armor,
    const BallisticImpactResult& impact,
    std::vector<Fragment>& effects
) {
    if (!impact.is_penetrated) {
        return;
    }

    uint32_t count = calculate_fragment_count(armor, impact.penetration_depth_mm, impact.damage_energy_joules);
    if (count == 0) {
        return;
    }

    effects.reserve(effects.size() + count);
    for (uint32_t i = 0; i < count; ++i) {
        Fragment frag{};
        frag.mass_kg = 0.01f;
        frag.velocity = _mm256_set1_ps(impact.residual_velocity_ms * 0.5f);
        frag.penetration_power = 0.2f;
        frag.target_component = 0;
        frag.time_to_target = 1.0f + i * 0.5f;
        frag.impact_energy_joules = impact.damage_energy_joules * 0.1f / static_cast<float>(count);
        effects.push_back(frag);
    }
}

bool BallisticsSystem::calculate_era_activation(
    const ArmorCharacteristics& armor,
    const BallisticImpactResult& impact,
    BallisticImpactResult& modified_impact
) {
    if (armor.reactive_coverage_percent <= 0.0f) {
        return false;
    }
    if (impact.damage_energy_joules < armor.reactive_activation_threshold) {
        return false;
    }
    modified_impact = impact;
    modified_impact.penetration_depth_mm *= 0.7f;
    modified_impact.activated_era = true;
    return true;
}

bool BallisticsSystem::calculate_composite_armor_interaction(
    const ArmorCharacteristics& armor,
    float impact_velocity_ms,
    float impact_angle_deg,
    BallisticImpactResult& result
) {
    std::memset(&result, 0, sizeof(result));
    result.impact_angle_deg = impact_angle_deg;

    float effective_thickness = armor.thickness_mm / std::cos(impact_angle_deg * 3.14159f / 180.0f);
    if (armor.composite_layers > 1) {
        impact_velocity_ms *= 0.8f;
    }

    float penetration = calculate_de_marre_penetration(
        4.0f,
        impact_velocity_ms,
        20.0f,
        armor.hardness_rha
    );

    result.is_penetrated = penetration >= effective_thickness;
    result.penetration_depth_mm = std::min(penetration, effective_thickness);
    result.residual_velocity_ms = result.is_penetrated ? impact_velocity_ms * 0.3f : 0.0f;
    result.damage_energy_joules = 0.5f * 4.0f * impact_velocity_ms * impact_velocity_ms * 0.001f;
    result.armor_damage_mm = result.penetration_depth_mm * 0.25f;
    result.caused_spalling = result.is_penetrated;
    return result.is_penetrated;
}

float BallisticsSystem::calculate_ballistic_coefficient(
    const ProjectileCharacteristics& projectile
) {
    if (projectile.caliber_mm <= 0.0f || projectile.drag_coefficient <= 0.0f) {
        return 0.0f;
    }
    return projectile.mass_kg / (projectile.caliber_mm * projectile.caliber_mm * projectile.drag_coefficient);
}

float BallisticsSystem::calculate_drag_force(
    const ProjectileCharacteristics& projectile,
    float velocity_ms,
    float air_density_kg_m3
) {
    float radius_m = projectile.caliber_mm * 0.001f * 0.5f;
    float area = 3.14159f * radius_m * radius_m;
    return 0.5f * air_density_kg_m3 * velocity_ms * velocity_ms * projectile.drag_coefficient * area;
}

float BallisticsSystem::calculate_velocity_degradation(
    const ProjectileCharacteristics& projectile,
    float distance_m,
    float air_density_kg_m3
) {
    if (projectile.mass_kg <= 0.0f || projectile.drag_coefficient <= 0.0f) {
        return projectile.velocity_ms;
    }
    float decay = projectile.drag_coefficient * distance_m * air_density_kg_m3 / projectile.mass_kg;
    return projectile.velocity_ms * std::exp(-decay);
}

void BallisticsSystem::calculate_fragment_velocities(
    const ArmorCharacteristics& armor,
    float penetration_depth_mm,
    float impact_energy_joules,
    std::vector<float>& fragment_velocities
) {
    uint32_t count = calculate_fragment_count(armor, penetration_depth_mm, impact_energy_joules);
    fragment_velocities.clear();
    if (count == 0) {
        return;
    }

    fragment_velocities.resize(count);
    float base_velocity = std::sqrt(std::max(impact_energy_joules, 0.0f) * 0.1f / static_cast<float>(count)) * 100.0f;
    for (auto& v : fragment_velocities) {
        v = base_velocity * (0.5f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    }
}

float BallisticsSystem::calculate_energy_transfer(
    const ArmorCharacteristics& armor,
    float penetration_depth_mm,
    float impact_energy_joules
) {
    float angle_factor = std::cos(penetration_depth_mm * 0.01f);
    return 0.5f * impact_energy_joules * (1.0f - angle_factor);
}

void BallisticsSystem::get_effective_material_properties(
    const ArmorCharacteristics& armor,
    float& hardness_factor,
    float& density_kg_m3,
    float& yield_strength
) {
    hardness_factor = armor.hardness_rha;
    density_kg_m3 = armor.density_kg_m3;
    yield_strength = armor.yield_strength_mpa;
}

float BallisticsSystem::apply_environmental_modifiers(
    const ImpactParameters& impact,
    const ArmorCharacteristics& armor
) {
    float factor = 1.0f;
    if (impact.temperature_celsius < -20.0f) {
        factor *= 1.1f;
    }
    if (impact.humidity_percent > 80.0f) {
        factor *= 0.95f;
    }
    return factor;
}

// Multi-layered armor penetration calculation
float BallisticsSystem::calculate_penetration_depth(
    const ProjectileCharacteristics& proj,
    const ArmorPack& armor_pack,
    float impact_angle_deg,
    float velocity_ms
) {
    float remaining_velocity = velocity_ms;
    float remaining_mass = proj.mass_kg;
    
    for (const auto& layer : armor_pack.layers) {
        if (remaining_velocity <= 0.0f) break;
        
        // Effective thickness with angle
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        float cos_angle = std::cos(impact_angle_deg * kDegToRad);
        float effective_thickness = layer.thickness_mm / std::pow(cos_angle, 0.7f);
        
        if (layer.type == MaterialType::CERAMIC) {
            // Ceramic erosion: P ~ sqrt(V)
            float pen_depth = 0.5f * std::sqrt(remaining_velocity) * remaining_mass;
            if (pen_depth >= effective_thickness) {
                remaining_velocity *= 0.6f; // Velocity loss
                remaining_mass *= 0.9f;     // Mass erosion
            } else {
                remaining_velocity = 0.0f;
                break;
            }
        } else if (static_cast<ProjectileType>(proj.projectile_type) == ProjectileType::APFSDS && remaining_velocity > 1000.0f) {
            // Hydrodynamic penetration
            float pen = simulate_hydrodynamic_penetration(proj, layer, remaining_velocity);
            if (pen >= effective_thickness) {
                remaining_velocity -= (effective_thickness / pen) * remaining_velocity;
            } else {
                remaining_velocity = 0.0f;
                break;
            }
        } else {
            // Standard steel penetration (De Marre)
            float pen_limit = 0.001f * std::pow(remaining_velocity, 0.7f) * std::pow(remaining_mass, 0.5f);
            if (pen_limit < effective_thickness) {
                remaining_velocity = 0.0f;
                break;
            } else {
                remaining_velocity *= 0.8f; // Simplified loss
            }
        }
    }
    
    return remaining_velocity; // If > 0, penetrated
}

// Hydrodynamic penetration for APFSDS (Alekseevskii-Tate model)
float BallisticsSystem::simulate_hydrodynamic_penetration(
    const ProjectileCharacteristics& proj,
    const ArmorLayer& layer,
    float velocity
) {
    // V_pen = V_impact * (ρ_projectile / ρ_armor)^(1/3)
    float rho_ratio = std::cbrt(proj.density_kg_m3 / layer.density);
    float v_pen = velocity * rho_ratio;
    
    // Penetration depth P = L_striker * (1 - exp(-ρ_armor/ρ_projectile * V_impact / V_pen))
    float length = proj.length_mm * 0.001f; // Convert to meters

    // Avoid division by zero and invalid density values
    if (velocity < 100.0f || proj.density_kg_m3 < 1.0f || layer.density < 1.0f) {
        return 0.0f;
    }

    float rho_p = proj.density_kg_m3;  // Projectile density (tungsten ~15600 kg/m³)
    float rho_a = layer.density;       // Armor density (steel ~7850 kg/m³)
    float density_ratio = rho_p / rho_a;
    float v_penetration_factor = std::cbrt(density_ratio);
    float v_penetration = velocity * v_penetration_factor;

    // Exponent: A = (ρ_armor / ρ_projectile) * (V / V_penetration)
    float exponent_arg = std::pow(rho_a / rho_p, 4.0f / 3.0f);
    float scale_factor = 1.0f;
    float exponent = -exponent_arg * scale_factor;

    if (exponent < -100.0f) exponent = -100.0f;

    float penetration_m = length * (1.0f - std::exp(exponent));
    return std::max(0.0f, penetration_m * 1000.0f);
}

} // namespace physics_core