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

#include "types.h"
#include <vector>
#include <cstdint>
#include <cmath>

namespace physics_core {

// ============================================================================
// Acoustic System - Shockwave and Sound Simulation
// ============================================================================

/**
 * @enum ShockwaveType
 * @brief Classification of explosive phenomena
 */
enum class ShockwaveType : uint8_t {
    EXPLOSION = 0,              // General explosive detonation
    SHAPED_CHARGE = 1,          // Directional shaped charge (HEAT)
    KINETIC_PENETRATION = 2,   // Projectile impact (lower pressure pulse)
    FUEL_AIR_EXPLOSION = 3,    // High-order detonation (very broad pulse)
};

/**
 * @struct ExplosionEvent
 * @brief Data for a single explosion event in the simulation
 * 
 * Used to track:
 * - Pressure wave propagation
 * - Sound generation
 * - Thermal radiation
 * - Fragment trajectories
 */
struct ExplosionEvent {
    Vec3 position;                  // Detonation location [m]
    ShockwaveType type;            // Classification
    
    float tnt_equivalent;           // Explosive charge equivalent [kg TNT]
    float max_pressure;             // Peak overpressure at 1m [Pa]
    float duration;                 // Pulse duration [s]
    float timestamp;                // Simulation time of event [s]
    
    float max_propagation_dist;     // Maximum distance for effects [m]
    float pressure_decay_exp;       // Pressure falloff exponent (typically 1.0)
    
    bool is_active = true;
    
    /**
     * Calculate pressure at distance from epicenter
     * Uses scaling law: P(r) = P_1m * (1 / r^n)
     */
    float get_pressure_at_distance(float distance) const {
        if (distance < 0.1f) distance = 0.1f; // Avoid singularity
        const float base_dist = 1.0f;
        return max_pressure * std::pow(base_dist / distance, pressure_decay_exp);
    }
    
    /**
     * Estimate impulse (force × time) at distance
     * Used for ballistic effects on objects
     */
    float get_impulse_at_distance(float distance) const {
        float pressure = get_pressure_at_distance(distance);
        return pressure * duration;  // Simplified: P * t
    }
};

/**
 * @struct AcousticReceiver
 * @brief Virtual microphone for sound propagation simulation
 * 
 * Simulates arrival of acoustic energy at a listener location,
 * accounting for:
 * - Travel time (distance / speed of sound)
 * - Attenuation (frequency-dependent)
 * - Angle of incidence (for directional effects)
 */
struct AcousticReceiver {
    Vec3 position;                  // Listener location [m]
    uint32_t entity_id;            // Associated entity (crew member, sensor, etc.)
    
    struct ImpactData {
        float arrival_time;         // When sound reaches receiver [s]
        float sound_pressure_level; // SPL in dB
        float peak_pressure;        // Peak acoustic pressure [Pa]
        ShockwaveType source_type;  // Type of explosion
    };
    
    std::vector<ImpactData> recent_impacts;  // Impacts in current frame
};

/**
 * @class AcousticsSystem
 * @brief Simulation of shockwave propagation and acoustic effects
 * 
 * Responsibilities:
 * 1. **Pressure Wave Simulation**: Track pressure fronts from explosions
 *    - Pressure decays as 1/r or 1/r² depending on geometry
 *    - Account for artificial viscosity in SPH for shock stability
 * 
 * 2. **Sound Generation**: Emit acoustic events for audio system
 *    - Classify explosion type (free-field, confined, etc.)
 *    - Generate frequency content
 *    - Calculate delay and amplitude for distant listeners
 * 
 * 3. **Barotrauma Calculation**: Damage to personnel from pressure
 *    - ΔP > 50 kPa: eardrum rupture risk
 *    - ΔP > 100 kPa: severe lung injury
 *    - ΔP > 200 kPa: fatal
 * 
 * 4. **Acoustic Shadowing**: Objects block or attenuate sound
 *    This is simplified - full implementation would need ray tracing
 */
class AcousticsSystem {
public:
    // Speed of sound in air [m/s] at sea level, 20°C
    static constexpr float SPEED_OF_SOUND = 343.0f;
    
    // Reference pressure for dB calculation (20 µPa = 20e-6 Pa)
    static constexpr float REFERENCE_PRESSURE = 20e-6f;
    
    AcousticsSystem();
    ~AcousticsSystem();
    
    /**
     * Register an explosion event
     * @param position Detonation location
     * @param type Shockwave classification
     * @param tnt_equivalent Charge equivalent in kg TNT
     */
    void register_explosion(
        const Vec3& position,
        ShockwaveType type,
        float tnt_equivalent
    );
    
    /**
     * Register an acoustic receiver (listener)
     * @param position Listener location
     * @param entity_id Associated entity for callbacks
     */
    uint32_t register_receiver(const Vec3& position, uint32_t entity_id);
    
    /**
     * Update receiver position
     */
    void update_receiver_position(uint32_t receiver_id, const Vec3& position);
    
    /**
     * Update acoustic effects for this frame
     * @param current_time Simulation time [seconds]
     * @param dt Time step [seconds]
     */
    void update(float current_time, float dt);
    
    /**
     * Get impacts on a receiver for this frame
     */
    const std::vector<AcousticReceiver::ImpactData>& get_receiver_impacts(uint32_t receiver_id) const;
    
    /**
     * Get particle displacement field (for visualization of pressure waves)
     * Useful for debugging shockwave propagation
     */
    float get_particle_displacement(const Vec3& position, float time_since_explosion) const;
    
    /**
     * Get active explosion events
     */
    const std::vector<ExplosionEvent>& get_active_explosions() const { return active_explosions_; }
    
    /**
     * Clear inactive events and receivers
     */
    void cleanup();
    
private:
    std::vector<ExplosionEvent> active_explosions_;
    std::vector<AcousticReceiver> receivers_;
    
    float current_time_ = 0.0f;
    
    // Pressure decay parameters
    float pressure_reference_ = 101325.0f;  // 1 atm in Pa
    
    /**
     * Calculate sound pressure level in dB
     * SPL = 20 * log10(P_acoustic / P_ref)
     * where P_ref = 20e-6 Pa (hearing threshold)
     */
    float calculate_spl(float acoustic_pressure) const {
        if (acoustic_pressure < REFERENCE_PRESSURE) return 0.0f;
        return 20.0f * std::log10(acoustic_pressure / REFERENCE_PRESSURE);
    }
    
    /**
     * Assess barotrauma risk level
     * @param pressure_differential Overpressure above atmospheric [Pa]
     * @return Damage level: 0=none, 1=minor, 2=serious, 3=critical
     */
    int assess_barotrauma(float pressure_differential) const;
};

} // namespace physics_core
