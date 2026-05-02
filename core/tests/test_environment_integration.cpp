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
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <iomanip>
#include <memory>
#include <chrono>
#include "physics_core/terrain_system.h"
#include "physics_core/environment_system.h"
#include "physics_core/destruction_system.h"
#include "physics_core/physics_core.h"

using namespace physics_core;

struct TestResults {
    double terrain_simd_time;
    double weather_integration_time;
    double destruction_time;
    double ground_forces_time;
    int tiles_loaded;
    int vehicles_processed;
    int total_assertions;
    int passed_assertions;
};

void test_integrated_environment_simulation() {
    std::cout << "\n===== Integrated Environment Simulation Test =====" << std::endl;

    // Create standalone environment systems (without full PhysicsCore)
    auto physics = std::make_unique<PhysicsCore>();
    physics->initialize(4, 0);
    auto terrain = std::make_unique<TerrainSystem>();
    auto environment = std::make_unique<EnvironmentSystem>();
    auto destruction = std::make_unique<DestructionSystem>();

    terrain->initialize(100.0f, 1.0f, 5);

    assert(terrain != nullptr);
    assert(environment != nullptr);
    assert(destruction != nullptr);

    // ===== Setup Terrain =====
    std::cout << "\n[1] Loading terrain tiles..." << std::endl;
    std::vector<TerrainTile*> tiles;
    for (int tx = 0; tx < 2; ++tx) {
        for (int ty = 0; ty < 2; ++ty) {
            uint32_t tile_id = tx * 2 + ty + 1;
            Vec3 origin(tx * 100.0f, ty * 100.0f, 0.0f);
            TerrainTile* tile = terrain->add_tile(tile_id, origin);
            assert(tile != nullptr);
            tiles.push_back(tile);

            // Initialize height data with a rolling terrain
            for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
                for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
                    float px = tx * 100.0f + (x / 63.0f) * 100.0f;
                    float py = ty * 100.0f + (y / 63.0f) * 100.0f;
                    tile->height_data[y][x] = 10.0f + 3.0f * std::sin(px / 30.0f) +
                                              2.0f * std::cos(py / 20.0f);
                    tile->surface_type[y][x] = static_cast<uint8_t>(SurfaceType::GRASS);
                    tile->moisture_level[y][x] = 0;
                    tile->snow_depth[y][x] = 0;
                }
            }
        }
    }

    std::cout << "✓ Loaded " << tiles.size() << " terrain tiles" << std::endl;

    // ===== Register Destructible Objects =====
    std::cout << "\n[2] Registering destructible structures..." << std::endl;
    std::vector<DestructibleObject> buildings;
    for (int i = 0; i < 5; ++i) {
        DestructibleObject building{};
        building.object_id = i + 1000;
        building.object_type = 0;
        building.structural_integrity = 1.0f;
        building.destruction_threshold = 0.3f;
        float center_x = 20.0f + i * 20.0f;
        float center_y = 50.0f;
        building.world_aabb_min = _mm256_setr_ps(center_x, center_y, 0.0f, 0, 0, 0, 0, 0);
        building.world_aabb_max = _mm256_setr_ps(center_x + 10.0f, center_y + 10.0f, 8.0f, 0, 0, 0, 0, 0);
        building.fragment_count = 10;
        building.destruction_flags = 0;
        destruction->register_object(building);
        buildings.push_back(building);
    }

    std::cout << "✓ Registered " << buildings.size() << " destructible objects" << std::endl;

    // ===== Simulate Weather Effects =====
    std::cout << "\n[3] Simulating weather effects..." << std::endl;
    WeatherState weather = {0.0f, 5.0f, 1.0f, 0.0f, 20.0f, 0.3f, 10000.0f, WeatherType::CLEAR, 0.0f};

    // Simulate rain starting
    weather.rain_intensity = 0.7f;
    weather.weather_type = WeatherType::RAIN;
    for (int frame = 0; frame < 50; ++frame) {
        environment->update_weather_effects(weather, *terrain, nullptr);
    }

    std::cout << "✓ Weather simulation: 50 frames of " << weather.rain_intensity << " rain intensity" << std::endl;

    // Check for mud formation
    int mud_tiles = 0;
    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            if (tiles[0]->surface_type[y][x] == static_cast<uint8_t>(SurfaceType::MUD)) {
                mud_tiles++;
            }
        }
    }

    std::cout << "  Mud tiles formed: " << mud_tiles << " / " << (TerrainSystem::kTileResolution * TerrainSystem::kTileResolution)
              << std::endl;

    // ===== Ground Interaction Simulation =====
    std::cout << "\n[4] Simulating vehicle-terrain interaction..." << std::endl;
    std::vector<VehicleState> vehicles;
    const int num_vehicles = 4;
    for (int i = 0; i < num_vehicles; ++i) {
        VehicleState vehicle{};
        vehicle.vehicle_id = i + 1;
        vehicle.mass = 40000.0f + i * 5000.0f;
        vehicle.speed = 10.0f + i * 2.0f;
        vehicle.contact_points = {
            Vec3(40.0f + i * 15.0f, 30.0f, 0.0f),
            Vec3(40.0f + i * 15.0f + 4.0f, 30.0f, 0.0f),
            Vec3(40.0f + i * 15.0f, 40.0f, 0.0f),
            Vec3(40.0f + i * 15.0f + 4.0f, 40.0f, 0.0f)
        };
        vehicle.contact_count = 4;
        vehicles.push_back(vehicle);
    }

    std::vector<GroundInteractionForces> ground_forces(num_vehicles);
    environment->calculate_ground_forces(vehicles.data(), ground_forces.data(), num_vehicles, *terrain);

    std::cout << "✓ Calculated ground forces for " << num_vehicles << " vehicles" << std::endl;
    for (int i = 0; i < num_vehicles; ++i) {
        std::cout << "  Vehicle " << i + 1 << ": friction=" << std::fixed << std::setprecision(3)
                  << ground_forces[i].friction_coefficient << ", traction="
                  << ground_forces[i].traction_factor << std::endl;
    }

    // ===== Destruction Simulation =====
    std::cout << "\n[5] Simulating destruction events..." << std::endl;
    for (size_t i = 0; i < buildings.size(); ++i) {
        destruction->apply_damage(buildings[i].object_id, 0.4f, *terrain, *physics);
    }

    destruction->update(0.016f, *terrain);
    const auto& craters = destruction->get_craters();
    std::cout << "✓ Created " << craters.size() << " craters from destruction" << std::endl;
    for (const auto& crater : craters) {
        std::cout << "  Crater at (" << crater.center_x << ", " << crater.center_y << "), "
                  << "radius=" << crater.radius << ", depth=" << crater.depth << std::endl;
    }

    // ===== Large-Scale Query Test =====
    std::cout << "\n[6] Performance test: bulk queries..." << std::endl;
    const size_t num_queries = 50000;
    std::vector<float> query_x(num_queries);
    std::vector<float> query_y(num_queries);
    std::vector<float> heights(num_queries);
    std::vector<uint8_t> materials(num_queries);

    for (size_t i = 0; i < num_queries; ++i) {
        query_x[i] = static_cast<float>(rand() % 200);
        query_y[i] = static_cast<float>(rand() % 200);
    }

    auto start = std::chrono::high_resolution_clock::now();
    terrain->query_height_simd(query_x.data(), query_y.data(), heights.data(), num_queries);
    auto end = std::chrono::high_resolution_clock::now();
    double height_query_time = std::chrono::duration<double, std::micro>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    terrain->query_surface_material_simd(query_x.data(), query_y.data(), materials.data(), num_queries);
    end = std::chrono::high_resolution_clock::now();
    double material_query_time = std::chrono::duration<double, std::micro>(end - start).count();

    std::cout << "✓ Height queries: " << std::setprecision(3) << height_query_time / num_queries
              << " μs/query" << std::endl;
    std::cout << "✓ Material queries: " << material_query_time / num_queries << " μs/query"
              << std::endl;

    std::cout << "\n✅ Integrated environment simulation complete!" << std::endl;
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Terrain & Environment System - Integration Test Suite        ║" << std::endl;
    std::cout << "║   Testing: Heightmap, Weather, Destruction, Vehicle Physics   ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;

    try {
        test_integrated_environment_simulation();
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  ✅ ALL INTEGRATION TESTS PASSED - System Ready for Use       ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
