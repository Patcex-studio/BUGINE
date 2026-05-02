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
        }

        // Fill remaining slots with defaults for SIMD processing
        for (size_t j = current_batch; j < batch_size; ++j) {
            velocity[j] = 0.0f;
            angle[j] = 0.0f;
            armor_thickness[j] = 1.0f;
            hardness[j] = 2400.0f;
            spall_coeff[j] = 0.5f;
            projectile_type[j] = 0.0f;
        }

        // Load into AVX2 registers
        __m256 vel_v = _mm256_load_ps(velocity);
        __m256 ang_v = _mm256_load_ps(angle);
        __m256 armor_v = _mm256_load_ps(armor_thickness);
        __m256 hard_v = _mm256_load_ps(hardness);
        __m256 spall_v = _mm256_load_ps(spall_coeff);
        __m256 type_v = _mm256_load_ps(projectile_type);

        // Output arrays
        alignas(32) float penetrated_mask[8];
        alignas(32) float depth[8];
        alignas(32) float residual_energy[8];

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
    // SIMD APFSDS penetration: P = (L/D) * sqrt(mass) * velocity / (K * hardness)
    __m256 l_over_d = _mm256_set1_ps(10.0f); // Assume L/D ratio
    __m256 k_factor = _mm256_set1_ps(2400.0f);
    __m256 mass_sqrt = _mm256_set1_ps(2.0f); // sqrt(4.0f) for 4kg projectile

    // Calculate angle factor (approximation: cos(x) ≈ 1 - x²/2 for small angles)
    __m256 angle_rad = _mm256_mul_ps(angle, _mm256_set1_ps(3.14159f / 180.0f));
    __m256 angle_sq = _mm256_mul_ps(angle_rad, angle_rad);
    __m256 angle_factor = _mm256_sub_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(angle_sq, _mm256_set1_ps(0.5f)));

    // Penetration capability
    __m256 penetration = _mm256_div_ps(
        _mm256_mul_ps(_mm256_mul_ps(l_over_d, mass_sqrt), velocity),
        _mm256_mul_ps(k_factor, hardness)
    );
    penetration = _mm256_mul_ps(penetration, angle_factor);

    // Compare with armor thickness
    __m256 penetrated = _mm256_cmp_ps(penetration, armor, _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    // Calculate depth and residual energy
    __m256 calc_depth = _mm256_min_ps(penetration, armor);
    __m256 calc_residual = _mm256_set1_ps(0.0f);
    __m256 penetrated_check = _mm256_cmp_ps(penetration, armor, _CMP_GT_OQ);
    calc_residual = _mm256_blendv_ps(calc_residual, 
        _mm256_div_ps(_mm256_sub_ps(penetration, armor), penetration), penetrated_check);

    // Store results
    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, calc_depth);
    _mm256_store_ps(residual_energy, calc_residual);
}

void BallisticsSystem::calculate_penetration_batch_heat(
    __m256 velocity, __m256 angle, __m256 armor, __m256 hardness, __m256 spall_coeff, __m256 mask,
    float* penetrated_mask, float* depth, float* residual_energy
) {
    // SIMD HEAT penetration: P = K * D^(2/3) / (1 + (S/D)^2)^(1/3)
    // Simplified: assume D=100mm, S=standoff, K=2.5
    __m256 k_factor = _mm256_set1_ps(2.5f);
    __m256 diameter = _mm256_set1_ps(0.1f); // 100mm
    __m256 standoff = _mm256_set1_ps(0.2f); // Assume 200mm standoff

    // D^(2/3) ≈ D^0.666
    __m256 d_power = _mm256_mul_ps(diameter, _mm256_sqrt_ps(diameter)); // Approximation

    // (S/D)^2
    __m256 s_over_d = _mm256_div_ps(standoff, diameter);
    __m256 s_over_d_sq = _mm256_mul_ps(s_over_d, s_over_d);

    // 1 + (S/D)^2
    __m256 denominator = _mm256_add_ps(_mm256_set1_ps(1.0f), s_over_d_sq);

    // (1 + (S/D)^2)^(1/3) ≈ (1 + (S/D)^2)^0.333
    __m256 denom_power = _mm256_mul_ps(_mm256_sqrt_ps(denominator), 
                                       _mm256_rsqrt_ps(_mm256_sqrt_ps(denominator))); // Approximation

    // Penetration
    __m256 penetration = _mm256_div_ps(_mm256_mul_ps(k_factor, d_power), denom_power);

    // Compare with armor
    __m256 penetrated = _mm256_cmp_ps(penetration, armor, _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    // Depth and residual (simplified)
    __m256 calc_depth = _mm256_min_ps(penetration, armor);
    __m256 calc_residual = _mm256_set1_ps(0.0f);

    // Store results
    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, calc_depth);
    _mm256_store_ps(residual_energy, calc_residual);
}

void BallisticsSystem::calculate_penetration_batch_hesh(
    __m256 velocity, __m256 angle, __m256 armor, __m256 hardness, __m256 spall_coeff, __m256 mask,
    float* penetrated_mask, float* depth, float* residual_energy
) {
    // SIMD HESH: spalling damage rather than deep penetration
    // Effectiveness based on explosive mass and armor thickness
    
    __m256 explosive_mass = _mm256_set1_ps(1.0f); // Assume 1kg explosive
    __m256 spall_factor = _mm256_mul_ps(explosive_mass, spall_coeff);
    
    // HESH is effective against thick armor via spalling
    __m256 penetration = _mm256_mul_ps(spall_factor, _mm256_set1_ps(0.1f)); // Reduced penetration
    
    __m256 penetrated = _mm256_cmp_ps(penetration, armor, _CMP_GE_OQ);
    penetrated = _mm256_and_ps(penetrated, mask);

    __m256 calc_depth = _mm256_min_ps(penetration, armor);
    __m256 calc_residual = _mm256_set1_ps(0.0f);

    // Store results
    _mm256_store_ps(penetrated_mask, penetrated);
    _mm256_store_ps(depth, calc_depth);
    _mm256_store_ps(residual_energy, calc_residual);
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
    float exponent = -(layer.density / proj.density_kg_m3) * velocity / v_pen;
    float penetration = length * (1.0f - std::exp(exponent));
    
    return penetration * 1000.0f; // Convert back to mm
}

} // namespace physics_core