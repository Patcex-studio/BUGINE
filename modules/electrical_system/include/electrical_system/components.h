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
#include <atomic>

namespace electrical_system {

/// Generator (power producer): engine alternator, APU, solar panels
struct GeneratorComponent {
    float max_power_w;              // Maximum power output (Watts)
    float current_power_w;          // Current power output
    bool is_running;                // Generator actively producing power?
    bool is_damaged;                // Functionally destroyed?
    float efficiency;               // Efficiency factor (0..1)
    
    explicit GeneratorComponent(float max_power = 5000.0f, 
                               float efficiency = 0.95f)
        : max_power_w(max_power),
          current_power_w(0.0f),
          is_running(false),
          is_damaged(false),
          efficiency(efficiency) {}
    
    /// Get effective power output considering damage and running state
    float get_effective_power() const {
        if (is_damaged || !is_running) return 0.0f;
        return current_power_w * efficiency;
    }
};

/// Consumer (power user): turret drive, radio, lights, weapons
struct ConsumerComponent {
    float required_power_w;         // Nominal power consumption (Watts)
    float priority;                 // 0.0 (critical) .. 1.0 (optional)
    std::atomic<uint8_t> is_active{1};    // Thread-safe: is consumer enabled?
    std::atomic<uint8_t> is_damaged{0};   // Thread-safe: is consumer broken?
    
    // Optional callback when power is lost (main thread only)
    std::function<void(EntityID, PowerLossReason)> on_power_lost;
    
    explicit ConsumerComponent(float power = 1000.0f, float priority = 0.5f)
        : required_power_w(power),
          priority(priority),
          on_power_lost(nullptr) {}
    
    /// Effective power consumption considering state
    float get_consumption() const {
        if (!is_active.load(std::memory_order_acquire) || 
            is_damaged.load(std::memory_order_acquire)) {
            return 0.0f;
        }
        return required_power_w;
    }
};

/// Main electrical grid: aggregates generators and consumers for one vehicle
struct ElectricalGridComponent {
    EntityID parent_vehicle_id;              // Associated vehicle entity
    
    // Power accounting
    float total_generated_power_w;           // Sum of all generator outputs (Watts)
    float total_consumed_power_w;            // Sum of active consumer draws (Watts)
    float battery_charge_w_h;                // Current battery charge (Watt-hours)
    float max_battery_charge_w_h;            // Battery capacity (Watt-hours)
    
    // State flags
    std::atomic<bool> is_overloaded{false};  // Thread-safe: consumption > generation?
    uint32_t damage_flags;                   // Bitfield: BATTERY_DEAD, SHORT_CIRCUIT, etc.
    
    // Battery monitoring
    float battery_dead_timer;                // Time spent below critical threshold
    float battery_critical_threshold;        // Battery % below which we're in recovery (default 5%)
    
    // Efficiency and losses
    float charge_efficiency;                 // Efficiency for charging (0..1, default 0.99)
    float discharge_efficiency;              // Efficiency for discharging (0..1, default 1.0)
    
    // Network sync
    NetworkElectricalState last_network_state{};
    
    explicit ElectricalGridComponent(EntityID vehicle_id = 0,
                                    float battery_capacity = 100.0f)
        : parent_vehicle_id(vehicle_id),
          total_generated_power_w(0.0f),
          total_consumed_power_w(0.0f),
          battery_charge_w_h(battery_capacity),
          max_battery_charge_w_h(battery_capacity),
          damage_flags(0),
          battery_dead_timer(0.0f),
          battery_critical_threshold(0.05f),
          charge_efficiency(0.99f),
          discharge_efficiency(1.0f) {}
    
    /// Get effective power available from battery
    float get_available_battery_power() const {
        return (damage_flags & static_cast<uint32_t>(DamageFlag::BATTERY_DEAD)) ? 0.0f 
               : battery_charge_w_h;
    }
    
    /// Check if grid is in overload condition
    bool check_overload() const {
        return total_consumed_power_w > total_generated_power_w;
    }
    
    /// Get battery charge percentage
    float get_battery_percentage() const {
        if (max_battery_charge_w_h <= 0.0f) return 0.0f;
        return battery_charge_w_h / max_battery_charge_w_h;
    }
};

} // namespace electrical_system
