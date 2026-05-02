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
#include <cstdint>
#include <algorithm>

namespace electrical_system {

/// Integration hooks between DamageSystem and ElectricalSystem
class DamageSystemIntegration {
public:
    explicit DamageSystemIntegration(ElectricalSystem* electrical_system)
        : electrical_system_(electrical_system) {}
    
    /// Called when a component takes damage
    /// Should be called from DamageSystem::apply_damage_to_component
    /// Returns true if electrical damage was applied
    bool apply_electrical_damage(EntityID component_entity,
                                EntityID vehicle_entity,
                                float damage_amount,
                                uint8_t component_type) {
        if (!electrical_system_) return false;
        
        auto* grid = electrical_system_->get_grid(vehicle_entity);
        if (!grid) return false;
        
        // Damage mapping based on component type
        // ComponentType from vehicle_component.h:
        // ENGINE = 2, GENERATOR = (implicitly associated with engine)
        // FUEL_TANK = 7, AMMO_RACK = 6
        
        float damage_ratio = damage_amount / 100.0f; // Normalize to 0..1
        damage_ratio = std::clamp(damage_ratio, 0.0f, 1.0f);
        
        switch (component_type) {
            case 2: // ENGINE - damages generators
                apply_generator_damage(vehicle_entity, damage_ratio);
                break;
                
            case 6: // AMMO_RACK - potential short circuit
                if (damage_ratio > 0.7f) {
                    trigger_short_circuit(grid, vehicle_entity);
                }
                break;
                
            case 7: // FUEL_TANK - potential fire
                if (damage_ratio > 0.5f) {
                    set_fire_hazard_flag(grid);
                }
                break;
                
            case 5: // GUN_BARREL - affects targeting systems (consumers)
                apply_consumer_damage(vehicle_entity, "targeting_system", damage_ratio);
                break;
                
            default:
                // Other components may have associated consumers
                break;
        }
        
        return true;
    }
    
    /// Called when fire spreads to electrical components
    void apply_fire_damage(EntityID vehicle_entity, float fire_intensity) {
        auto* grid = electrical_system_->get_grid(vehicle_entity);
        if (!grid) return;
        
        // High fire intensity can trigger short circuit
        if (fire_intensity > 0.8f) {
            trigger_short_circuit(grid, vehicle_entity);
        }
        
        // Moderate fire increases battery drain rate (no implementation needed,
        // handled in ElectricalSystem if required)
    }
    
    /// Check electrical damage flags and report back to damage system
    uint32_t get_electrical_damage_flags(EntityID vehicle_entity) const {
        auto* grid = electrical_system_->get_grid(vehicle_entity);
        return grid ? grid->damage_flags : 0;
    }
    
private:
    ElectricalSystem* electrical_system_;
    
    void apply_generator_damage(EntityID vehicle_entity, float damage_ratio) {
        auto* grid = electrical_system_->get_grid(vehicle_entity);
        if (!grid) return;
        
        // Mark generator as damaged if damage is severe
        if (damage_ratio > 0.5f) {
            set_damage_flag(grid->damage_flags, DamageFlag::GENERATOR_FAULT, true);
        }
    }
    
    void apply_consumer_damage(EntityID vehicle_entity, 
                              const char* consumer_name,
                              float damage_ratio) {
        // This would require mapping consumer names to entity IDs
        // For now, skip - integration point for future expansion
    }
    
    void trigger_short_circuit(ElectricalGridComponent* grid, EntityID vehicle_entity) {
        if (!grid) return;
        set_damage_flag(grid->damage_flags, DamageFlag::SHORT_CIRCUIT, true);
        
        // Optional: trigger immediate battery discharge
        grid->battery_charge_w_h *= 0.5f; // 50% instant discharge
    }
    
    void set_fire_hazard_flag(ElectricalGridComponent* grid) {
        if (!grid) return;
        set_damage_flag(grid->damage_flags, DamageFlag::FIRE_HAZARD, true);
    }
    
    // Helper to set damage flags (copied from ElectricalSystem)
    static void set_damage_flag(uint32_t& flags, DamageFlag flag, bool set) {
        if (set) {
            flags |= static_cast<uint32_t>(flag);
        } else {
            flags &= ~static_cast<uint32_t>(flag);
        }
    }
};

} // namespace electrical_system
