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
#include <cmath>

// Simple projectile structure
struct SimpleProjectile {
    physics_core::Vec3 position;
    physics_core::Vec3 velocity;
    float damage;
    float penetration_power;
    
    void update(float dt) {
        // Gravity
        velocity.y -= 9.81f * dt;
        // Air resistance
        velocity *= 0.99f;
        position += velocity * dt;
    }
    
    float calculate_penetration(float distance, float impact_angle_deg) const {
        float base_pen = penetration_power * std::max(0.0f, 1.0f - distance / 2000.0f);
        float angle_factor = std::cos(impact_angle_deg * 3.14159f / 180.0f);
        return base_pen * angle_factor;
    }
};

// Simple vehicle structure
struct SimpleVehicle {
    physics_core::Vec3 position;
    float health = 100.0f;
    float armor_thickness = 200.0f; // mm
    physics_core::Vec3 barrel_tip;
    physics_core::Vec3 barrel_direction = {1.0f, 0.0f, 0.0f};
};

int main() {
    std::cout << "Combat Simulation Demo" << std::endl;
    std::cout << "Simulating tank battle with ballistics and damage" << std::endl;
    
    physics_core::PhysicsCore physics;
    physics.initialize(1, 0); // 1 thread, Verlet integrator
    physics.set_gravity({0, -9.81f, 0});
    
    // Create ground
    physics_core::PhysicsBody ground;
    ground.type = physics_core::BodyType::Static;
    ground.shape = physics_core::Shape::Box{1000.0f, 1.0f, 1000.0f};
    ground.position = {0, -1, 0};
    physics.add_body(ground);
    
    // Create tanks
    SimpleVehicle player_tank;
    player_tank.position = {0, 0, 0};
    player_tank.barrel_tip = player_tank.position + physics_core::Vec3{5, 2, 0};
    
    SimpleVehicle enemy_tank;
    enemy_tank.position = {100, 0, 0};
    
    // Simulation loop
    auto start_time = std::chrono::steady_clock::now();
    float elapsed_time = 0.0f;
    float fire_timer = 0.0f;
    int hits = 0, misses = 0;
    
    while (elapsed_time < 60.0f) { // 60 seconds demo
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - start_time).count();
        start_time = now;
        elapsed_time += dt;
        
        // Fire every second
        fire_timer += dt;
        if (fire_timer >= 1.0f) {
            fire_timer = 0.0f;
            
            SimpleProjectile proj;
            proj.position = player_tank.barrel_tip;
            proj.velocity = player_tank.barrel_direction * 800.0f; // 800 m/s
            proj.damage = 50.0f;
            proj.penetration_power = 400.0f; // mm
            
            // Simulate flight
            bool hit = false;
            for (int step = 0; step < 100; ++step) { // Max 1 second flight
                proj.update(0.01f);
                
                float dist = (proj.position - enemy_tank.position).length();
                if (dist < 5.0f) { // Hit radius
                    float impact_angle = 0.0f; // Simplified
                    float pen = proj.calculate_penetration(dist, impact_angle);
                    
                    if (pen >= enemy_tank.armor_thickness) {
                        enemy_tank.health -= proj.damage;
                        hits++;
                        std::cout << "HIT! Penetration: " << pen << "mm, Enemy HP: " << enemy_tank.health << std::endl;
                    } else {
                        misses++;
                        std::cout << "RICOCHET! Needed: " << enemy_tank.armor_thickness << "mm, got: " << pen << std::endl;
                    }
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                misses++;
                std::cout << "MISS!" << std::endl;
            }
        }
        
        // Update physics
        physics.update(dt);
        
        // Output every 5 seconds
        if (static_cast<int>(elapsed_time) % 5 == 0 && static_cast<int>(elapsed_time) != static_cast<int>(elapsed_time - dt)) {
            std::cout << "Time: " << elapsed_time << "s, Player HP: " << player_tank.health 
                      << ", Enemy HP: " << enemy_tank.health << ", Hits: " << hits << ", Misses: " << misses << std::endl;
        }
        
        // Check win condition
        if (enemy_tank.health <= 0) {
            std::cout << "Player wins! Enemy destroyed." << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    physics.shutdown();
    
    std::cout << "Demo ended. Final stats - Hits: " << hits << ", Misses: " << misses << std::endl;
    return 0;
}