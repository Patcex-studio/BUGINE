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
#include <physics_core/physics_core.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <unordered_map>

// Simple terrain cache
class TerrainCache {
    std::unordered_map<uint64_t, float> cache_;
    static constexpr float CELL_SIZE = 1.0f;
    
    static uint64_t hash_cell(float x, float z) {
        int32_t cx = static_cast<int32_t>(x / CELL_SIZE);
        int32_t cz = static_cast<int32_t>(z / CELL_SIZE);
        return (uint64_t)cx << 32 | (uint32_t)cz;
    }
    
public:
    float get_height(physics_core::TerrainSystem& terrain, float x, float z) {
        uint64_t key = hash_cell(x, z);
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;
        
        float h = terrain.sample_height(x, z);
        cache_[key] = h;
        return h;
    }
    
    void clear() { cache_.clear(); }
};

// Simple vehicle
struct SimpleVehicle {
    physics_core::Vec3 position;
    physics_core::Vec3 velocity;
    float speed = 10.0f;
};

int main() {
    std::cout << "Terrain Demonstration" << std::endl;
    std::cout << "Showing terrain influence on vehicle movement" << std::endl;
    
    physics_core::PhysicsCore physics;
    physics.initialize(1, 0);
    physics.set_gravity({0, -9.81f, 0});
    
    // Assume terrain is initialized in physics core
    // For demo, we'll simulate terrain effects
    
    TerrainCache height_cache;
    
    // Create vehicles on different surfaces
    SimpleVehicle tank;
    tank.position = {0, 0, 0};
    
    SimpleVehicle jeep;
    jeep.position = {50, 0, 0};
    jeep.speed = 15.0f; // Faster but less traction
    
    // Simulation
    auto start_time = std::chrono::steady_clock::now();
    float elapsed_time = 0.0f;
    float cache_clear_timer = 0.0f;
    
    while (elapsed_time < 30.0f) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - start_time).count();
        start_time = now;
        elapsed_time += dt;
        cache_clear_timer += dt;
        
        // Simulate terrain sampling (since we don't have real terrain, use fake values)
        float tank_height = 0.0f; // Assume flat
        physics_core::SurfaceType tank_surface = physics_core::SurfaceType::Asphalt;
        
        float jeep_x = jeep.position.x;
        float jeep_z = jeep.position.z;
        // Fake terrain: mud at x > 25
        float jeep_height = (jeep_x > 25) ? -0.5f : 0.0f;
        physics_core::SurfaceType jeep_surface = (jeep_x > 25) ? physics_core::SurfaceType::Mud : physics_core::SurfaceType::Asphalt;
        
        // Apply surface modifiers
        float tank_modifier = (tank_surface == physics_core::SurfaceType::Asphalt) ? 1.0f : 0.5f;
        float jeep_modifier = (jeep_surface == physics_core::SurfaceType::Asphalt) ? 1.0f : 0.3f;
        
        // Move vehicles
        tank.position.x += tank.speed * tank_modifier * dt;
        tank.position.y = tank_height;
        
        jeep.position.x += jeep.speed * jeep_modifier * dt;
        jeep.position.y = jeep_height;
        
        // Output every 2 seconds
        if (static_cast<int>(elapsed_time) % 2 == 0 && static_cast<int>(elapsed_time) != static_cast<int>(elapsed_time - dt)) {
            std::cout << "Time: " << elapsed_time << "s" << std::endl;
            std::cout << "Tank: pos(" << tank.position.x << ", " << tank.position.y << "), surface: Asphalt, speed: " << tank.speed * tank_modifier << std::endl;
            std::cout << "Jeep: pos(" << jeep.position.x << ", " << jeep.position.y << "), surface: " << (jeep_surface == physics_core::SurfaceType::Mud ? "Mud" : "Asphalt") << ", speed: " << jeep.speed * jeep_modifier << std::endl;
        }
        
        // Clear cache periodically
        if (cache_clear_timer >= 10.0f) {
            height_cache.clear();
            cache_clear_timer = 0.0f;
            std::cout << "Cache cleared" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    physics.shutdown();
    
    std::cout << "Terrain demo completed." << std::endl;
    return 0;
}