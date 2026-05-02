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
#include "physics_core/thermal_system.h"
#include <immintrin.h>
#include <algorithm>
#include <cmath>

namespace physics_core {

// ============================================================================
// ThermalSystem Implementation
// ============================================================================

ThermalSystem::ThermalSystem(SPHSystem* sph_system)
    : sph_system_(sph_system)
{
    // Initialize with default material properties
    material_properties_.resize(6);  // 6 fluid types
    
    // Default: water
    material_properties_[static_cast<uint8_t>(FluidType::WATER)] = 
        MaterialThermalProperties();
    
    // Mud
    material_properties_[static_cast<uint8_t>(FluidType::MUD)] = 
        MaterialThermalProperties(1700.0f, 0.8f, 273.15f, 373.15f, 334000.0f, 2260000.0f);
    
    // Blood
    material_properties_[static_cast<uint8_t>(FluidType::BLOOD)] = 
        MaterialThermalProperties(3600.0f, 0.5f, 268.15f, 378.15f, 250000.0f, 2200000.0f);
    
    // Oil
    material_properties_[static_cast<uint8_t>(FluidType::OIL)] = 
        MaterialThermalProperties(2100.0f, 0.14f, 253.15f, 473.15f, 150000.0f, 300000.0f);
    
    // Fuel
    material_properties_[static_cast<uint8_t>(FluidType::FUEL)] = 
        MaterialThermalProperties(2000.0f, 0.1f, 220.15f, 450.15f, 50000.0f, 250000.0f);
    
    // Chemical (placeholder)
    material_properties_[static_cast<uint8_t>(FluidType::CHEMICAL)] = 
        MaterialThermalProperties();
}

ThermalSystem::~ThermalSystem() = default;

void ThermalSystem::set_material_properties(FluidType fluid_type, const MaterialThermalProperties& properties)
{
    uint8_t idx = static_cast<uint8_t>(fluid_type);
    if (idx < material_properties_.size()) {
        material_properties_[idx] = properties;
    }
}

void ThermalSystem::update(float dt)
{
    if (!sph_system_) return;
    
    // Ensure thermal particle array is synchronized
    const auto& sph_particles = sph_system_->get_particles();
    if (thermal_particles_.count != sph_particles.size()) {
        thermal_particles_.resize(sph_particles.size());
        
        // Initialize all particles as liquid at room temperature
        for (size_t i = 0; i < sph_particles.size(); ++i) {
            thermal_particles_.temperature[i] = sph_particles[i].temperature;
            thermal_particles_.phase[i] = static_cast<uint8_t>(PhaseState::LIQUID);
            thermal_particles_.latent_heat_accum[i] = 0.0f;
            thermal_particles_.material_type[i] = sph_particles[i].fluid_type;
            thermal_particles_.phase_transition_progress[i] = 0.0f;
        }
    }
    
    // Clear recent transitions
    recent_phase_transitions_.clear();
    
    // Main thermal computations
    compute_heat_transfer_simd(dt);
    process_phase_transitions();
}

void ThermalSystem::add_thermal_energy(uint32_t particle_idx, float energy, float mass)
{
    if (particle_idx >= thermal_particles_.count) return;
    
    float& mass_per_particle = mass;  // Assume uniform mass
    uint8_t mat_type = thermal_particles_.material_type[particle_idx];
    
    if (mat_type < material_properties_.size()) {
        float cp = material_properties_[mat_type].specific_heat;
        float dT = energy / (mass * cp);
        thermal_particles_.temperature[particle_idx] += dT;
    }
}

void ThermalSystem::set_particle_temperature(uint32_t particle_idx, float temperature)
{
    if (particle_idx < thermal_particles_.count) {
        thermal_particles_.temperature[particle_idx] = temperature;
    }
}

PhaseState ThermalSystem::get_particle_phase(uint32_t particle_idx) const
{
    if (particle_idx < thermal_particles_.count) {
        return static_cast<PhaseState>(thermal_particles_.phase[particle_idx]);
    }
    return PhaseState::LIQUID;
}

float ThermalSystem::get_particle_temperature(uint32_t particle_idx) const
{
    if (particle_idx < thermal_particles_.count) {
        return thermal_particles_.temperature[particle_idx];
    }
    return 293.15f;
}

float ThermalSystem::get_total_thermal_energy() const
{
    float total_energy = 0.0f;
    
    // Simplified: assume unit mass for each particle
    for (size_t i = 0; i < thermal_particles_.count; ++i) {
        uint8_t mat_type = thermal_particles_.material_type[i];
        if (mat_type < material_properties_.size()) {
            float cp = material_properties_[mat_type].specific_heat;
            float T = thermal_particles_.temperature[i];
            float T_ref = 273.15f;  // Reference temperature
            
            total_energy += cp * (T - T_ref);
            total_energy += thermal_particles_.latent_heat_accum[i];
        }
    }
    
    return total_energy;
}

void ThermalSystem::compute_heat_transfer_simd(float dt)
{
    if (thermal_particles_.count < 2) return;
    
    // Simplified heat transfer using neighbor diffusion
    // In full implementation, would use SPH kernel for proper diffusion
    
    const float diffusivity = heat_diffusion_coeff_ * dt;
    std::vector<float> new_temperatures = thermal_particles_.temperature;
    
    // Process particles in SIMD batches
    static constexpr size_t BATCH_SIZE = 8;
    
    for (size_t i = 0; i < thermal_particles_.count; i += BATCH_SIZE) {
        size_t batch_end = std::min(i + BATCH_SIZE, thermal_particles_.count);
        
        // For each particle in batch, do simple heat diffusion with neighbors
        for (size_t j = i; j < batch_end; ++j) {
            float T_j = thermal_particles_.temperature[j];
            float dT = 0.0f;
            int neighbor_count = 0;
            
            // Check nearby particles (simplified neighbor search)
            const int search_range = 5;
            for (int k = std::max(0, (int)j - search_range); k < std::min((int)thermal_particles_.count, (int)j + search_range + 1); ++k) {
                if (k == (int)j) continue;
                
                float T_k = thermal_particles_.temperature[k];
                float delta_T = (T_k - T_j) * diffusivity;
                dT += delta_T;
                neighbor_count++;
            }
            
            if (neighbor_count > 0) {
                dT /= neighbor_count;
                new_temperatures[j] += dT;
            }
        }
    }
    
    thermal_particles_.temperature = new_temperatures;
}

void ThermalSystem::process_phase_transitions()
{
    for (size_t i = 0; i < thermal_particles_.count; ++i) {
        uint8_t mat_type = thermal_particles_.material_type[i];
        if (mat_type >= material_properties_.size()) continue;
        
        const auto& props = material_properties_[mat_type];
        float T = thermal_particles_.temperature[i];
        PhaseState current_phase = static_cast<PhaseState>(thermal_particles_.phase[i]);
        float& latent_accum = thermal_particles_.latent_heat_accum[i];
        
        // Simplified phase transition logic
        if (current_phase == PhaseState::SOLID) {
            if (T > props.melting_point) {
                // Try to melt
                if (latent_accum < props.latent_heat_fusion) {
                    // Absorbing latent heat
                    latent_accum += props.specific_heat * (T - props.melting_point);
                    thermal_particles_.temperature[i] = props.melting_point;
                } else {
                    // Transition to liquid
                    transition_particle(i, PhaseState::LIQUID);
                    latent_accum = 0.0f;
                    thermal_particles_.phase_transition_progress[i] = 1.0f;
                }
            }
        }
        else if (current_phase == PhaseState::LIQUID) {
            if (T > props.boiling_point) {
                // Try to evaporate
                if (latent_accum < props.latent_heat_vaporization) {
                    // Absorbing latent heat
                    latent_accum += props.specific_heat * (T - props.boiling_point);
                    thermal_particles_.temperature[i] = props.boiling_point;
                } else {
                    // Transition to gas
                    transition_particle(i, PhaseState::GAS);
                    latent_accum = 0.0f;
                    thermal_particles_.phase_transition_progress[i] = 1.0f;
                }
            }
            else if (T < props.melting_point) {
                // Freeze back to solid
                transition_particle(i, PhaseState::SOLID);
                latent_accum = 0.0f;
            }
        }
        else if (current_phase == PhaseState::GAS) {
            if (T < props.boiling_point) {
                // Condense back to liquid
                transition_particle(i, PhaseState::LIQUID);
                latent_accum = 0.0f;
            }
        }
    }
}

void ThermalSystem::transition_particle(uint32_t idx, PhaseState new_phase)
{
    if (idx >= thermal_particles_.count) return;
    
    PhaseState old_phase = static_cast<PhaseState>(thermal_particles_.phase[idx]);
    thermal_particles_.phase[idx] = static_cast<uint8_t>(new_phase);
    
    // Record transition for callbacks
    recent_phase_transitions_.emplace_back(idx, new_phase);
    
    // In full implementation, would:
    // - Transfer particle between SPH, Liquid, and Gas systems
    // - Update visual representation
    // - Generate events for damage/effects systems
}

} // namespace physics_core
