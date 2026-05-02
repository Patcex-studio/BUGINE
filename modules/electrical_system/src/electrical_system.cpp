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
#include "electrical_system/electrical_system.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace electrical_system {

ElectricalSystem::ElectricalSystem(size_t max_vehicles)
    : vehicles_(max_vehicles) {
}

void ElectricalSystem::update(float dt, ecs::EntityManager* em) {
    entity_manager_ = em;
    ++frame_id_;
    
    // Process each vehicle's electrical grid
    for (auto& vdata : vehicles_) {
        if (!vdata.grid) continue;
        
        auto& grid = *vdata.grid;
        
        // Step 1: Accumulate power from all generators
        accumulate_generators(vdata);
        grid.total_generated_power_w = std::accumulate(
            vdata.generators.begin(),
            vdata.generators.end(),
            0.0f,
            [](float sum, GeneratorComponent* gen) {
                return sum + gen->get_effective_power();
            }
        );
        
        // Step 2: Gather active consumers
        size_t consumer_count = gather_consumers(vdata, 
                                                {temp_consumer_buffer_.data(), 
                                                 std::min(vdata.consumers.size(),
                                                        MAX_CONSUMERS_PER_VEHICLE)});
        
        // Step 3: Distribute power and handle overload
        distribute_power(vdata,
                        {temp_consumer_buffer_.data(), consumer_count},
                        dt);
        
        // Step 4: Update battery state
        update_battery(grid, grid.total_generated_power_w, 
                      grid.total_consumed_power_w, dt);
        
        // Step 5: Check critical battery state
        process_battery_critical(grid, vdata, dt);
        
        // Step 6: Handle cascade effects
        if (grid.damage_flags & static_cast<uint32_t>(DamageFlag::SHORT_CIRCUIT)) {
            handle_short_circuit(vdata.grid->parent_vehicle_id, grid);
            // Reset short circuit flag after processing (one-time event)
            set_damage_flag(grid.damage_flags, DamageFlag::SHORT_CIRCUIT, false);
        }
    }
}

void ElectricalSystem::on_grid_created(EntityID vehicle_id, 
                                       ElectricalGridComponent* grid) {
    if (vehicle_index_map_.find(vehicle_id) != vehicle_index_map_.end()) {
        return; // Already registered
    }
    
    // Find first free slot
    auto it = std::find_if(vehicles_.begin(), vehicles_.end(),
                           [](const VehicleElectricalData& v) { return !v.grid; });
    
    if (it == vehicles_.end()) {
        vehicles_.resize(vehicles_.size() * 2); // Grow if needed
        it = std::find_if(vehicles_.begin(), vehicles_.end(),
                         [](const VehicleElectricalData& v) { return !v.grid; });
    }
    
    size_t index = std::distance(vehicles_.begin(), it);
    vehicles_[index].grid = grid;
    vehicle_index_map_[vehicle_id] = index;
}

void ElectricalSystem::on_grid_destroyed(EntityID vehicle_id) {
    auto it = vehicle_index_map_.find(vehicle_id);
    if (it == vehicle_index_map_.end()) return;
    
    size_t index = it->second;
    vehicles_[index].grid = nullptr;
    vehicles_[index].generators.clear();
    vehicles_[index].consumers.clear();
    vehicles_[index].consumer_entity_ids.clear();
    vehicle_index_map_.erase(it);
}

void ElectricalSystem::on_generator_created(EntityID entity_id,
                                           GeneratorComponent* gen,
                                           EntityID parent_vehicle) {
    auto it = vehicle_index_map_.find(parent_vehicle);
    if (it == vehicle_index_map_.end()) return;
    
    size_t v_idx = it->second;
    if (vehicles_[v_idx].generators.size() < MAX_GENERATORS_PER_VEHICLE) {
        vehicles_[v_idx].generators.push_back(gen);
        generator_to_vehicle_[entity_id] = parent_vehicle;
    }
}

void ElectricalSystem::on_generator_destroyed(EntityID entity_id) {
    auto it = generator_to_vehicle_.find(entity_id);
    if (it == generator_to_vehicle_.end()) return;
    
    EntityID vehicle_id = it->second;
    auto v_it = vehicle_index_map_.find(vehicle_id);
    if (v_it == vehicle_index_map_.end()) return;
    
    auto& gens = vehicles_[v_it->second].generators;
    gens.erase(std::remove_if(gens.begin(), gens.end(),
                             [](GeneratorComponent* g) { return g == nullptr; }),
              gens.end());
    generator_to_vehicle_.erase(it);
}

void ElectricalSystem::on_consumer_created(EntityID entity_id,
                                          ConsumerComponent* consumer,
                                          EntityID parent_vehicle) {
    auto it = vehicle_index_map_.find(parent_vehicle);
    if (it == vehicle_index_map_.end()) return;
    
    size_t v_idx = it->second;
    if (vehicles_[v_idx].consumers.size() < MAX_CONSUMERS_PER_VEHICLE) {
        vehicles_[v_idx].consumers.push_back(consumer);
        vehicles_[v_idx].consumer_entity_ids.push_back(entity_id);
        consumer_to_vehicle_[entity_id] = parent_vehicle;
    }
}

void ElectricalSystem::on_consumer_destroyed(EntityID entity_id) {
    auto it = consumer_to_vehicle_.find(entity_id);
    if (it == consumer_to_vehicle_.end()) return;
    
    EntityID vehicle_id = it->second;
    auto v_it = vehicle_index_map_.find(vehicle_id);
    if (v_it == vehicle_index_map_.end()) return;
    
    size_t v_idx = v_it->second;
    auto& cons = vehicles_[v_idx].consumers;
    auto& ids = vehicles_[v_idx].consumer_entity_ids;
    
    // Find and remove
    for (size_t i = 0; i < cons.size(); ++i) {
        if (ids[i] == entity_id) {
            cons.erase(cons.begin() + i);
            ids.erase(ids.begin() + i);
            break;
        }
    }
    consumer_to_vehicle_.erase(it);
}

ElectricalGridComponent* ElectricalSystem::get_grid(EntityID vehicle_id) {
    auto it = vehicle_index_map_.find(vehicle_id);
    if (it == vehicle_index_map_.end()) return nullptr;
    return vehicles_[it->second].grid;
}

const ElectricalGridComponent* ElectricalSystem::get_grid(EntityID vehicle_id) const {
    auto it = vehicle_index_map_.find(vehicle_id);
    if (it == vehicle_index_map_.end()) return nullptr;
    return vehicles_[it->second].grid;
}

size_t ElectricalSystem::gather_consumers(VehicleElectricalData& vdata,
                                          std::span<ConsumerEntry> buffer) {
    size_t count = 0;
    for (size_t i = 0; i < vdata.consumers.size() && count < buffer.size(); ++i) {
        auto* consumer = vdata.consumers[i];
        if (!consumer) continue;
        
        buffer[count] = {
            .entity_id = vdata.consumer_entity_ids[i],
            .required_power_w = consumer->required_power_w,
            .priority = consumer->priority,
            .is_active = consumer->is_active.load(std::memory_order_acquire),
            .is_damaged = consumer->is_damaged.load(std::memory_order_acquire)
        };
        ++count;
    }
    return count;
}

void ElectricalSystem::accumulate_generators(VehicleElectricalData& vdata) {
    for (auto* gen : vdata.generators) {
        if (!gen) continue;
        if (gen->is_damaged || !gen->is_running) {
            gen->current_power_w = 0.0f;
        } else {
            gen->current_power_w = gen->max_power_w;
        }
    }
}

void ElectricalSystem::distribute_power(VehicleElectricalData& vdata,
                                       std::span<ConsumerEntry> consumers,
                                       float dt) {
    auto& grid = *vdata.grid;
    grid.total_consumed_power_w = 0.0f;
    
    // Calculate total consumption
    for (const auto& consumer : consumers) {
        if (consumer.is_active && !consumer.is_damaged) {
            grid.total_consumed_power_w += consumer.required_power_w;
        }
    }
    
    // Check for overload
    float deficit = grid.total_consumed_power_w - grid.total_generated_power_w;
    if (deficit > 0.0f) {
        handle_overload(grid, consumers, deficit);
        grid.is_overloaded.store(true, std::memory_order_release);
    } else {
        grid.is_overloaded.store(false, std::memory_order_release);
    }
}

void ElectricalSystem::handle_overload(ElectricalGridComponent& grid,
                                      std::span<ConsumerEntry> consumers,
                                      float deficit) {
    // Sort consumers by priority (deterministic)
    sort_consumers_by_priority(consumers);
    
    float remaining_deficit = deficit;
    
    // Disconnect consumers from highest priority (optional) to lowest
    for (int i = static_cast<int>(consumers.size()) - 1; i >= 0 && remaining_deficit > 0.0f; --i) {
        auto& consumer = consumers[i];
        
        // Only disconnect optional systems (high priority value > 0.5)
        if (consumer.is_active && !consumer.is_damaged && consumer.priority > 0.5f) {
            consumer.is_active = 0;
            
            // Find and update the actual consumer component
            auto it = consumer_to_vehicle_.find(consumer.entity_id);
            if (it != consumer_to_vehicle_.end()) {
                EntityID vehicle_id = it->second;
                auto v_it = vehicle_index_map_.find(vehicle_id);
                if (v_it != vehicle_index_map_.end()) {
                    auto& vdata = vehicles_[v_it->second];
                    for (size_t j = 0; j < vdata.consumer_entity_ids.size(); ++j) {
                        if (vdata.consumer_entity_ids[j] == consumer.entity_id) {
                            vdata.consumers[j]->is_active.store(0, 
                                                                 std::memory_order_release);
                            
                            // Invoke callback if present
                            if (vdata.consumers[j]->on_power_lost) {
                                vdata.consumers[j]->on_power_lost(
                                    consumer.entity_id,
                                    PowerLossReason::OVERLOAD
                                );
                            }
                            break;
                        }
                    }
                }
            }
            
            remaining_deficit -= consumer.required_power_w;
        }
    }
    
    // If still in deficit, tap battery
    grid.total_consumed_power_w = std::min(grid.total_consumed_power_w,
                                          grid.total_generated_power_w + 
                                          grid.battery_charge_w_h);
}

void ElectricalSystem::update_battery(ElectricalGridComponent& grid,
                                     float generated,
                                     float consumed,
                                     float dt) {
    float balance = (generated - consumed) * dt / 3600.0f; // Convert seconds to hours
    
    float new_charge = grid.battery_charge_w_h + balance;
    grid.battery_charge_w_h = std::clamp(new_charge, 
                                         0.0f, 
                                         grid.max_battery_charge_w_h);
    
    // Apply efficiency losses
    if (balance > 0.0f) {
        grid.battery_charge_w_h *= grid.charge_efficiency;
    } else {
        grid.battery_charge_w_h *= grid.discharge_efficiency;
    }
}

void ElectricalSystem::process_battery_critical(ElectricalGridComponent& grid,
                                               VehicleElectricalData& vdata,
                                               float dt) {
    float battery_pct = grid.get_battery_percentage();
    
    if (battery_pct < grid.battery_critical_threshold) {
        grid.battery_dead_timer += dt;
        
        if (grid.battery_dead_timer > BATTERY_DEAD_THRESHOLD) {
            set_damage_flag(grid.damage_flags, DamageFlag::BATTERY_DEAD, true);
            
            // Disable all consumers
            for (auto* consumer : vdata.consumers) {
                if (consumer) {
                    consumer->is_active.store(0, std::memory_order_release);
                }
            }
        }
    } else {
        grid.battery_dead_timer = 0.0f;
        set_damage_flag(grid.damage_flags, DamageFlag::BATTERY_DEAD, false);
    }
}

void ElectricalSystem::handle_short_circuit(EntityID grid_id,
                                           const ElectricalGridComponent& grid) {
    // Short circuit = immediate energy loss + cascade damage
    // This is typically called from DamageSystem integration
    // For now, implementation is placeholder for event cascade
}

void ElectricalSystem::sort_consumers_by_priority(std::span<ConsumerEntry> consumers) {
    // Build priority array
    for (size_t i = 0; i < consumers.size(); ++i) {
        priority_buffer_[i] = {
            .tier = static_cast<uint8_t>(consumers[i].priority * 10.0f),
            .entity_id = static_cast<uint16_t>(consumers[i].entity_id & 0xFFFF)
        };
    }
    
    // Sort indices by priority (deterministic)
    std::sort(priority_buffer_.begin(),
             priority_buffer_.begin() + consumers.size());
}

uint16_t ElectricalSystem::quantize_battery(float charge, float max_charge) {
    if (max_charge <= 0.0f) return 0;
    float ratio = std::clamp(charge / max_charge, 0.0f, 1.0f);
    return static_cast<uint16_t>(ratio * 1023.0f);
}

float ElectricalSystem::dequantize_battery(uint16_t quantized, float max_charge) {
    return (quantized / 1023.0f) * max_charge;
}

NetworkElectricalState ElectricalSystem::get_network_state(EntityID grid_id) const {
    auto* grid = get_grid(grid_id);
    if (!grid) {
        return NetworkElectricalState{.grid_id = 0};
    }
    
    return {
        .grid_id = grid_id,
        .battery_charge_q = quantize_battery(grid->battery_charge_w_h, 
                                            grid->max_battery_charge_w_h),
        .damage_flags = static_cast<uint8_t>(grid->damage_flags & 0xFF),
        .active_consumers_mask = 0  // Would be populated with actual consumer states
    };
}

void ElectricalSystem::apply_network_state(EntityID grid_id,
                                          const NetworkElectricalState& state) {
    auto* grid = get_grid(grid_id);
    if (!grid) return;
    
    grid->battery_charge_w_h = dequantize_battery(state.battery_charge_q,
                                                 grid->max_battery_charge_w_h);
    grid->damage_flags = state.damage_flags;
}

void ElectricalSystem::sync_generator_with_engine(EntityID vehicle_id, 
                                                  bool engine_running) {
    auto it = vehicle_index_map_.find(vehicle_id);
    if (it == vehicle_index_map_.end()) return;
    
    size_t v_idx = it->second;
    for (auto* gen : vehicles_[v_idx].generators) {
        if (gen) {
            gen->is_running = engine_running;
        }
    }
}

#ifdef ENABLE_ELECTRICAL_DEBUG
void ElectricalSystem::debug_inject_overload(EntityID grid_id, float excess_power) {
    auto* grid = get_grid(grid_id);
    if (grid) {
        grid->total_consumed_power_w += excess_power;
        std::cerr << "[ELECTRICAL DEBUG] Injected overload " << excess_power 
                  << "W on grid " << grid_id << std::endl;
    }
}

void ElectricalSystem::debug_inject_short_circuit(EntityID grid_id) {
    auto* grid = get_grid(grid_id);
    if (grid) {
        set_damage_flag(grid->damage_flags, DamageFlag::SHORT_CIRCUIT, true);
        std::cerr << "[ELECTRICAL DEBUG] Injected short circuit on grid " 
                  << grid_id << std::endl;
    }
}

void ElectricalSystem::debug_reset_grid(EntityID grid_id) {
    auto* grid = get_grid(grid_id);
    if (grid) {
        grid->battery_charge_w_h = grid->max_battery_charge_w_h;
        grid->damage_flags = 0;
        grid->battery_dead_timer = 0.0f;
        std::cerr << "[ELECTRICAL DEBUG] Reset grid " << grid_id << std::endl;
    }
}
#endif

} // namespace electrical_system
