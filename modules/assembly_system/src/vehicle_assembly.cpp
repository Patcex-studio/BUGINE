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
#include "assembly_system/vehicle_assembly.h"

#include <cmath>
#include <iterator>
#include <unordered_set>

namespace assembly_system {

static float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

bool DamagePropagationSystem::add_node(DamagePropagationNode node) {
    if (node.component_entity == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(graph_mutex_);
    propagation_graph_[node.component_entity] = std::move(node);
    return true;
}

bool DamagePropagationSystem::add_connection(EntityID source_component, DamageConnection connection) {
    if (source_component == 0 || connection.target_component == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(graph_mutex_);
    auto it = propagation_graph_.find(source_component);
    if (it == propagation_graph_.end()) {
        return false;
    }

    it->second.connections.push_back(connection);
    if (connection.is_bidirectional) {
        DamageConnection reverse = connection;
        reverse.target_component = source_component;
        it = propagation_graph_.find(connection.target_component);
        if (it != propagation_graph_.end()) {
            it->second.connections.push_back(reverse);
        }
    }
    return true;
}

bool DamagePropagationSystem::propagate_damage_from_hit(const DamageEvent& initial_hit, std::vector<DamageResult>& results) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    results.clear();

    if (initial_hit.hit_component_id == 0) {
        return false;
    }

    auto root_it = propagation_graph_.find(initial_hit.hit_component_id);
    if (root_it == propagation_graph_.end()) {
        return false;
    }

    std::queue<std::pair<EntityID, float>> work_queue;
    work_queue.push({initial_hit.hit_component_id, initial_hit.hit_strength});

    std::unordered_map<EntityID, float> accumulated_damage;
    std::unordered_set<EntityID> visited;
    visited.insert(initial_hit.hit_component_id);

    while (!work_queue.empty()) {
        auto [component_id, current_damage] = work_queue.front();
        work_queue.pop();

        auto node_it = propagation_graph_.find(component_id);
        if (node_it == propagation_graph_.end()) {
            continue;
        }

        const DamagePropagationNode& node = node_it->second;
        float applied_damage = current_damage * node.damage_multiplier;
        if (applied_damage <= 0.0f) {
            continue;
        }

        accumulated_damage[component_id] = std::max(accumulated_damage[component_id], applied_damage);

        for (const DamageConnection& connection : node.connections) {
            float next_damage = applied_damage * connection.propagation_factor;
            if (next_damage <= 0.01f) {
                continue;
            }

            if (visited.insert(connection.target_component).second) {
                work_queue.push({connection.target_component, next_damage});
            }
        }
    }

    for (const auto& [component_entity, applied_damage] : accumulated_damage) {
        results.push_back({component_entity, applied_damage, applied_damage >= propagation_graph_[component_entity].critical_threshold});
    }

    return !results.empty();
}

void DamagePropagationSystem::clear() {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    propagation_graph_.clear();
}

ModularVehicleSystem::ModularVehicleSystem(ecs::EntityManager* entity_manager)
    : entity_manager_(entity_manager) {
}

void ModularVehicleSystem::set_entity_manager(ecs::EntityManager* entity_manager) {
    entity_manager_ = entity_manager;
}

uint32_t ModularVehicleSystem::socket_type_mask(SocketType type) const noexcept {
    return 1u << static_cast<uint32_t>(type);
}

bool ModularVehicleSystem::is_socket_compatible(const SocketPoint& socket, SocketType type) const noexcept {
    if (socket.compatible_types_mask == UINT32_MAX) {
        return true;
    }
    return (socket.compatible_types_mask & socket_type_mask(type)) != 0;
}

bool ModularVehicleSystem::validate_blueprint(const VehicleBlueprint& blueprint, std::string& error_message) const {
    if (blueprint.hull.structural_integrity <= 0.0f) {
        error_message = "Hull integrity must be greater than zero.";
        return false;
    }

    if (blueprint.hull.socket_count == 0) {
        error_message = "Hull blueprint must define at least one attachment socket.";
        return false;
    }

    if (blueprint.engine.max_power_hp <= 0.0f) {
        error_message = "Engine blueprint must specify a positive maximum power.";
        return false;
    }

    if (blueprint.weapons.ammunition_total < 0.0f) {
        error_message = "Weapon ammunition must be non-negative.";
        return false;
    }

    if (blueprint.control.suspension_health <= 0.0f) {
        error_message = "Control system suspension health must be positive.";
        return false;
    }

    return true;
}

void ModularVehicleSystem::build_damage_graph(EntityID root_entity, const std::vector<EntityID>& component_entities) {
    damage_system_.clear();

    for (EntityID entity : component_entities) {
        DamagePropagationNode node;
        node.component_entity = entity;
        node.damage_multiplier = 1.0f;
        node.critical_threshold = 0.5f;
        node.flags = DAMAGE_PROP_NONE;

        if (entity_manager_->get_component<HullComponent>(entity)) {
            node.damage_multiplier = 1.0f;
            node.flags = DAMAGE_PROP_MECHANICAL | DAMAGE_PROP_ELECTRICAL;
        } else if (entity_manager_->get_component<EngineComponent>(entity)) {
            node.damage_multiplier = 0.85f;
            node.flags = DAMAGE_PROP_MECHANICAL | DAMAGE_PROP_FUEL;
        } else if (entity_manager_->get_component<WeaponsComponent>(entity)) {
            node.damage_multiplier = 0.7f;
            node.flags = DAMAGE_PROP_MECHANICAL | DAMAGE_PROP_ELECTRICAL;
        } else if (entity_manager_->get_component<ControlSystemsComponent>(entity)) {
            node.damage_multiplier = 0.6f;
            node.flags = DAMAGE_PROP_ELECTRICAL | DAMAGE_PROP_HYDRAULIC;
        }

        damage_system_.add_node(std::move(node));
    }

    for (EntityID entity : component_entities) {
        if (entity == root_entity) {
            continue;
        }

        DamageConnection connection;
        connection.target_component = entity;
        connection.propagation_factor = 0.75f;
        connection.connection_type = static_cast<uint32_t>(ConnectionType::MECHANICAL);
        connection.is_bidirectional = true;
        damage_system_.add_connection(root_entity, connection);
    }
}

bool ModularVehicleSystem::assemble_vehicle_from_blueprint(const VehicleBlueprint& blueprint, EntityID& root_entity, std::string& error_message) {
    std::scoped_lock lock(assembly_mutex_);
    if (!entity_manager_) {
        error_message = "Entity manager is not available.";
        return false;
    }

    if (!validate_blueprint(blueprint, error_message)) {
        return false;
    }

    root_entity = entity_manager_->create_entity();
    HullComponent hull = blueprint.hull;
    hull.entity_id = root_entity;
    entity_manager_->add_component(root_entity, hull);

    std::vector<EntityID> component_entities;
    component_entities.push_back(root_entity);

    EntityID engine_entity = entity_manager_->create_entity();
    EngineComponent engine = blueprint.engine;
    engine.entity_id = engine_entity;
    entity_manager_->add_component(engine_entity, engine);
    component_entities.push_back(engine_entity);

    EntityID weapons_entity = entity_manager_->create_entity();
    WeaponsComponent weapons = blueprint.weapons;
    weapons.entity_id = weapons_entity;
    entity_manager_->add_component(weapons_entity, weapons);
    component_entities.push_back(weapons_entity);

    EntityID control_entity = entity_manager_->create_entity();
    ControlSystemsComponent control = blueprint.control;
    control.entity_id = control_entity;
    entity_manager_->add_component(control_entity, control);
    component_entities.push_back(control_entity);

    if (hull.socket_count > 0 && engine.entity_id != 0) {
        hull.attachment_sockets[0].connected_entity = engine_entity;
        hull.attachment_sockets[0].connection_strength = 1.0f;
        entity_manager_->get_component<HullComponent>(root_entity)->attachment_sockets[0] = hull.attachment_sockets[0];
    }

    if (hull.socket_count > 1 && weapons.entity_id != 0) {
        hull.attachment_sockets[1].connected_entity = weapons_entity;
        hull.attachment_sockets[1].connection_strength = 0.9f;
        entity_manager_->get_component<HullComponent>(root_entity)->attachment_sockets[1] = hull.attachment_sockets[1];
    }

    build_damage_graph(root_entity, component_entities);
    vehicle_component_index_[root_entity] = std::move(component_entities);
    return true;
}

void ModularVehicleSystem::process_component_events(float delta_time) {
    ComponentEvent event;
    while (event_queue_.pop(event)) {
        process_event(event);
    }
}

bool ModularVehicleSystem::enqueue_component_event(const ComponentEvent& event) {
    return event_queue_.push(event);
}

void ModularVehicleSystem::process_event(const ComponentEvent& event) {
    switch (event.event_type) {
        case ComponentEventType::POWER_REQUEST: {
            if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(event.target_entity)) {
                float efficiency = clamp01(control->electronics_health) * event.intensity;
                control->steering_response = std::min(1.0f, control->steering_response + efficiency * 0.1f);
            }
            break;
        }
        case ComponentEventType::FUEL_FLOW: {
            if (auto* engine = entity_manager_->get_component<EngineComponent>(event.target_entity)) {
                float flow = std::clamp(event.intensity, 0.0f, engine->max_power_hp * 0.01f);
                engine->fuel_current_liters = std::max(0.0f, engine->fuel_current_liters - flow * 0.01f);
            }
            break;
        }
        case ComponentEventType::DAMAGE_RECEIVED: {
            std::vector<DamageResult> results;
            DamageEvent damage{event.target_entity, event.intensity, Vec3(), event.intensity * 0.5f};
            receive_ballistic_damage(damage, event.target_entity, results);
            break;
        }
        case ComponentEventType::SYSTEM_FAILURE: {
            if (auto* engine = entity_manager_->get_component<EngineComponent>(event.target_entity)) {
                engine->is_operational = false;
            }
            if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(event.target_entity)) {
                control->comms_operational = false;
            }
            break;
        }
        case ComponentEventType::CONTROL_INPUT: {
            if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(event.target_entity)) {
                control->steering_response = std::clamp(control->steering_response + event.intensity * 0.01f, 0.0f, 1.0f);
            }
            break;
        }
        case ComponentEventType::SENSOR_DATA: {
            if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(event.target_entity)) {
                control->electronics_health = std::min(1.0f, control->electronics_health + 0.001f * event.intensity);
            }
            break;
        }
        default:
            break;
    }
}

float ModularVehicleSystem::apply_damage_to_component(EntityID component_entity, float damage) {
    if (auto* hull = entity_manager_->get_component<HullComponent>(component_entity)) {
        float applied = damage * 0.01f;
        hull->structural_integrity = clamp01(hull->structural_integrity - applied);
        return applied;
    }

    if (auto* engine = entity_manager_->get_component<EngineComponent>(component_entity)) {
        float applied = damage * 0.02f;
        engine->engine_health = clamp01(engine->engine_health - applied);
        if (engine->engine_health < 0.25f) {
            engine->is_operational = false;
        }
        return applied;
    }

    if (auto* weapons = entity_manager_->get_component<WeaponsComponent>(component_entity)) {
        float applied = damage * 0.015f;
        weapons->barrel_wear = std::min(1.0f, weapons->barrel_wear + applied);
        if (weapons->barrel_wear > 0.9f) {
            weapons->is_jammed = true;
        }
        return applied;
    }

    if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(component_entity)) {
        float applied = damage * 0.018f;
        control->electronics_health = clamp01(control->electronics_health - applied);
        if (control->electronics_health < 0.3f) {
            control->comms_operational = false;
        }
        return applied;
    }

    return 0.0f;
}

bool ModularVehicleSystem::receive_ballistic_damage(const DamageEvent& hit, EntityID vehicle_root, std::vector<DamageResult>& results) {
    if (hit.hit_component_id == 0) {
        return false;
    }

    if (!damage_system_.propagate_damage_from_hit(hit, results)) {
        return false;
    }

    for (const auto& result : results) {
        float applied = apply_damage_to_component(result.component_entity, result.applied_damage);
        if (applied > 0.0f && result.critical_hit) {
            ComponentEvent failure_event;
            failure_event.source_entity = hit.hit_component_id;
            failure_event.target_entity = result.component_entity;
            failure_event.event_type = ComponentEventType::SYSTEM_FAILURE;
            failure_event.intensity = applied;
            failure_event.timestamp = hit.hit_point.x + hit.hit_point.y + hit.hit_point.z;
            enqueue_component_event(failure_event);
        }
    }

    return !results.empty();
}

void ModularVehicleSystem::sync_to_physics_core(EntityID vehicle_root, physics_core::PhysicsCore& physics) {
    auto it = vehicle_component_index_.find(vehicle_root);
    if (it == vehicle_component_index_.end()) {
        return;
    }

    for (EntityID entity : it->second) {
        float mass = 1000.0f;
        if (auto* hull = entity_manager_->get_component<HullComponent>(entity)) {
            mass = 12000.0f * clamp01(hull->structural_integrity);
        } else if (auto* engine = entity_manager_->get_component<EngineComponent>(entity)) {
            mass = 900.0f * clamp01(engine->engine_health);
        } else if (auto* weapons = entity_manager_->get_component<WeaponsComponent>(entity)) {
            mass = 450.0f;
        } else if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(entity)) {
            mass = 350.0f;
        }

        physics.create_rigid_body(Vec3(0.0, 0.0, 0.0), mass, physics_core::Mat3x3::identity());
    }
}

bool ModularVehicleSystem::create_modern_mb_tank(const VehicleBlueprint& specs, EntityID& tank_entity, std::string& error_message) {
    VehicleBlueprint blueprint = specs;
    blueprint.hull.armor_thickness_front = std::max(blueprint.hull.armor_thickness_front, 220.0f);
    blueprint.hull.armor_thickness_side = std::max(blueprint.hull.armor_thickness_side, 180.0f);
    blueprint.hull.armor_thickness_rear = std::max(blueprint.hull.armor_thickness_rear, 150.0f);
    blueprint.hull.structural_integrity = std::clamp(blueprint.hull.structural_integrity, 0.5f, 1.0f);
    blueprint.engine.max_power_hp = std::max(blueprint.engine.max_power_hp, 1200.0f);
    blueprint.control.has_stabilization = true;
    blueprint.weapons.barrel_wear = 0.0f;

    return assemble_vehicle_from_blueprint(blueprint, tank_entity, error_message);
}

void ModularVehicleSystem::process_field_repairs(EntityID vehicle_root, const RepairOrder& repair_order) {
    const auto it = vehicle_component_index_.find(vehicle_root);
    if (it == vehicle_component_index_.end()) {
        return;
    }

    float remaining_points = repair_order.repair_points;
    for (EntityID component_entity : it->second) {
        if (remaining_points <= 0.0f) {
            break;
        }

        if (std::find(repair_order.components_to_repair.begin(), repair_order.components_to_repair.end(), component_entity) == repair_order.components_to_repair.end()) {
            continue;
        }

        if (auto* hull = entity_manager_->get_component<HullComponent>(component_entity)) {
            float repair_amount = std::min(remaining_points * 0.002f, 1.0f - hull->structural_integrity);
            hull->structural_integrity += repair_amount;
            remaining_points -= repair_amount * 100.0f;
        }

        if (auto* engine = entity_manager_->get_component<EngineComponent>(component_entity)) {
            float repair_amount = std::min(remaining_points * 0.001f, 1.0f - engine->engine_health);
            engine->engine_health += repair_amount;
            remaining_points -= repair_amount * 100.0f;
            if (engine->engine_health > 0.5f) {
                engine->is_operational = true;
            }
        }

        if (auto* control = entity_manager_->get_component<ControlSystemsComponent>(component_entity)) {
            float repair_amount = std::min(remaining_points * 0.0015f, 1.0f - control->electronics_health);
            control->electronics_health += repair_amount;
            remaining_points -= repair_amount * 100.0f;
            if (control->electronics_health > 0.4f) {
                control->comms_operational = true;
            }
        }
    }
}

} // namespace assembly_system
