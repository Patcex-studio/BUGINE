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
#include <chrono>
#include <vector>
#include "physics_core/physics_core.h"

using namespace physics_core;

// ============================================================================
// Physics Demo: Falling Bodies Simulation
// ============================================================================

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          Physics Core - Free Fall Demo              ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // Create physics engine
    PhysicsCore engine;
    engine.initialize(4);  // Use 4 worker threads
    engine.set_debug_enabled(true);
    
    std::cout << "[✓] Physics engine initialized\n" << std::endl;
    
    // Set gravity
    Vec3 gravity(0, -9.81f, 0);
    engine.set_gravity(gravity);
    std::cout << "[✓] Gravity set to: " << gravity.y << " m/s²\n" << std::endl;
    
    // Create ground (static body)
    EntityID ground = engine.create_static_body(Vec3(0, -5, 0));
    std::cout << "[✓] Created ground body (ID: " << ground << ")\n" << std::endl;
    
    // Create falling bodies
    std::cout << "\n[Creating 100 falling bodies...]\n" << std::endl;
    std::vector<EntityID> falling_bodies;
    
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            EntityID body = engine.create_rigid_body(
                Vec3(i * 1.5f - 7.5f, 10 + j * 2, 0),  // Positioned in grid
                1.0f  // 1 kg mass
            );
            falling_bodies.push_back(body);
        }
    }
    
    std::cout << "[✓] Created " << falling_bodies.size() << " bodies\n" << std::endl;
    
    // Simulation parameters
    const float dt = 1.0f / 60.0f;  // 60 FPS
    const int simulation_steps = 300;  // 5 seconds
    
    std::cout << "╭────────────────────────────────────────────────────────╮" << std::endl;
    std::cout << "│             SIMULATION RUNNING                        │" << std::endl;
    std::cout << "├────────────────────────────────────────────────────────┤" << std::endl;
    
    auto sim_start = std::chrono::high_resolution_clock::now();
    
    // Run simulation
    for (int step = 0; step < simulation_steps; ++step) {
        engine.update(dt);
        
        // Print progress every 60 steps (1 second)
        if ((step + 1) % 60 == 0) {
            float time = (step + 1) * dt;
            
            // Get statistics
            PhysicsCore::Stats stats = engine.get_stats();
            
            std::cout << "│ Time: " << time << "s | Bodies: " << stats.total_bodies 
                      << " | Frame: " << stats.avg_frame_time_ms << " ms" << std::endl;
        }
    }
    
    auto sim_end = std::chrono::high_resolution_clock::now();
    double sim_total_time = std::chrono::duration<double>(sim_end - sim_start).count();
    
    std::cout << "└────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // Print results
    PhysicsCore::Stats final_stats = engine.get_stats();
    
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║               SIMULATION RESULTS                      ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "│ Total simulation time: " << sim_total_time << " seconds" << std::endl;
    std::cout << "│ Total bodies: " << final_stats.total_bodies << std::endl;
    std::cout << "│ Rigid bodies: " << final_stats.rigid_bodies << std::endl;
    std::cout << "│ Frames processed: " << final_stats.frame_count << std::endl;
    std::cout << "│ Average frame time: " << final_stats.avg_frame_time_ms << " ms" << std::endl;
    std::cout << "│ Memory usage: " << (engine.get_memory_usage() / 1024) << " KB" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // Sample body positions
    std::cout << "[Sample positions after " << simulation_steps * dt << " seconds]:\n" << std::endl;
    
    PhysicsBody* sample_body = engine.get_body(falling_bodies[0]);
    if (sample_body) {
        std::cout << "  Body #0: position=(" << sample_body->position.x << ", "
                  << sample_body->position.y << ", " << sample_body->position.z << ")" << std::endl;
        std::cout << "           velocity=(" << sample_body->velocity.x << ", "
                  << sample_body->velocity.y << ", " << sample_body->velocity.z << ")" << std::endl;
    }
    
    std::cout << "\n[Demo Complete]\n" << std::endl;
    
    return 0;
}
