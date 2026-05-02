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

int main() {
    std::cout << "Physics Demonstration" << std::endl;
    std::cout << "Showing collision, friction, and gravity" << std::endl;
    
    physics_core::PhysicsCore physics;
    physics.initialize(1, 0); // 1 thread, Verlet
    physics.set_gravity({0, -9.81f, 0});
    
    // Create ground
    physics_core::PhysicsBody ground;
    ground.type = physics_core::BodyType::Static;
    ground.shape = physics_core::Shape::Box{100.0f, 1.0f, 100.0f};
    ground.position = {0, -1, 0};
    physics.add_body(ground);
    
    // Create falling spheres
    std::vector<physics_core::EntityID> sphere_ids;
    for (int i = 0; i < 3; ++i) {
        physics_core::PhysicsBody sphere;
        sphere.type = physics_core::BodyType::Dynamic;
        sphere.shape = physics_core::Shape::Sphere{1.0f};
        sphere.position = {static_cast<float>(i * 5 - 5), 10.0f, 0};
        sphere.mass = 1.0f;
        sphere.restitution = 0.8f;
        sphere.friction = 0.5f;
        sphere_ids.push_back(physics.add_body(sphere));
    }
    
    // Simulation loop
    auto start_time = std::chrono::steady_clock::now();
    float elapsed_time = 0.0f;
    
    while (elapsed_time < 30.0f) { // 30 seconds
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - start_time).count();
        start_time = now;
        elapsed_time += dt;
        
        physics.update(dt);
        
        // Output positions every second
        if (static_cast<int>(elapsed_time) % 1 == 0 && static_cast<int>(elapsed_time) != static_cast<int>(elapsed_time - dt)) {
            const auto& bodies = physics.get_dynamic_bodies();
            for (size_t i = 0; i < bodies.size(); ++i) {
                std::cout << "Sphere " << i << ": pos(" << bodies[i].position.x << ", " 
                          << bodies[i].position.y << ", " << bodies[i].position.z 
                          << ") vel(" << bodies[i].velocity.x << ", " << bodies[i].velocity.y 
                          << ", " << bodies[i].velocity.z << ")" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    physics.shutdown();
    
    std::cout << "Physics demo completed." << std::endl;
    return 0;
}