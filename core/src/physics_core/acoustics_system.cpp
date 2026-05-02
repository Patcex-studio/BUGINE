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
#include "physics_core/acoustics_system.h"
#include <cmath>
#include <algorithm>

namespace physics_core {

// ============================================================================
// AcousticsSystem Implementation
// ============================================================================

AcousticsSystem::AcousticsSystem()
{
}

AcousticsSystem::~AcousticsSystem() = default;

void AcousticsSystem::register_explosion(
    const Vec3& position,
    ShockwaveType type,
    float tnt_equivalent
)
{
    // Calculate peak pressure at 1 meter using scaling law
    // For TNT: roughly 1 kg TNT ~ 4.7 MPa at 1 meter (empirical)
    float peak_pressure_1m = tnt_equivalent * 4.7e6f;
    
    // Cap extreme values
    peak_pressure_1m = std::min(peak_pressure_1m, 50e6f);  // 50 MPa max
    
    // Determine explosion characteristics based on type
    float duration = 0.005f;  // Default pulse duration (5 ms)
    float max_dist = 100.0f;  // Default propagation distance
    float decay_exp = 1.0f;   // Default 1/r decay
    
    switch (type) {
        case ShockwaveType::EXPLOSION:
            duration = 0.005f;
            max_dist = 200.0f * std::pow(tnt_equivalent, 0.33f);
            decay_exp = 1.0f;
            break;
            
        case ShockwaveType::SHAPED_CHARGE:
            // Very directional, narrow pulse
            duration = 0.002f;
            max_dist = 50.0f * std::pow(tnt_equivalent, 0.33f);
            decay_exp = 1.2f;  // Faster falloff
            break;
            
        case ShockwaveType::KINETIC_PENETRATION:
            // Relatively low pressure, short duration
            duration = 0.001f;
            max_dist = 20.0f * std::pow(tnt_equivalent, 0.33f);
            decay_exp = 0.8f;  // Slower falloff
            break;
            
        case ShockwaveType::FUEL_AIR_EXPLOSION:
            // Very broad pulse, lower initial pressure
            duration = 0.050f;  // Longer duration
            peak_pressure_1m *= 0.5f;  // Lower peak
            max_dist = 300.0f * std::pow(tnt_equivalent, 0.33f);
            decay_exp = 0.9f;
            break;
    }
    
    ExplosionEvent event;
    event.position = position;
    event.type = type;
    event.tnt_equivalent = tnt_equivalent;
    event.max_pressure = peak_pressure_1m;
    event.duration = duration;
    event.timestamp = current_time_;
    event.max_propagation_dist = max_dist;
    event.pressure_decay_exp = decay_exp;
    event.is_active = true;
    
    active_explosions_.push_back(event);
}

uint32_t AcousticsSystem::register_receiver(const Vec3& position, uint32_t entity_id)
{
    AcousticReceiver receiver;
    receiver.position = position;
    receiver.entity_id = entity_id;
    
    uint32_t receiver_id = static_cast<uint32_t>(receivers_.size());
    receivers_.push_back(receiver);
    
    return receiver_id;
}

void AcousticsSystem::update_receiver_position(uint32_t receiver_id, const Vec3& position)
{
    if (receiver_id < receivers_.size()) {
        receivers_[receiver_id].position = position;
    }
}

void AcousticsSystem::update(float current_time, float dt)
{
    current_time_ = current_time;
    
    // Clear previous frame impacts
    for (auto& receiver : receivers_) {
        receiver.recent_impacts.clear();
    }
    
    // Process each active explosion
    for (auto& explosion : active_explosions_) {
        if (!explosion.is_active) continue;
        
        float time_since_event = current_time - explosion.timestamp;
        
        // Explosion is active for its duration + some decay time
        if (time_since_event > explosion.duration + 0.1f) {
            explosion.is_active = false;
            continue;
        }
        
        // Calculate impact on each receiver
        for (uint32_t r = 0; r < receivers_.size(); ++r) {
            const Vec3& receiver_pos = receivers_[r].position;
            
            // Distance from epicenter
            float distance = static_cast<float>((receiver_pos - explosion.position).magnitude());
            
            // Skip if beyond max propagation distance
            if (distance > explosion.max_propagation_dist) {
                continue;
            }
            
            // Calculate propagation delay (speed of sound)
            float propagation_delay = distance / SPEED_OF_SOUND;
            float arrival_time = explosion.timestamp + propagation_delay;
            
            // Check if sound reaches receiver in current frame
            if (current_time >= arrival_time && current_time < arrival_time + explosion.duration) {
                // Calculate pressure at this distance
                float pressure = explosion.get_pressure_at_distance(distance);
                
                // Convert to dB
                float spl = calculate_spl(pressure);
                
                // Record impact
                AcousticReceiver::ImpactData impact;
                impact.arrival_time = arrival_time;
                impact.sound_pressure_level = spl;
                impact.peak_pressure = pressure;
                impact.source_type = explosion.type;
                
                receivers_[r].recent_impacts.push_back(impact);
            }
        }
    }
}

const std::vector<AcousticReceiver::ImpactData>& AcousticsSystem::get_receiver_impacts(uint32_t receiver_id) const
{
    static const std::vector<AcousticReceiver::ImpactData> empty;
    
    if (receiver_id < receivers_.size()) {
        return receivers_[receiver_id].recent_impacts;
    }
    
    return empty;
}

float AcousticsSystem::get_particle_displacement(const Vec3& position, float time_since_explosion) const
{
    // Simplified particle displacement based on N-wave (sawtooth pressure profile)
    // In reality, this is more complex and depends on medium properties
    
    // For visualization: sinusoidal displacement proportional to pressure
    // This is a placeholder - full implementation would need wave equation solver
    
    if (active_explosions_.empty()) return 0.0f;
    
    float max_displacement = 0.0f;
    
    for (const auto& explosion : active_explosions_) {
        float distance = static_cast<float>((position - explosion.position).magnitude());
        
        if (distance > explosion.max_propagation_dist) {
            continue;
        }
        
        float time_from_arrival = time_since_explosion - (distance / SPEED_OF_SOUND);
        
        if (time_from_arrival >= 0.0f && time_from_arrival < explosion.duration) {
            // Simple sinusoidal model of particle motion
            float pressure = explosion.get_pressure_at_distance(distance);
            
            // Displacement proportional to pressure and sinusoidal in time
            float phase = (time_from_arrival / explosion.duration) * 6.28318f;
            float displacement = (pressure / 1e5f) * std::sin(phase) * 0.01f;  // Max 1cm displacement
            
            max_displacement = std::max(max_displacement, std::abs(displacement));
        }
    }
    
    return max_displacement;
}

int AcousticsSystem::assess_barotrauma(float pressure_differential) const
{
    // Barotrauma severity assessment
    // Based on empirical data from blast injuries
    
    const float EARDRUM_RUPTURE_THRESHOLD = 50000.0f;  // 50 kPa
    const float LUNG_INJURY_THRESHOLD = 100000.0f;     // 100 kPa
    const float LETHAL_THRESHOLD = 200000.0f;          // 200 kPa
    
    if (pressure_differential < EARDRUM_RUPTURE_THRESHOLD) {
        return 0;  // Minor or no injury
    } else if (pressure_differential < LUNG_INJURY_THRESHOLD) {
        return 1;  // Eardrum rupture risk
    } else if (pressure_differential < LETHAL_THRESHOLD) {
        return 2;  // Serious lung/internal injuries
    } else {
        return 3;  // Critical/lethal
    }
}

void AcousticsSystem::cleanup()
{
    // Remove inactive explosions
    auto it = active_explosions_.begin();
    while (it != active_explosions_.end()) {
        if (!it->is_active) {
            it = active_explosions_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace physics_core
