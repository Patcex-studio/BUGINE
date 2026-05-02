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

#include "assembly_system/universal_assembly_engine.h"
#include "ecs/entity_manager.h"

TEST_CASE("UniversalAssemblyEngine can construct a vehicle from an assembly blueprint", "[assembly_system]") {
    ecs::EntityManager manager;
    assembly_system::UniversalAssemblyEngine engine(&manager);

    assembly_system::AssemblyComponentDefinition hull_def;
    hull_def.component_id = 1;
    std::strncpy(hull_def.component_name, "HullComponent", sizeof(hull_def.component_name) - 1);
    hull_def.component_category = static_cast<uint32_t>(assembly_system::ComponentCategory::HULL);
    hull_def.performance.armor_bonus = 0.8f;
    hull_def.performance.reliability_rating = 0.95f;
    hull_def.attachment_point_count = 2;
    hull_def.attachment_points[0].socket_type = static_cast<uint32_t>(assembly_system::SocketType::ENGINE_MOUNT);
    hull_def.attachment_points[0].compatible_types_mask = 1u << static_cast<uint32_t>(assembly_system::SocketType::ENGINE_MOUNT);
    hull_def.attachment_points[1].socket_type = static_cast<uint32_t>(assembly_system::SocketType::WEAPON_MOUNT);
    hull_def.attachment_points[1].compatible_types_mask = 1u << static_cast<uint32_t>(assembly_system::SocketType::WEAPON_MOUNT);
    engine.component_library().register_definition(hull_def);

    assembly_system::AssemblyComponentDefinition engine_def;
    engine_def.component_id = 2;
    std::strncpy(engine_def.component_name, "EngineComponent", sizeof(engine_def.component_name) - 1);
    engine_def.component_category = static_cast<uint32_t>(assembly_system::ComponentCategory::ENGINE);
    engine_def.performance.mobility_bonus = 0.9f;
    engine_def.performance.reliability_rating = 0.92f;
    engine_def.resources.fuel_consumption_lph = 12.0f;
    engine.component_library().register_definition(engine_def);

    assembly_system::AssemblyComponentDefinition weapon_def;
    weapon_def.component_id = 3;
    std::strncpy(weapon_def.component_name, "WeaponComponent", sizeof(weapon_def.component_name) - 1);
    weapon_def.component_category = static_cast<uint32_t>(assembly_system::ComponentCategory::WEAPON);
    weapon_def.performance.weapon_accuracy = 0.72f;
    engine.component_library().register_definition(weapon_def);

    assembly_system::AssemblyComponentDefinition control_def;
    control_def.component_id = 4;
    std::strncpy(control_def.component_name, "ControlComponent", sizeof(control_def.component_name) - 1);
    control_def.component_category = static_cast<uint32_t>(assembly_system::ComponentCategory::CONTROL);
    control_def.performance.mobility_bonus = 0.8f;
    control_def.performance.reliability_rating = 0.9f;
    engine.component_library().register_definition(control_def);

    assembly_system::AssemblyBlueprint blueprint;
    blueprint.name = "test_vehicle";
    blueprint.components = {
        {1, 1, static_cast<uint32_t>(assembly_system::ComponentCategory::HULL), 0, 8000.0f, 0.95f, 0},
        {2, 2, static_cast<uint32_t>(assembly_system::ComponentCategory::ENGINE), 0, 2000.0f, 0.92f, 1},
        {3, 3, static_cast<uint32_t>(assembly_system::ComponentCategory::WEAPON), 0, 1200.0f, 0.85f, 1},
        {4, 4, static_cast<uint32_t>(assembly_system::ComponentCategory::CONTROL), 0, 500.0f, 0.9f, 1}
    };

    assembly_system::AssemblyConstructionOptions options;
    options.require_historical_accuracy = false;

    assembly_system::AssemblyValidationResult validation_result;
    assembly_system::EntityID output_entity = 0;

    REQUIRE(engine.construct_vehicle(blueprint, options, output_entity, validation_result));
    REQUIRE(output_entity != 0);
    REQUIRE(validation_result.is_valid);
    REQUIRE(validation_result.stability_score >= 0.0f);
    REQUIRE(validation_result.performance_score >= 0.0f);
}
