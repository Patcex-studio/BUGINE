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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <immintrin.h>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "ecs/entity_manager.h"
#include "physics_core/physics_core.h"
#include "physics_core/types.h"

namespace assembly_system {

using EntityID = physics_core::EntityID;
using Vec3 = physics_core::Vec3;

enum class SocketType : uint32_t {
    HULL_MOUNT = 0,
    ENGINE_MOUNT = 1,
    WEAPON_MOUNT = 2,
    CONTROL_MOUNT = 3,
    SENSOR_MOUNT = 4,
    ANY = UINT32_MAX
};

enum class ComponentEventType : uint32_t {
    POWER_REQUEST = 0,
    FUEL_FLOW = 1,
    DAMAGE_RECEIVED = 2,
    SYSTEM_FAILURE = 3,
    CONTROL_INPUT = 4,
    SENSOR_DATA = 5
};

enum class ConnectionType : uint32_t {
    MECHANICAL = 0,
    ELECTRICAL = 1,
    HYDRAULIC = 2,
    FUEL = 3
};

enum DamagePropagationFlags : uint32_t {
    DAMAGE_PROP_NONE = 0,
    DAMAGE_PROP_MECHANICAL = 1 << 0,
    DAMAGE_PROP_ELECTRICAL = 1 << 1,
    DAMAGE_PROP_HYDRAULIC = 1 << 2,
    DAMAGE_PROP_FUEL = 1 << 3,
    DAMAGE_PROP_CRITICAL = 1 << 4
};

enum class EngineType : uint32_t {
    DIESEL = 0,
    GASOLINE = 1,
    GAS_TURBINE = 2
};

struct alignas(32) BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct alignas(32) SocketPoint {
    __m256 local_position;
    __m256 local_rotation;
    uint32_t socket_type;
    uint32_t compatible_types_mask;
    EntityID connected_entity;
    float connection_strength;
};

struct FuelLine {
    EntityID source_tank = 0;
    EntityID target_engine = 0;
    float max_flow_lpm = 0.0f;
    float current_flow_lpm = 0.0f;
    bool is_blocked = false;
};

struct WeaponMount {
    EntityID mount_entity = 0;
    SocketPoint mount_socket;
    float recoil_force = 0.0f;
    bool is_operational = true;
};

struct AmmoType {
    uint32_t ammo_id = 0;
    float penetration = 0.0f;
    float explosive_mass = 0.0f;
    float mass_kg = 0.0f;
};

struct SensorSystem {
    uint32_t sensor_id = 0;
    uint32_t type = 0;
    float range_m = 0.0f;
    float health = 1.0f;
};

struct ControlLink {
    EntityID source = 0;
    EntityID target = 0;
    uint32_t type = 0;
    float responsiveness = 1.0f;
};

struct HullComponent {
    EntityID entity_id = 0;
    float armor_thickness_front = 0.0f;
    float armor_thickness_side = 0.0f;
    float armor_thickness_rear = 0.0f;
    float armor_thickness_top = 0.0f;
    uint32_t armor_material_type = 0;
    float structural_integrity = 1.0f;
    BoundingBox local_bounds;
    std::array<SocketPoint, 8> attachment_sockets{};
    uint32_t socket_count = 0;
};

struct EngineComponent {
    EntityID entity_id = 0;
    float max_power_hp = 0.0f;
    float current_rpm = 0.0f;
    float fuel_capacity_liters = 0.0f;
    float fuel_current_liters = 0.0f;
    float engine_health = 1.0f;
    bool is_operational = true;
    float temperature_celsius = 20.0f;
    uint32_t engine_type = static_cast<uint32_t>(EngineType::DIESEL);
    std::array<FuelLine, 4> fuel_lines{};
    uint32_t fuel_line_count = 0;
};

struct WeaponsComponent {
    EntityID entity_id = 0;
    std::array<WeaponMount, 6> weapon_mounts{};
    uint32_t weapon_mount_count = 0;
    uint32_t active_weapon_index = 0;
    float ammunition_total = 0.0f;
    float ammunition_current = 0.0f;
    float reload_time_seconds = 0.0f;
    bool is_jammed = false;
    float barrel_wear = 0.0f;
    std::array<AmmoType, 4> ammo_types{};
    uint32_t ammo_type_count = 0;
};

struct ControlSystemsComponent {
    EntityID entity_id = 0;
    float steering_response = 1.0f;
    float suspension_health = 1.0f;
    bool has_stabilization = false;
    float electronics_health = 1.0f;
    bool comms_operational = true;
    std::array<SensorSystem, 8> sensors{};
    uint32_t sensor_count = 0;
    std::array<ControlLink, 8> control_links{};
    uint32_t control_link_count = 0;
};

struct ComponentEvent {
    EntityID source_entity = 0;
    EntityID target_entity = 0;
    ComponentEventType event_type = ComponentEventType::POWER_REQUEST;
    float intensity = 0.0f;
    float timestamp = 0.0f;
    uint32_t flags = 0;
};

class LockFreeComponentEventQueue {
public:
    static constexpr size_t kCapacity = 1024;

    bool push(const ComponentEvent& event) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(current_tail);
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[current_tail] = event;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(ComponentEvent& output) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        output = buffer_[current_head];
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr size_t increment(size_t value) noexcept {
        return (value + 1) % kCapacity;
    }

    std::array<ComponentEvent, kCapacity> buffer_{};
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

struct DamageConnection {
    EntityID target_component = 0;
    float propagation_factor = 0.0f;
    uint32_t connection_type = static_cast<uint32_t>(ConnectionType::MECHANICAL);
    bool is_bidirectional = true;
};

struct DamagePropagationNode {
    EntityID component_entity = 0;
    std::vector<DamageConnection> connections;
    float damage_multiplier = 1.0f;
    float critical_threshold = 0.5f;
    uint32_t flags = DAMAGE_PROP_NONE;
};

struct DamageEvent {
    EntityID hit_component_id = 0;
    float hit_strength = 0.0f;
    Vec3 hit_point{};
    float penetration = 0.0f;
};

struct DamageResult {
    EntityID component_entity = 0;
    float applied_damage = 0.0f;
    bool critical_hit = false;
};

struct RepairOrder {
    std::vector<EntityID> components_to_repair;
    float repair_points = 0.0f;
    bool has_field_kit = false;
};

struct VehicleBlueprint {
    std::string name;
    HullComponent hull;
    EngineComponent engine;
    WeaponsComponent weapons;
    ControlSystemsComponent control;

    static VehicleBlueprint create_default() {
        VehicleBlueprint blueprint;
        blueprint.name = "light_modular_vehicle";
        blueprint.hull.armor_thickness_front = 80.0f;
        blueprint.hull.armor_thickness_side = 60.0f;
        blueprint.hull.armor_thickness_rear = 40.0f;
        blueprint.hull.armor_thickness_top = 30.0f;
        blueprint.hull.armor_material_type = 1;
        blueprint.hull.structural_integrity = 0.95f;
        blueprint.hull.local_bounds.min = Vec3(-1.0, -0.5, -0.3);
        blueprint.hull.local_bounds.max = Vec3(1.0, 0.5, 0.8);
        blueprint.hull.socket_count = 2;
        blueprint.hull.attachment_sockets[0].socket_type = static_cast<uint32_t>(SocketType::ENGINE_MOUNT);
        blueprint.hull.attachment_sockets[0].compatible_types_mask = 1u << static_cast<uint32_t>(SocketType::ENGINE_MOUNT);
        blueprint.hull.attachment_sockets[0].connection_strength = 1.0f;
        blueprint.hull.attachment_sockets[1].socket_type = static_cast<uint32_t>(SocketType::WEAPON_MOUNT);
        blueprint.hull.attachment_sockets[1].compatible_types_mask = 1u << static_cast<uint32_t>(SocketType::WEAPON_MOUNT);
        blueprint.hull.attachment_sockets[1].connection_strength = 1.0f;

        blueprint.engine.max_power_hp = 540.0f;
        blueprint.engine.current_rpm = 650.0f;
        blueprint.engine.fuel_capacity_liters = 350.0f;
        blueprint.engine.fuel_current_liters = 280.0f;
        blueprint.engine.engine_health = 1.0f;
        blueprint.engine.engine_type = static_cast<uint32_t>(EngineType::DIESEL);
        blueprint.engine.fuel_line_count = 1;
        blueprint.engine.fuel_lines[0].max_flow_lpm = 120.0f;
        blueprint.engine.fuel_lines[0].current_flow_lpm = 90.0f;

        blueprint.weapons.weapon_mount_count = 1;
        blueprint.weapons.active_weapon_index = 0;
        blueprint.weapons.ammunition_total = 120.0f;
        blueprint.weapons.ammunition_current = 120.0f;
        blueprint.weapons.reload_time_seconds = 4.2f;
        blueprint.weapons.ammo_type_count = 1;
        blueprint.weapons.ammo_types[0].ammo_id = 1;
        blueprint.weapons.ammo_types[0].penetration = 650.0f;
        blueprint.weapons.ammo_types[0].explosive_mass = 2.1f;
        blueprint.weapons.ammo_types[0].mass_kg = 20.0f;

        blueprint.control.steering_response = 0.85f;
        blueprint.control.suspension_health = 0.9f;
        blueprint.control.has_stabilization = true;
        blueprint.control.electronics_health = 1.0f;
        blueprint.control.comms_operational = true;
        blueprint.control.sensor_count = 1;
        blueprint.control.sensors[0].sensor_id = 101;
        blueprint.control.sensors[0].type = 0;
        blueprint.control.sensors[0].range_m = 1500.0f;
        blueprint.control.sensors[0].health = 1.0f;
        blueprint.control.control_link_count = 1;
        blueprint.control.control_links[0].type = static_cast<uint32_t>(ConnectionType::ELECTRICAL);
        blueprint.control.control_links[0].responsiveness = 0.9f;
        return blueprint;
    }
};

class DamagePropagationSystem {
public:
    bool add_node(DamagePropagationNode node);
    bool add_connection(EntityID source_component, DamageConnection connection);
    bool propagate_damage_from_hit(const DamageEvent& initial_hit, std::vector<DamageResult>& results);
    void clear();

private:
    std::unordered_map<EntityID, DamagePropagationNode> propagation_graph_;
    mutable std::mutex graph_mutex_;
};

class ModularVehicleSystem {
public:
    explicit ModularVehicleSystem(ecs::EntityManager* entity_manager = nullptr);

    void set_entity_manager(ecs::EntityManager* entity_manager);

    bool assemble_vehicle_from_blueprint(
        const VehicleBlueprint& blueprint,
        EntityID& root_entity,
        std::string& error_message
    );

    void process_component_events(float delta_time);
    bool enqueue_component_event(const ComponentEvent& event);

    bool receive_ballistic_damage(
        const DamageEvent& hit,
        EntityID vehicle_root,
        std::vector<DamageResult>& results
    );

    void sync_to_physics_core(EntityID vehicle_root, physics_core::PhysicsCore& physics);
    bool create_modern_mb_tank(const VehicleBlueprint& specs, EntityID& tank_entity, std::string& error_message);
    void process_field_repairs(EntityID vehicle_root, const RepairOrder& repair_order);

private:
    bool validate_blueprint(const VehicleBlueprint& blueprint, std::string& error_message) const;
    void build_damage_graph(EntityID root_entity, const std::vector<EntityID>& component_entities);
    void process_event(const ComponentEvent& event);
    float apply_damage_to_component(EntityID component_entity, float damage);
    uint32_t socket_type_mask(SocketType type) const noexcept;
    bool is_socket_compatible(const SocketPoint& socket, SocketType type) const noexcept;

    ecs::EntityManager* entity_manager_ = nullptr;
    LockFreeComponentEventQueue event_queue_;
    DamagePropagationSystem damage_system_;
    std::mutex assembly_mutex_;
    std::unordered_map<EntityID, std::vector<EntityID>> vehicle_component_index_;
};

} // namespace assembly_system
