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

#include "sph_system.h"
#include "types.h"
#include <vector>
#include <cstdint>

namespace physics_core {

// ============================================================================
// Thermal Effects System
// ============================================================================

/**
 * @enum PhaseState
 * @brief Material phase state for thermodynamic transitions
 */
enum class PhaseState : uint8_t {
    SOLID = 0,      // Frozen/solid state
    LIQUID = 1,     // Melted/liquid state
    GAS = 2,        // Evaporated/gaseous state
};

/**
 * @struct MaterialThermalProperties
 * @brief Thermal properties for a material
 * 
 * Equations:
 * - Heat capacity: Q = m * c_p * ΔT
 * - Heat conduction: q = k * ∇T
 * - Phase change: Q_latent = m * L_fusion (or L_vaporization)
 */
struct MaterialThermalProperties {
    float specific_heat;            // c_p [J/(kg·K)] - energy per unit mass per Kelvin
    float thermal_conductivity;     // k [W/(m·K)] - heat flow coefficient
    float density_solid;            // ρ_solid [kg/m³]
    float density_liquid;           // ρ_liquid [kg/m³]
    float density_gas;              // ρ_gas [kg/m³]
    
    float melting_point;            // T_melt [K] - transition from solid to liquid
    float boiling_point;            // T_boil [K] - transition from liquid to gas
    
    float latent_heat_fusion;       // L_fusion [J/kg] - energy to melt
    float latent_heat_vaporization; // L_vap [J/kg] - energy to evaporate
    
    // Constructor with typical water values
    MaterialThermalProperties(
        float cp = 4186.0f,         // Water: ~4186 J/(kg·K)
        float k = 0.6f,             // Water: ~0.6 W/(m·K)
        float t_melt = 273.15f,     // Ice melting point
        float t_boil = 373.15f,     // Water boiling point
        float l_fus = 334000.0f,    // Ice latent heat ~334 kJ/kg
        float l_vap = 2260000.0f    // Water latent heat ~2260 kJ/kg
    ) : specific_heat(cp)
      , thermal_conductivity(k)
      , density_solid(917.0f)
      , density_liquid(1000.0f)
      , density_gas(0.597f)
      , melting_point(t_melt)
      , boiling_point(t_boil)
      , latent_heat_fusion(l_fus)
      , latent_heat_vaporization(l_vap)
    {}
};

/**
 * @struct ThermalParticleSoA
 * @brief Structure of Arrays for thermal particle properties
 * 
 * Extended SPH particle data for thermodynamic simulations.
 * Aligned to 64 bytes for SIMD operations.
 */
struct alignas(64) ThermalParticleSoA {
    // Primary thermal properties
    alignas(64) std::vector<float> temperature;           // T [K] - absolute temperature
    alignas(64) std::vector<uint8_t> phase;              // PhaseState enum
    alignas(64) std::vector<float> latent_heat_accum;    // Accumulated latent heat during transition
    
    // Material type (index into properties table)
    alignas(64) std::vector<uint8_t> material_type;
    
    // Phase transition state machine
    alignas(64) std::vector<float> phase_transition_progress; // [0,1] - progress of phase change
    
    size_t count = 0;
    
    /**
     * Allocate space for thermal particles
     */
    void resize(size_t n) {
        temperature.resize(n);
        phase.resize(n);
        latent_heat_accum.resize(n);
        material_type.resize(n);
        phase_transition_progress.resize(n);
        count = n;
    }
};

/**
 * @class ThermalSystem
 * @brief Manages heat transfer and phase transitions in SPH fluids
 * 
 * Physics:
 * 1. **Heat Conduction**: Diffusion of temperature through particles
 *    ∂T/∂t = (k/(ρc_p)) * ∇²T
 * 
 * 2. **Phase Transitions**: Tracking latent heat absorption/release
 *    - Solid→Liquid: absorbs L_fusion, temp stays at T_melt until complete
 *    - Liquid→Gas: absorbs L_vap, temp stays at T_boil until complete
 *    - Reverse for cooling
 * 
 * 3. **Energy Conservation**: Total energy = kinetic + thermal + latent
 */
class ThermalSystem {
public:
    ThermalSystem(SPHSystem* sph_system);
    ~ThermalSystem();
    
    /**
     * Initialize thermal properties for a fluid type
     * @param fluid_type FluidType enum
     * @param properties MaterialThermalProperties
     */
    void set_material_properties(FluidType fluid_type, const MaterialThermalProperties& properties);
    
    /**
     * Update thermal effects (heat conduction + phase transitions)
     * @param dt Time step [seconds]
     */
    void update(float dt);
    
    /**
     * Heat particle by direct temperature increase
     * @param particle_idx Particle index
     * @param energy Energy to add [Joules]
     * @param mass Mass of particle [kg]
     */
    void add_thermal_energy(uint32_t particle_idx, float energy, float mass);
    
    /**
     * Set particle temperature directly
     * @param particle_idx Particle index
     * @param temperature Temperature [K]
     */
    void set_particle_temperature(uint32_t particle_idx, float temperature);
    
    /**
     * Get particle phase state
     */
    PhaseState get_particle_phase(uint32_t particle_idx) const;
    
    /**
     * Get particle temperature
     */
    float get_particle_temperature(uint32_t particle_idx) const;
    
    /**
     * Compute total thermal energy in system (for validation)
     * @return Total thermal energy [Joules]
     */
    float get_total_thermal_energy() const;
    
    /**
     * Get particles that underwent phase transitions this frame
     * @return Vector of (particle_idx, new_phase)
     */
    const std::vector<std::pair<uint32_t, PhaseState>>& get_phase_transitions() const {
        return recent_phase_transitions_;
    }
    
    /**
     * Get thermal particle data
     */
    const ThermalParticleSoA& get_thermal_particles() const { return thermal_particles_; }
    
<<<<<<< HEAD
=======
    /**
     * Update SPH fluid properties based on thermal effects
     * Applied after heat transfer and phase transitions to influence viscosity/density
     */
    void update_thermal_properties();
    
>>>>>>> c308d63 (Helped the rabbits find a home)
private:
    SPHSystem* sph_system_;
    ThermalParticleSoA thermal_particles_;
    
    // Material thermal properties indexed by FluidType
    std::vector<MaterialThermalProperties> material_properties_;
    
    // Record of phase transitions this frame
    std::vector<std::pair<uint32_t, PhaseState>> recent_phase_transitions_;
    
    // SIMD computation parameters
    float heat_diffusion_coeff_ = 0.1f;  // Controls heat diffusion speed
    float phase_transition_speed_ = 0.5f; // Controls phase transition rate
    
    /**
     * Compute heat conduction between particles (SIMD)
     * Implements Fourier's law through SPH kernel
     */
    void compute_heat_transfer_simd(float dt);
    
    /**
     * Check and handle phase state transitions
     */
    void process_phase_transitions();
    
    /**
     * Helper: transition particle from one phase to another
     */
    void transition_particle(uint32_t idx, PhaseState new_phase);
};

} // namespace physics_core
