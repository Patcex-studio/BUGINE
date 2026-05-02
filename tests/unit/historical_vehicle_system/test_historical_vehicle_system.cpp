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
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <fstream>

#include "historical_vehicle_system/historical_vehicle_system.h"
#include "procedural_armor_factory/armor_generator.h"
#include "procedural_armor_factory/parametric_template.h"
#include "ecs/entity_manager.h"
#include "assembly_system/vehicle_assembly.h"
#include "physics_core/physics_core.h"

using namespace historical_vehicle_system;

static const char kSampleCsv[] =
"vehicle_id,vehicle_name,nation,era,vehicle_class,length_m,width_m,height_m,weight_tons,ground_pressure_kpa,"
"hull_front,hull_side,hull_rear,hull_top,turret_front,turret_side,turret_rear,turret_top,"
"engine_power_hp,max_speed_kmh,reverse_speed_kmh,fuel_capacity_liters,operational_range_km,engine_type,"
"main_gun_caliber_mm,secondary_weapons_count,rate_of_fire_rpm,ammunition_capacity,max_elevation_deg,max_depression_deg,"
"crew_size,reload_time_seconds,has_radio,has_night_vision,data_confidence,source_references\n"
"101,T-34/85,0,0,0,6.68,3.00,2.45,28.0,1.05,45,40,40,20,90,75,60,30,500,55,6,350,250,0,85,2,6,60,30,5,5,7.5,1,0,2,10\n"
"102,M4 Sherman,2,0,0,6.27,2.62,2.74,30.3,1.12,51,38,38,25,76,59,38,30,450,48,6,340,240,0,75,2,6,60,30,5,5,7.8,1,0,2,8\n";

TEST_CASE("HistoricalDatabase loads and filters sample vehicles", "[historical_vehicle_system]") {
    const std::string temp_path = "test_historical_database.csv";
    std::ofstream file(temp_path, std::ios::binary);
    REQUIRE(file.is_open());
    file << kSampleCsv;
    file.close();

    HistoricalDatabase database;
    REQUIRE(database.load_vehicle_database(temp_path, DataValidationLevel::FULL));
    const HistoricalVehicleSpecs* vehicle = database.get_vehicle_by_id(101);
    REQUIRE(vehicle != nullptr);
    REQUIRE(std::string(vehicle->vehicle_name) == "T-34/85");

    std::vector<uint32_t> era_ids;
    REQUIRE(database.filter_vehicles_by_era(0, era_ids));
    REQUIRE(era_ids.size() == 2);
}

TEST_CASE("PhysicsMappingSystem generates plausible physical parameters", "[historical_vehicle_system]") {
    HistoricalVehicleSpecs sample = {};
    sample.vehicle_id = 101;
    std::strncpy(sample.vehicle_name, "T-34/85", sizeof(sample.vehicle_name) - 1);
    sample.length_m = 6.68f;
    sample.width_m = 3.00f;
    sample.height_m = 2.45f;
    sample.weight_tons = 28.0f;
    sample.ground_pressure_kpa = 1.05f;
    sample.weapons.rate_of_fire_rpm = 6.0f;

    PhysicsMappingSystem mapper;
    PhysicsMappingParameters params;
    BalanceProfile profile;
    profile.realism_level = 0.8f;
    profile.team_balance_factor = 0.9f;
    REQUIRE(mapper.generate_physics_parameters(sample, profile, params));
    REQUIRE(params.track_friction_coeff > 0.0f);
    REQUIRE(params.inertia_scale_xx >= 0.6f);
}

TEST_CASE("ProceduralArmorGenerator is deterministic for same seed and varies with different seed", "[procedural_armor_factory]") {
    procedural_armor_factory::ParametricTankTemplate tmpl{};
    tmpl.hull_length = 6.5f;
    tmpl.hull_width = 3.0f;
    tmpl.hull_height = 2.4f;
    tmpl.hull_front_angle = 30.0f;
    tmpl.road_wheels_count = 6;
    tmpl.wheel_radius = 0.35f;
    tmpl.track_width = 0.35f;
    tmpl.turret_ring_diameter = 2.0f;
    tmpl.turret_height = 1.1f;
    tmpl.gun_caliber = 85.0f;
    tmpl.gun_length = 48.0f;
    tmpl.era_flags = 1;
    tmpl.weight_tons = 28.0f;

    procedural_armor_factory::ProceduralArmorGenerator generator;
    tmpl.seed = 12345u;
    auto first = generator.generate(tmpl);
    auto second = generator.generate(tmpl);

    REQUIRE(first.visual_vertices.size() == second.visual_vertices.size());
    REQUIRE(first.visual_indices.size() == second.visual_indices.size());
    REQUIRE(std::memcmp(
        first.visual_vertices.data(),
        second.visual_vertices.data(),
        first.visual_vertices.size() * sizeof(rendering_engine::Vertex)) == 0);
    REQUIRE(std::memcmp(
        first.visual_indices.data(),
        second.visual_indices.data(),
        first.visual_indices.size() * sizeof(uint32_t)) == 0);

    tmpl.seed = 54321u;
    auto third = generator.generate(tmpl);
    bool generated_variation = (third.visual_vertices.size() != first.visual_vertices.size()) ||
                                (third.visual_indices.size() != first.visual_indices.size());
    REQUIRE(generated_variation);
}

TEST_CASE("HistoricalVehicleSystem can convert specs to blueprint and instantiate entity", "[historical_vehicle_system]") {
    const std::string temp_path = "test_historical_system.csv";
    std::ofstream file(temp_path, std::ios::binary);
    REQUIRE(file.is_open());
    file << kSampleCsv;
    file.close();

    HistoricalVehicleSystem system;
    REQUIRE(system.load_vehicle_database(temp_path, DataValidationLevel::FULL));

    ecs::EntityManager manager;
    system.set_entity_manager(&manager);

    physics_core::EntityID entity = 0;
    BalanceProfile profile;
    profile.realism_level = 0.7f;
    profile.team_balance_factor = 0.9f;

    REQUIRE(system.create_vehicle_from_historical_id(101, profile, entity));
    REQUIRE(entity != 0);
}
