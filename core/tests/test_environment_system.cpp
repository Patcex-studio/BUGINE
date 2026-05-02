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
#include "physics_core/physics_core.h"
#include "physics_core/terrain_system.h"
#include "physics_core/environment_system.h"
#include "physics_core/destruction_system.h"

using namespace physics_core;

void test_terrain_simd_query() {
    std::cout << "Testing terrain SIMD queries..." << std::endl;

    TerrainSystem terrain;
    terrain.initialize(100.0f, 1.0f, 5);
    TerrainTile* tile = terrain.add_tile(1, Vec3(0.0, 0.0, 0.0));
    assert(tile != nullptr);

    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            tile->height_data[y][x] = static_cast<float>(x + y);
            tile->surface_type[y][x] = static_cast<uint8_t>(SurfaceType::GRASS);
        }
    }

    float positions_x[8] = {10.0f, 20.0f, 30.0f, 40.0f, 55.0f, 99.0f, 0.5f, 64.0f};
    float positions_y[8] = {10.0f, 20.0f, 30.0f, 40.0f, 55.0f, 99.0f, 0.5f, 64.0f};
    float heights[8];
    uint8_t materials[8];

    terrain.query_height_simd(positions_x, positions_y, heights, 8);
    terrain.query_surface_material_simd(positions_x, positions_y, materials, 8);

    for (size_t i = 0; i < 8; ++i) {
        float expected_height;
        bool ok = terrain.sample_height(positions_x[i], positions_y[i], expected_height);
        assert(ok);
        assert(std::abs(expected_height - heights[i]) < 1e-3f);
        assert(materials[i] == static_cast<uint8_t>(SurfaceType::GRASS));
    }

    std::cout << "✓ Terrain SIMD queries test passed" << std::endl;
}

void test_weather_and_environment() {
    std::cout << "Testing weather and environment integration..." << std::endl;

    TerrainSystem terrain;
    terrain.initialize(100.0f, 1.0f, 5);
    TerrainTile* tile = terrain.add_tile(1, Vec3(0.0, 0.0, 0.0));
    assert(tile != nullptr);

    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            tile->surface_type[y][x] = static_cast<uint8_t>(SurfaceType::DIRT);
        }
    }

    EnvironmentSystem environment;
    WeatherState weather = {0.8f, 12.0f, 1.0f, 0.0f, 8.0f, 0.9f, 3000.0f, WeatherType::RAIN, 0.0f};
    for (int frame = 0; frame < 40; ++frame) {
        environment.update_weather_effects(weather, terrain, nullptr);
    }

    bool found_mud = false;
    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            if (tile->surface_type[y][x] == static_cast<uint8_t>(SurfaceType::MUD)) {
                found_mud = true;
                break;
            }
        }
        if (found_mud) break;
    }

    assert(found_mud);
    std::cout << "✓ Weather environment test passed" << std::endl;
}

void test_destruction_effects() {
    std::cout << "Testing destruction system..." << std::endl;

    PhysicsCore physics;
    physics.initialize(4, 0);

    TerrainSystem terrain;
    terrain.initialize(100.0f, 1.0f, 5);
    terrain.add_tile(1, Vec3(0.0, 0.0, 0.0));

    DestructionSystem destruction;
    DestructibleObject building{};
    building.object_id = 42;
    building.object_type = 0;
    building.structural_integrity = 0.12f;
    building.destruction_threshold = 0.15f;
    building.world_aabb_min = _mm256_setr_ps(10.0f, 10.0f, 0.0f, 0, 0, 0, 0, 0);
    building.world_aabb_max = _mm256_setr_ps(18.0f, 18.0f, 8.0f, 0, 0, 0, 0, 0);
    building.fragment_count = 5;
    building.destruction_flags = 0;

    destruction.register_object(building);
    destruction.apply_damage(42, 0.2f, terrain, physics);
    destruction.update(0.016f, terrain);

    assert(!destruction.get_craters().empty());
    std::cout << "✓ Destruction system test passed" << std::endl;
}

void test_performance() {
    std::cout << "Testing performance..." << std::endl;

    TerrainSystem terrain;
    terrain.initialize(100.0f, 1.0f, 5);
    TerrainTile* tile = terrain.add_tile(1, Vec3(0.0, 0.0, 0.0));
    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            tile->height_data[y][x] = static_cast<float>(x + y) * 0.1f;
            tile->surface_type[y][x] = static_cast<uint8_t>(SurfaceType::GRASS);
        }
    }

    const size_t num_queries = 10000;
    std::vector<float> positions_x(num_queries);
    std::vector<float> positions_y(num_queries);
    std::vector<float> heights(num_queries);
    std::vector<uint8_t> materials(num_queries);

    for (size_t i = 0; i < num_queries; ++i) {
        positions_x[i] = static_cast<float>(rand() % 100);
        positions_y[i] = static_cast<float>(rand() % 100);
    }

    auto start = std::chrono::high_resolution_clock::now();
    terrain.query_height_simd(positions_x.data(), positions_y.data(), heights.data(), num_queries);
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
    double per_query = elapsed / num_queries;

    std::cout << "SIMD height query: " << per_query << " μs per query" << std::endl;
    assert(per_query < 1.0);  // < 1 μs target

    start = std::chrono::high_resolution_clock::now();
    terrain.query_surface_material_simd(positions_x.data(), positions_y.data(), materials.data(), num_queries);
    end = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration<double, std::micro>(end - start).count();
    per_query = elapsed / num_queries;

    std::cout << "SIMD material query: " << per_query << " μs per query" << std::endl;
    assert(per_query < 0.5);  // < 0.5 μs target

    std::cout << "✓ Performance test passed" << std::endl;
}

void test_ground_forces() {
    std::cout << "Testing ground forces calculation..." << std::endl;

    TerrainSystem terrain;
    terrain.initialize(100.0f, 1.0f, 5);
    TerrainTile* tile = terrain.add_tile(1, Vec3(0.0, 0.0, 0.0));
    for (size_t y = 0; y < TerrainSystem::kTileResolution; ++y) {
        for (size_t x = 0; x < TerrainSystem::kTileResolution; ++x) {
            tile->height_data[y][x] = 0.0f;
            tile->surface_type[y][x] = static_cast<uint8_t>(SurfaceType::DIRT);
        }
    }

    EnvironmentSystem environment;
    VehicleState vehicle{};
    vehicle.vehicle_id = 1;
    vehicle.contact_points = {Vec3(10.0f, 10.0f, 0.0f), Vec3(20.0f, 10.0f, 0.0f), Vec3(10.0f, 20.0f, 0.0f), Vec3(20.0f, 20.0f, 0.0f)};
    vehicle.contact_count = 4;
    vehicle.mass = 50000.0f;  // 50 tons
    vehicle.speed = 10.0f;

    GroundInteractionForces forces[1];
    environment.calculate_ground_forces(&vehicle, forces, 1, terrain);

    assert(forces[0].friction_coefficient > 0.0f);
    assert(forces[0].rolling_resistance > 0.0f);
    assert(forces[0].traction_factor > 0.0f);
    assert(forces[0].deformation_pressure >= 0.0f);

    std::cout << "Friction: " << forces[0].friction_coefficient << ", Rolling: " << forces[0].rolling_resistance << ", Traction: " << forces[0].traction_factor << std::endl;
    std::cout << "✓ Ground forces test passed" << std::endl;
}

int main() {
    std::cout << "\n=== Environment System Unit Tests ===" << std::endl;
    test_terrain_simd_query();
    test_weather_and_environment();
    test_destruction_effects();
    test_performance();
    test_ground_forces();
    std::cout << "\n✓ All environment system tests passed" << std::endl;
    return 0;
}
