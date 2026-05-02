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

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "assembly_system/vehicle_assembly.h"
#include "ecs/entity_manager.h"
#include "physics_core/damage_system.h"
#include "physics_core/physics_core.h"
#include "procedural_armor_factory/parametric_template.h"

namespace rendering_engine {
    class RenderingEngine;
    class ModelSystem;
}

namespace historical_vehicle_system {

enum class DataValidationLevel : uint32_t {
    NONE = 0,
    BASIC = 1,
    FULL = 2
};

enum class EngineType : uint32_t {
    DIESEL = 0,
    PETROL = 1,
    TURBINE = 2
};

struct CampaignSpecs {
    uint32_t start_year = 0;
    uint32_t end_year = 0;
    uint32_t nation = UINT32_MAX;
    uint32_t era = UINT32_MAX;
    uint32_t vehicle_class = UINT32_MAX;
};

struct alignas(32) HistoricalVehicleSpecs {
    uint32_t vehicle_id = 0;
    char vehicle_name[64] = {};
    uint32_t nation = 0;
    uint32_t era = 0;
    uint32_t vehicle_class = 0;

    float length_m = 0.0f;
    float width_m = 0.0f;
    float height_m = 0.0f;
    float weight_tons = 0.0f;
    float ground_pressure_kpa = 0.0f;

    struct ArmorSpecs {
        float hull_front = 0.0f;
        float hull_side = 0.0f;
        float hull_rear = 0.0f;
        float hull_top = 0.0f;
        float turret_front = 0.0f;
        float turret_side = 0.0f;
        float turret_rear = 0.0f;
        float turret_top = 0.0f;
    } armor;

    struct EngineSpecs {
        float engine_power_hp = 0.0f;
        float max_speed_kmh = 0.0f;
        float reverse_speed_kmh = 0.0f;
        float fuel_capacity_liters = 0.0f;
        float operational_range_km = 0.0f;
        uint32_t engine_type = 0;
    } engine;

    struct WeaponSpecs {
        uint32_t main_gun_caliber_mm = 0;
        uint32_t secondary_weapons_count = 0;
        float rate_of_fire_rpm = 0.0f;
        uint32_t ammunition_capacity = 0;
        float max_elevation_deg = 0.0f;
        float max_depression_deg = 0.0f;
    } weapons;

    uint32_t crew_size = 0;
    float reload_time_seconds = 0.0f;
    bool has_radio = false;
    bool has_night_vision = false;

    uint32_t data_confidence = 0;
    uint32_t source_references = 0;
};

struct alignas(32) PhysicsMappingParameters {
    float mass_distribution_x = 0.0f;
    float mass_distribution_y = 0.0f;
    float mass_distribution_z = 0.0f;

    float inertia_scale_xx = 1.0f;
    float inertia_scale_yy = 1.0f;
    float inertia_scale_zz = 1.0f;

    float suspension_stiffness = 0.0f;
    float suspension_damping = 0.0f;
    float track_friction_coeff = 0.0f;

    float drag_coefficient = 0.0f;
    float lift_coefficient = 0.0f;

    float recoil_impulse_scale = 0.0f;
    float barrel_elevation_speed = 0.0f;

    float mobility_multiplier = 1.0f;
    float armor_multiplier = 1.0f;
    float weapon_accuracy_multiplier = 1.0f;
};

struct BalanceProfile {
    float realism_level = 1.0f;
    float team_balance_factor = 1.0f;
    float era_balance_factor = 1.0f;
    float map_size_factor = 1.0f;
};

class HistoricalDatabase {
public:
    HistoricalDatabase() = default;

    bool load_vehicle_database(const std::string& database_path, DataValidationLevel validation_level);
    const HistoricalVehicleSpecs* get_vehicle_by_id(uint32_t vehicle_id) const;
    bool filter_vehicles_by_era(uint32_t era_filter, std::vector<uint32_t>& output_vehicle_ids) const;
    bool get_nation_variants(uint32_t base_vehicle_id, uint32_t nation, std::vector<HistoricalVehicleSpecs>& variants) const;
    bool get_campaign_vehicles(const CampaignSpecs& campaign, std::vector<uint32_t>& available_vehicles) const;
    const std::vector<HistoricalVehicleSpecs>& vehicles() const noexcept { return vehicles_; }
    const std::string& last_error() const noexcept { return last_error_message_; }

private:
    bool parse_csv(std::istream& stream, DataValidationLevel validation_level);
    bool parse_json(std::istream& stream, DataValidationLevel validation_level);
    bool validate_vehicle_entry(const HistoricalVehicleSpecs& specs, DataValidationLevel validation_level, std::string& out_message) const;
    void build_soa(const HistoricalVehicleSpecs& specs);
    static std::string normalize_name(std::string_view name);
    static std::vector<std::string> split_csv_line(const std::string& line);

    std::vector<HistoricalVehicleSpecs> vehicles_;
    std::unordered_map<uint32_t, size_t> vehicle_index_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> era_index_;
    std::vector<uint32_t> vehicle_ids_;
    std::vector<float> vehicle_lengths_;
    std::vector<float> vehicle_widths_;
    std::vector<float> vehicle_heights_;
    std::vector<float> vehicle_weights_;
    std::vector<float> ground_pressures_;
    std::string last_error_message_;
};

class PhysicsMappingSystem {
public:
    bool generate_physics_parameters(
        const HistoricalVehicleSpecs& historical_specs,
        const BalanceProfile& balance_profile,
        PhysicsMappingParameters& output_params
    );

private:
    static uint64_t compute_cache_key(uint32_t vehicle_id, const BalanceProfile& profile) noexcept;
    static uint32_t pack_float(float value) noexcept;
    std::unordered_map<uint64_t, PhysicsMappingParameters> cache_;
};

class VehicleCalibrationSystem {
public:
    void set_active_profiles(const std::vector<BalanceProfile>& profiles);
    void auto_calibrate_for_balance(const HistoricalVehicleSpecs& specs, PhysicsMappingParameters& output);
    void apply_manual_calibration(uint32_t vehicle_id, const PhysicsMappingParameters& overrides);

private:
    std::vector<BalanceProfile> active_profiles_;
    std::unordered_map<uint32_t, PhysicsMappingParameters> calibration_db_;
    PhysicsMappingSystem mapping_system_;
};

class HistoricalVehicleSystem {
public:
    explicit HistoricalVehicleSystem(ecs::EntityManager* entity_manager = nullptr);

    void set_entity_manager(ecs::EntityManager* entity_manager);
    void set_database(const HistoricalDatabase& database);
    bool load_vehicle_database(const std::string& database_path, DataValidationLevel validation_level);

    bool create_vehicle_from_historical_id(
        uint32_t vehicle_id,
        const BalanceProfile& balance_profile,
        physics_core::EntityID& output_entity
    );

    bool convert_to_modular_blueprint(
        const HistoricalVehicleSpecs& specs,
        assembly_system::VehicleBlueprint& output_blueprint
    ) const;

    bool apply_physics_mapping(
        physics_core::EntityID vehicle_entity,
        const PhysicsMappingParameters& params,
        physics_core::PhysicsCore& physics_core
    ) const;

    bool assign_historical_appearance(
        physics_core::EntityID vehicle_entity,
        const HistoricalVehicleSpecs& specs,
        rendering_engine::RenderingEngine& renderer
    ) const;

    /**
     * Create a procedurally generated parametric vehicle (MVP: no manual models)
     * @param vehicle_id Historical vehicle ID to convert to parametric template
     * @param balance_profile Balance profile for physics
     * @param output_entity Output entity ID
     * @return true if successful
     */
    bool create_parametric_vehicle(
        uint32_t vehicle_id,
        const BalanceProfile& balance_profile,
        physics_core::EntityID& output_entity
    );

    /**
     * Create parametric vehicle from explicit template (advanced)
     * @param tmpl Parametric template
     * @param balance_profile Balance profile
     * @param physics_core Physics core for body creation
     * @param output_entity Output entity ID
     * @return true if successful
     */
    bool create_parametric_vehicle_from_template(
        const class procedural_armor_factory::ParametricTankTemplate& tmpl,
        const BalanceProfile& balance_profile,
        physics_core::PhysicsCore& physics_core,
        physics_core::EntityID& output_entity
    );

    void set_model_system(rendering_engine::ModelSystem* model_system);
    void set_physics_core(physics_core::PhysicsCore* physics_core);

private:
    ecs::EntityManager* entity_manager_ = nullptr;
    rendering_engine::ModelSystem* model_system_ = nullptr;
    physics_core::PhysicsCore* physics_core_ = nullptr;
    assembly_system::ModularVehicleSystem modular_vehicle_system_;
    HistoricalDatabase database_;
    PhysicsMappingSystem physics_mapper_;
    VehicleCalibrationSystem calibration_system_;
    std::unordered_map<physics_core::EntityID, physics_core::VehicleDamageState> vehicle_damage_registry_;
};

} // namespace historical_vehicle_system
