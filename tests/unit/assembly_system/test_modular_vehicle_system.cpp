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
#include <catch2/catch_approx.hpp>

#include "assembly_system/vehicle_assembly.h"
#include "ecs/entity_manager.h"
#include "physics_core/physics_core.h"

TEST_CASE("ModularVehicleSystem can assemble a template vehicle and process damage", "[assembly_system]") {
    ecs::EntityManager manager;
    assembly_system::ModularVehicleSystem system(&manager);

    assembly_system::VehicleBlueprint blueprint = assembly_system::VehicleBlueprint::create_default();
    assembly_system::EntityID root_entity = 0;
    std::string error_message;

    REQUIRE(system.assemble_vehicle_from_blueprint(blueprint, root_entity, error_message));
    REQUIRE(root_entity != 0);

    auto* hull = manager.get_component<assembly_system::HullComponent>(root_entity);
    REQUIRE(hull != nullptr);
    REQUIRE(hull->structural_integrity == Catch::Approx(0.95f));

    assembly_system::ComponentEvent power_event;
    power_event.source_entity = root_entity;
    power_event.target_entity = root_entity;
    power_event.event_type = assembly_system::ComponentEventType::POWER_REQUEST;
    power_event.intensity = 0.5f;
    power_event.timestamp = 0.016f;

    REQUIRE(system.enqueue_component_event(power_event));
    system.process_component_events(0.016f);

    assembly_system::DamageEvent hit;
    hit.hit_component_id = root_entity;
    hit.hit_strength = 150.0f;
    hit.hit_point = physics_core::Vec3(0.0, 0.0, 0.0);
    hit.penetration = 95.0f;

    std::vector<assembly_system::DamageResult> results;
    REQUIRE(system.receive_ballistic_damage(hit, root_entity, results));
    REQUIRE(!results.empty());
    REQUIRE(results.front().applied_damage > 0.0f);
}
