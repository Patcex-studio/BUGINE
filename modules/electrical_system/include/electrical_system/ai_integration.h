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

#include "electrical_system.h"
#include "components.h"
#include <string>
#include <atomic>

namespace electrical_system {

/// Integration with AI decision systems
/// Provides status checks for behavior trees and decision engines
class AIElectricalStatus {
public:
    explicit AIElectricalStatus(const ElectricalSystem* electrical_system)
        : electrical_system_(electrical_system) {}
    
    // Query functions for AI decision trees
    
    /// Is the electrical grid currently overloaded?
    bool is_grid_overloaded(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        return grid && grid->is_overloaded.load(std::memory_order_acquire);
    }
    
    /// Get battery charge percentage (0.0 .. 1.0)
    float get_battery_percentage(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        return grid ? grid->get_battery_percentage() : 0.0f;
    }
    
    /// Is battery critical (below 5%)?
    bool is_battery_critical(EntityID vehicle_id) const {
        return get_battery_percentage(vehicle_id) < 0.05f;
    }
    
    /// Is battery low (below 20%)?
    bool is_battery_low(EntityID vehicle_id) const {
        return get_battery_percentage(vehicle_id) < 0.20f;
    }
    
    /// Get total power deficit (negative = surplus)
    float get_power_deficit(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        if (!grid) return 0.0f;
        return grid->total_consumed_power_w - grid->total_generated_power_w;
    }
    
    /// Check specific damage flags
    bool has_damage_flag(EntityID vehicle_id, DamageFlag flag) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        if (!grid) return false;
        return (grid->damage_flags & static_cast<uint32_t>(flag)) != 0;
    }
    
    /// Is generator damaged?
    bool is_generator_damaged(EntityID vehicle_id) const {
        return has_damage_flag(vehicle_id, DamageFlag::GENERATOR_FAULT);
    }
    
    /// Is battery dead (or critical)?
    bool is_battery_dead(EntityID vehicle_id) const {
        return has_damage_flag(vehicle_id, DamageFlag::BATTERY_DEAD);
    }
    
    /// Is there a short circuit?
    bool is_short_circuit_detected(EntityID vehicle_id) const {
        return has_damage_flag(vehicle_id, DamageFlag::SHORT_CIRCUIT);
    }
    
    /// Is there a fire hazard?
    bool is_fire_hazard(EntityID vehicle_id) const {
        return has_damage_flag(vehicle_id, DamageFlag::FIRE_HAZARD);
    }
    
    // AI Decision Support
    
    /// Recommended actions based on electrical state
    enum class AIPowerAction {
        NORMAL,              // Normal operations
        REDUCE_OPTIONAL,     // Disable lights, comfort systems
        DISABLE_RADIO,       // Enter silent running (no comms)
        ENABLE_SILENT_RUN,   // Maximum power conservation
        SEEK_REPAIR,         // Find repair depot or use reserve power
        EMERGENCY_SHUTDOWN,  // Critical damage, shut down non-essential
        ABANDON_VEHICLE      // Total electrical failure
    };
    
    /// Get recommended action based on electrical status
    AIPowerAction get_recommended_action(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        if (!grid) return AIPowerAction::NORMAL;
        
        // Check critical conditions first
        if (is_battery_dead(vehicle_id)) {
            return AIPowerAction::ABANDON_VEHICLE;
        }
        
        if (is_short_circuit_detected(vehicle_id)) {
            return AIPowerAction::EMERGENCY_SHUTDOWN;
        }
        
        float battery_pct = grid->get_battery_percentage();
        
        if (battery_pct < 0.02f) {
            return AIPowerAction::EMERGENCY_SHUTDOWN;
        }
        
        if (battery_pct < 0.05f) {
            return AIPowerAction::SEEK_REPAIR;
        }
        
        if (battery_pct < 0.10f && grid->check_overload()) {
            return AIPowerAction::ENABLE_SILENT_RUN;
        }
        
        if (battery_pct < 0.20f) {
            return AIPowerAction::DISABLE_RADIO;
        }
        
        if (grid->check_overload()) {
            return AIPowerAction::REDUCE_OPTIONAL;
        }
        
        return AIPowerAction::NORMAL;
    }
    
    /// Get human-readable status string for debugging
    std::string get_status_string(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        if (!grid) return "NO_GRID";
        
        std::string status;
        status += "Battery:" + std::to_string(static_cast<int>(grid->get_battery_percentage() * 100)) + "% ";
        status += "Load:" + std::to_string(static_cast<int>(grid->total_consumed_power_w)) + "W ";
        status += "Gen:" + std::to_string(static_cast<int>(grid->total_generated_power_w)) + "W ";
        
        if (is_battery_dead(vehicle_id)) status += "[DEAD] ";
        if (is_short_circuit_detected(vehicle_id)) status += "[SHORT_CIRCUIT] ";
        if (is_overload(vehicle_id)) status += "[OVERLOAD] ";
        if (is_battery_critical(vehicle_id)) status += "[CRITICAL] ";
        
        return status;
    }
    
private:
    const ElectricalSystem* electrical_system_;
    
    bool is_overload(EntityID vehicle_id) const {
        auto* grid = electrical_system_->get_grid(vehicle_id);
        return grid && grid->check_overload();
    }
};

} // namespace electrical_system
