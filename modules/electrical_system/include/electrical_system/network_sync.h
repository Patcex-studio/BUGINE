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

#include "electrical_types.h"
#include "electrical_system.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace electrical_system {

/// Network synchronization for electrical systems
/// Handles delta-encoding, priority ordering, and compact serialization
class NetworkElectricalSync {
public:
    explicit NetworkElectricalSync(ElectricalSystem* system)
        : electrical_system_(system) {}
    
    /// Generate network updates for visible grids
    /// Returns vector of grids that need transmitting
    std::vector<NetworkElectricalState> generate_updates(
        const std::vector<EntityID>& visible_vehicles,
        bool reliable_channel = false) {
        
        std::vector<NetworkElectricalState> updates;
        updates.reserve(visible_vehicles.size());
        
        for (EntityID vehicle_id : visible_vehicles) {
            auto current_state = electrical_system_->get_network_state(vehicle_id);
            
            auto it = last_states_.find(vehicle_id);
            if (it == last_states_.end() || 
                current_state.needs_update(it->second)) {
                
                // Always send on critical events
                bool is_critical = (current_state.damage_flags & 
                                   (static_cast<uint8_t>(DamageFlag::SHORT_CIRCUIT) |
                                    static_cast<uint8_t>(DamageFlag::BATTERY_DEAD))) != 0;
                
                bool battery_critical = current_state.battery_charge_q < 50; // <5%
                
                if (is_critical || battery_critical || reliable_channel) {
                    updates.push_back(current_state);
                    last_states_[vehicle_id] = current_state;
                }
            }
        }
        
        return updates;
    }
    
    /// Apply incoming network update
    void apply_remote_update(const NetworkElectricalState& state) {
        electrical_system_->apply_network_state(state.grid_id, state);
        last_states_[state.grid_id] = state;
    }
    
    /// Serialize state to bytes (for network transmission)
    /// Returns number of bytes written
    size_t serialize_state(const NetworkElectricalState& state,
                          uint8_t* buffer,
                          size_t buffer_size) const {
        if (buffer_size < 8) return 0;
        
        // Compact format: 
        // [4] grid_id + [2] battery_q + [1] damage_flags + [1] consumers_mask
        uint32_t grid_id_le = state.grid_id; // Should be little-endian on most platforms
        memcpy(buffer + 0, &grid_id_le, sizeof(uint32_t));
        memcpy(buffer + 4, &state.battery_charge_q, sizeof(uint16_t));
        buffer[6] = state.damage_flags;
        buffer[7] = state.active_consumers_mask;
        
        return 8;
    }
    
    /// Deserialize state from bytes
    size_t deserialize_state(const uint8_t* buffer,
                            size_t buffer_size,
                            NetworkElectricalState& out_state) const {
        if (buffer_size < 8) return 0;
        
        memcpy(&out_state.grid_id, buffer + 0, sizeof(uint32_t));
        memcpy(&out_state.battery_charge_q, buffer + 4, sizeof(uint16_t));
        out_state.damage_flags = buffer[6];
        out_state.active_consumers_mask = buffer[7];
        
        return 8;
    }
    
    /// Clear stale entries (call periodically)
    void cleanup_stale_states(size_t max_entries = 1000) {
        if (last_states_.size() > max_entries) {
            // In a real implementation, remove oldest entries
            // For now, just cap at max_entries
            while (last_states_.size() > max_entries) {
                last_states_.erase(last_states_.begin());
            }
        }
    }
    
private:
    ElectricalSystem* electrical_system_;
    std::unordered_map<EntityID, NetworkElectricalState> last_states_;
};

} // namespace electrical_system
