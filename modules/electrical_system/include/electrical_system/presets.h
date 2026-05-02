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
/// Configuration and initialization examples for the electrical system
/// Place this file(s) in your config or blueprint directory

#pragma once

#include "electrical_system.h"

namespace electrical_system {

// ============================================================================
// CONSUMER BLUEPRINTS - Typical power consumers for different vehicle types
// ============================================================================

/// Standard vehicle electrical consumers (from ТЗ)
struct StandardConsumerPresets {
    static ConsumerComponent turret_drive() {
        return ConsumerComponent(5000.0f, 0.2f);  // 5kW, critical
    }
    
    static ConsumerComponent radio() {
        return ConsumerComponent(200.0f, 0.7f);   // 200W, comfort
    }
    
    static ConsumerComponent headlights() {
        return ConsumerComponent(100.0f, 0.8f);   // 100W, optional
    }
    
    static ConsumerComponent targeting_system() {
        return ConsumerComponent(300.0f, 0.3f);   // 300W, critical
    }
    
    static ConsumerComponent heating_cooling() {
        return ConsumerComponent(150.0f, 0.9f);   // 150W, optional comfort
    }
    
    static ConsumerComponent electronic_warfare() {
        return ConsumerComponent(800.0f, 0.4f);   // 800W, important
    }
};

// ============================================================================
// VEHICLE CONFIGURATIONS - Electrical setups for different vehicle types
// ============================================================================

struct VehicleElectricalConfig {
    float generator_max_power_w;     // Typically 3000-8000W
    float battery_capacity_w_h;      // Typically 50-500 Wh
    float generator_efficiency;      // Usually 0.90-0.95
    
    // Named: Tank, APC, Self-Propelled Gun, Helicopter
    
    static VehicleElectricalConfig medium_tank() {
        return {
            .generator_max_power_w = 5000.0f,
            .battery_capacity_w_h = 200.0f,
            .generator_efficiency = 0.92f
        };
    }
    
    static VehicleElectricalConfig light_tank() {
        return {
            .generator_max_power_w = 3000.0f,
            .battery_capacity_w_h = 100.0f,
            .generator_efficiency = 0.90f
        };
    }
    
    static VehicleElectricalConfig heavy_tank() {
        return {
            .generator_max_power_w = 8000.0f,
            .battery_capacity_w_h = 400.0f,
            .generator_efficiency = 0.93f
        };
    }
    
    static VehicleElectricalConfig armored_personnel_carrier() {
        return {
            .generator_max_power_w = 4000.0f,
            .battery_capacity_w_h = 150.0f,
            .generator_efficiency = 0.91f
        };
    }
    
    static VehicleElectricalConfig self_propelled_gun() {
        return {
            .generator_max_power_w = 6000.0f,
            .battery_capacity_w_h = 250.0f,
            .generator_efficiency = 0.92f
        };
    }
    
    static VehicleElectricalConfig helicopter() {
        return {
            .generator_max_power_w = 10000.0f,
            .battery_capacity_w_h = 300.0f,
            .generator_efficiency = 0.94f
        };
    }
};

// ============================================================================
// HELPER: Quick initialization function
// ============================================================================

/// Initialize electrical system for a vehicle with standard configuration
/// Usage:
///   auto& em = get_entity_manager();
///   auto elec_sys = std::make_unique<ElectricalSystem>();
///   setup_vehicle_electrical(elec_sys.get(), tank_entity, 
///       VehicleElectricalConfig::medium_tank(), em);
inline void setup_vehicle_electrical(
    ElectricalSystem* system,
    EntityID vehicle_id,
    const VehicleElectricalConfig& config,
    ecs::EntityManager& em)
{
    using namespace electrical_system;
    
    // 1. Create and register grid
    auto* grid = em.create_component<ElectricalGridComponent>(vehicle_id);
    grid->parent_vehicle_id = vehicle_id;
    grid->max_battery_charge_w_h = config.battery_capacity_w_h;
    grid->battery_charge_w_h = config.battery_capacity_w_h; // Full charge
    system->on_grid_created(vehicle_id, grid);
    
    // 2. Add generator
    auto* gen = em.create_component<GeneratorComponent>(vehicle_id);
    gen->max_power_w = config.generator_max_power_w;
    gen->efficiency = config.generator_efficiency;
    gen->is_running = false; // Will sync with engine later
    gen->is_damaged = false;
    
    EntityID generator_id = em.create_entity();
    system->on_generator_created(generator_id, gen, vehicle_id);
    
    // 3. Add standard consumers
    using CP = StandardConsumerPresets;
    
    struct ConsumerDef {
        std::function<ConsumerComponent()> factory;
        const char* name;
    };
    
    ConsumerDef consumers[] = {
        {CP::turret_drive, "turret_drive"},
        {CP::targeting_system, "targeting_system"},
        {CP::radio, "radio"},
        {CP::electronic_warfare, "electronic_warfare"},
        {CP::headlights, "headlights"},
        {CP::heating_cooling, "heating_cooling"}
    };
    
    for (size_t i = 0; i < 6; ++i) {
        auto* consumer = em.create_component<ConsumerComponent>(vehicle_id);
        *consumer = consumers[i].factory();
        
        EntityID consumer_id = em.create_entity();
        system->on_consumer_created(consumer_id, consumer, vehicle_id);
        
        // Optional: Set callbacks for debug/monitoring
        consumer->on_power_lost = [consumer_name = consumers[i].name](
            EntityID entity_id, PowerLossReason reason) {
            // Log power loss event
            // std::cerr << "Consumer " << consumer_name << " lost power\n";
        };
    }
}

} // namespace electrical_system
