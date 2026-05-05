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
#include "physics_core/repair_system.h"
#include <algorithm>
#include <cmath>
#include <iostream> // For debugging, remove in production

namespace physics_core {

// ============================================================================
// REPAIR SYSTEM IMPLEMENTATION
// ============================================================================

void RepairSystem::process_field_repairs(float delta_time) {
    // SIMD processing for active repairs
    const size_t batch_size = 8;
    for (size_t i = 0; i < active_repairs_.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, active_repairs_.size());
        
        // SIMD update for repair progress
        for (size_t j = i; j < end; ++j) {
            ComponentRepairState& repair = active_repairs_[j];
            if (repair.is_under_repair && repair.has_required_parts && repair.has_required_tools) {
                // Update progress
                float progress_increment = repair.repair_rate_per_second * delta_time;
                repair.repair_progress = std::min(1.0f, repair.repair_progress + progress_increment);
                repair.time_remaining_seconds = std::max(0.0f, repair.time_remaining_seconds - delta_time);
                
                // Check for completion
                if (repair.repair_progress >= 1.0f) {
                    complete_repair_operation(repair.operation_id); // Use real operation ID
                }
            }
        }
    }
    
    // Update vehicle repair states
    for (auto& vehicle_repair : vehicle_repair_states_) {
        if (vehicle_repair.is_repairing) {
            // Calculate overall progress
            float total_progress = 0.0f;
            for (const auto& comp_repair : vehicle_repair.component_repairs) {
                total_progress += comp_repair.repair_progress;
            }
            total_progress /= vehicle_repair.component_repairs.size();
            
            // Update time to completion
            vehicle_repair.time_to_completion_minutes = 
                (1.0f - total_progress) * 60.0f / vehicle_repair.overall_repair_efficiency;
        }
    }
}

void RepairSystem::sync_repair_to_damage_state(EntityID component_entity,
                                             const ComponentRepairState& repair_state,
                                             ComponentDamageState& damage_state) {
    // Restore health based on repair progress
    float health_restored = repair_state.repair_progress * damage_state.max_health;
    damage_state.current_health = std::min(damage_state.max_health, 
                                         damage_state.current_health + health_restored);
    
    // Update structural integrity
    damage_state.structural_integrity = std::min(1.0f, 
                                                damage_state.structural_integrity + repair_state.repair_progress);
    
    // Clear damage flags if fully repaired
    if (repair_state.repair_progress >= 1.0f) {
        damage_state.damage_type_flags = 0;
        damage_state.damage_level = 0;
        damage_state.cumulative_stress = 0.0f;
    }
}

bool RepairSystem::start_repair_operation(const RepairOperation& operation) {
    // Find or create component repair state
    ComponentRepairState* repair_state = get_component_repair_state(operation.component_entity);
    if (!repair_state) {
        active_repairs_.emplace_back();
        repair_state = &active_repairs_.back();
        repair_state->component_entity = operation.component_entity;
    }
    
    // Assign unique operation ID
    static uint32_t next_operation_id = 1;
    repair_state->operation_id = next_operation_id++;
    
    // Check logistics for parts availability
    repair_state->has_required_parts = true;
    if (logistics_system_) {
        for (const auto& part : operation.required_parts) {
            uint32_t available = logistics_system_->check_part_availability(part.part_id, operation.repair_unit_entity);
            if (available < part.quantity_needed) {
                repair_state->has_required_parts = false;
                break;
            }
        }
    }
    
    // Store required parts and tools
    repair_state->required_parts = operation.required_parts;
    repair_state->required_tools = operation.required_tools;
    
    // Check tools availability (simplified - would check ToolInventory of repair unit)
    repair_state->has_required_tools = true; // Assume tools are available for now
    // Check tools availability - verify repair unit has all required tools
    repair_state->has_required_tools = true;  // Optimistic start
    
    // Verify each tool type is available in the repair unit
    // Tools include: welders, hydraulic presses, electronic testers, etc.
    for (const auto& tool : operation.required_tools) {
        // Query tool inventory/availability
        // This would typically check if repair_unit_entity has tool with id=tool.tool_id
        // For now, implement a check with simple heuristics
        
        bool tool_available = false;
        
        // Tool availability check (would normally query a ToolInventory system)
        if (tool.tool_id == 1) {  // Welding equipment
            tool_available = true;  // Assume all repair units have basic welding
        } else if (tool.tool_id == 2) {  // Hydraulic press
            tool_available = true;  // Standard heavy equipment
        } else if (tool.tool_id == 3) {  // Electronic tester
            tool_available = true;  // Standard diagnostics
        } else if (tool.tool_id >= 100) {  // Specialized tools - may not be available
            tool_available = false;  // More restrictive for specialized equipment
        }
        
        if (!tool_available) {
            repair_state->has_required_tools = false;
            break;
        }
    }
    
    if (!repair_state->has_required_parts || !repair_state->has_required_tools) {
        return false; // Cannot start repair without parts/tools
    }
    
    // Initialize repair state
    repair_state->repair_unit_entity = operation.repair_unit_entity;
    repair_state->repair_progress = 0.0f;
    repair_state->repair_rate_per_second = 1.0f / (operation.estimated_time_minutes * 60.0f);
    repair_state->time_remaining_seconds = operation.estimated_time_minutes * 60.0f;
    repair_state->repair_priority = operation.priority_level;
    repair_state->is_under_repair = true;
    repair_state->repair_location = operation.repair_location;
    
    return true;
}

void RepairSystem::interrupt_repair_operation(uint32_t operation_id) {
    for (auto& repair : active_repairs_) {
        if (repair.operation_id == operation_id && repair.is_under_repair) {
            repair.is_interrupted = true;
            repair.interruption_penalty = 0.2f; // 20% penalty
            repair.repair_rate_per_second *= std::max(0.1f, 1.0f - repair.interruption_penalty);
            break;
        }
    }
}

void RepairSystem::complete_repair_operation(uint32_t operation_id) {
    for (auto& repair : active_repairs_) {
        if (repair.operation_id == operation_id && repair.repair_progress >= 1.0f) {
            repair.is_under_repair = false;
            repair.time_remaining_seconds = 0.0f;
            repair.repair_rate_per_second = 0.0f;
            repair.repair_priority = 0;

            if (logistics_system_) {
                for (const auto& part : repair.required_parts) {
                    logistics_system_->consume_parts(part.part_id, part.quantity_needed, repair.repair_unit_entity);
                }
            }

            ComponentDamageState* damage_state = get_component_damage_state(repair.component_entity);
            if (damage_state) {
                sync_repair_to_damage_state(repair.component_entity, repair, *damage_state);
            }
            break;
        }
    }
}

ComponentRepairState* RepairSystem::get_component_repair_state(EntityID component_entity) {
    for (auto& repair : active_repairs_) {
        if (repair.component_entity == component_entity) {
            return &repair;
        }
    }
    return nullptr;
}

ComponentDamageState* RepairSystem::get_component_damage_state(EntityID component_entity) {
    for (auto& damage_state : damage_states_) {
        if (damage_state.component_entity == component_entity) {
            return &damage_state;
        }
    }
    return nullptr;
}

VehicleRepairState* RepairSystem::get_vehicle_repair_state(EntityID vehicle_entity) {
    for (auto& vehicle_repair : vehicle_repair_states_) {
        if (vehicle_repair.vehicle_entity == vehicle_entity) {
            return &vehicle_repair;
        }
    }
    return nullptr;
}

const std::vector<ComponentRepairState>& RepairSystem::get_active_repairs() const {
    return active_repairs_;
}

const std::vector<VehicleRepairState>& RepairSystem::get_vehicle_repair_states() const {
    return vehicle_repair_states_;
}

// ============================================================================
// LOGISTICS SYSTEM IMPLEMENTATION
// ============================================================================

void LogisticsSystem::manage_supply_chain(float delta_time) {
    // Generate resupply orders based on demand
    generate_resupply_orders();
    
    // Execute transport operations
    execute_transport_operations(delta_time);
    
    // Update inventory levels
    for (auto& node : supply_nodes_) {
        for (auto& inventory : node.inventory) {
            // Simulate degradation over time
            inventory.degradation_factor += delta_time * 0.001f; // Slow degradation
            inventory.degradation_factor = std::min(1.0f, inventory.degradation_factor);
        }
    }
}

bool LogisticsSystem::request_parts_from_logistics(const std::vector<RequiredPart>& needed_parts,
                                                 EntityID requesting_location,
                                                 float urgency_level) {
    bool all_available = true;
    
    for (const auto& part : needed_parts) {
        uint32_t available = check_part_availability(part.part_id, requesting_location);
        if (available < part.quantity_needed) {
            all_available = false;
            // Generate resupply order
            ResupplyOrder order;
            order.part_id = part.part_id;
            order.quantity_requested = part.quantity_needed - available;
            order.requesting_node = requesting_location;
            order.urgency_level = urgency_level;
            pending_orders_.push_back(order);
        }
    }
    
    return all_available;
}

bool LogisticsSystem::reserve_parts(uint32_t part_id, uint32_t quantity, EntityID location) {
    for (auto& node : supply_nodes_) {
        if (node.node_entity == location) {
            for (auto& inventory : node.inventory) {
                if (inventory.part_id == part_id && inventory.quantity_available >= quantity) {
                    inventory.quantity_available -= quantity;
                    inventory.quantity_reserved += quantity;
                    return true;
                }
            }
        }
    }
    return false;
}

bool LogisticsSystem::consume_parts(uint32_t part_id, uint32_t quantity, EntityID location) {
    for (auto& node : supply_nodes_) {
        if (node.node_entity == location) {
            for (auto& inventory : node.inventory) {
                if (inventory.part_id == part_id && inventory.quantity_reserved >= quantity) {
                    inventory.quantity_reserved -= quantity;
                    return true;
                }
            }
        }
    }
    return false;
}

uint32_t LogisticsSystem::check_part_availability(uint32_t part_id, EntityID location) {
    for (const auto& node : supply_nodes_) {
        if (node.node_entity == location) {
            for (const auto& inventory : node.inventory) {
                if (inventory.part_id == part_id) {
                    return inventory.quantity_available;
                }
            }
        }
    }
    return 0;
}

void LogisticsSystem::generate_resupply_orders() {
    // Simplified: Generate orders for low inventory
    for (auto& node : supply_nodes_) {
        for (auto& inventory : node.inventory) {
            if (inventory.quantity_available < inventory.maximum_capacity * 0.2f) {
                ResupplyOrder order;
                order.part_id = inventory.part_id;
                order.quantity_requested = inventory.maximum_capacity - inventory.quantity_available;
                order.requesting_node = node.node_entity;
                order.urgency_level = 0.5f;
                pending_orders_.push_back(order);
            }
        }
    }
}

void LogisticsSystem::execute_transport_operations(float delta_time) {
    // Execute transport operations
    for (auto it = transport_requests_.begin(); it != transport_requests_.end(); ) {
        TransportRequest& request = *it;
        
        // Find supply route
        const SupplyRoute* route = nullptr;
        for (const auto& r : supply_routes_) {
            if (r.source_node == request.source_node && r.destination_node == request.destination_node) {
                route = &r;
                break;
            }
        }
        
        if (route) {
            // Update transport time
            request.time_remaining_seconds -= delta_time;
            
            if (request.time_remaining_seconds <= 0.0f) {
                // Transport completed - deliver parts
                for (auto& node : supply_nodes_) {
                    if (node.node_entity == request.destination_node) {
                        for (const auto& part : request.parts_to_transport) {
                            // Add parts to destination inventory
                            bool found = false;
                            for (auto& inventory : node.inventory) {
                                if (inventory.part_id == part.part_id) {
                                    inventory.quantity_available += part.quantity_available;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                PartInventory new_inventory = part;
                                new_inventory.quantity_available = part.quantity_available;
                                new_inventory.quantity_reserved = 0;
                                new_inventory.quantity_in_transit = 0;
                                node.inventory.push_back(new_inventory);
                            }
                        }
                        break;
                    }
                }
                
                // Remove completed transport
                it = transport_requests_.erase(it);
                continue;
            }
        }
        
        ++it;
    }
}

const std::vector<SparePartDefinition>& LogisticsSystem::get_part_catalog() const {
    return part_catalog_;
}

const std::vector<SupplyChainNode>& LogisticsSystem::get_supply_nodes() const {
    return supply_nodes_;
}

// ============================================================================
// REPAIR SCHEDULER IMPLEMENTATION
// ============================================================================

void RepairScheduler::assign_repairs_to_units(const std::vector<RepairOperation>& available_operations,
                                            const std::vector<RepairUnitCapabilities>& available_units) {
    current_assignments_.clear();
    
    // Simple assignment: match operations to units with matching specialties
    for (const auto& operation : available_operations) {
        for (const auto& unit : available_units) {
            bool can_assign = false;
            for (uint32_t specialty : unit.repair_specialties) {
                if (specialty == operation.operation_type) {
                    can_assign = true;
                    break;
                }
            }
            
            if (can_assign && current_assignments_.size() < unit.max_simultaneous_repairs) {
                RepairAssignment assignment;
                assignment.repair_unit_entity = unit.repair_unit_entity;
                assignment.operation_id = operation.operation_id;
                assignment.operation_type = operation.operation_type;
                assignment.assignment_efficiency = unit.repair_speed_multiplier * unit.skill_level;
                current_assignments_.push_back(assignment);
                break;
            }
        }
    }
}

void RepairScheduler::schedule_repair_operations(RepairOperationQueue& queue) {
    // Move high priority operations to active
    while (!queue.priority_queue.empty() && queue.active_operations.size() < queue.available_repair_units) {
        RepairOperation op = queue.priority_queue.top();
        queue.priority_queue.pop();
        op.status = static_cast<uint32_t>(RepairOperationStatus::IN_PROGRESS);
        queue.active_operations.push_back(std::move(op));
    }
}

void RepairScheduler::balance_repair_workload(const std::vector<RepairUnitCapabilities>& units) {
    // Calculate current workload for each unit
    std::vector<int> current_workloads(units.size(), 0);
    
    for (const auto& assignment : current_assignments_) {
        for (size_t i = 0; i < units.size(); ++i) {
            if (units[i].repair_unit_entity == assignment.repair_unit_entity) {
                current_workloads[i]++;
                break;
            }
        }
    }
    
    // Find overloaded and underloaded units
    std::vector<size_t> overloaded_units;
    std::vector<size_t> underloaded_units;
    
    for (size_t i = 0; i < units.size(); ++i) {
        float utilization = static_cast<float>(current_workloads[i]) / units[i].max_simultaneous_repairs;
        if (utilization > 0.8f) { // Over 80% capacity
            overloaded_units.push_back(i);
        } else if (utilization < 0.3f) { // Under 30% capacity
            underloaded_units.push_back(i);
        }
    }
    
    // Reassign operations from overloaded to underloaded units
    for (size_t overloaded_idx : overloaded_units) {
        // Find operations assigned to this unit
        std::vector<RepairAssignment> operations_to_reassign;
        for (auto it = current_assignments_.begin(); it != current_assignments_.end(); ) {
            if (it->repair_unit_entity == units[overloaded_idx].repair_unit_entity) {
                operations_to_reassign.push_back(*it);
                it = current_assignments_.erase(it);
            } else {
                ++it;
            }
        }
        
        // Reassign to underloaded units
        for (const auto& op : operations_to_reassign) {
            bool reassigned = false;
            for (size_t underloaded_idx : underloaded_units) {
                // Check if unit can handle this operation type
                bool can_handle = false;
                for (uint32_t specialty : units[underloaded_idx].repair_specialties) {
                    if (specialty == op.operation_type) {
                        can_handle = true;
                        break;
                    }
                }
                
                if (can_handle && current_workloads[underloaded_idx] < units[underloaded_idx].max_simultaneous_repairs) {
                    RepairAssignment new_assignment = op;
                    new_assignment.repair_unit_entity = units[underloaded_idx].repair_unit_entity;
                    current_assignments_.push_back(new_assignment);
                    current_workloads[underloaded_idx]++;
                    reassigned = true;
                    break;
                }
            }
            
            // If couldn't reassign, put back to original unit
            if (!reassigned) {
                current_assignments_.push_back(op);
            }
        }
    }
}

void RepairScheduler::update_repair_priorities(const std::vector<EntityID>& critical_vehicles) {
    // Increase assignment efficiency when critical vehicles are present.
    if (critical_vehicles.empty()) {
        return;
    }

    for (auto& assignment : current_assignments_) {
        assignment.assignment_efficiency = std::min(1.0f, assignment.assignment_efficiency * 1.1f);
    }
}

}  // namespace physics_core