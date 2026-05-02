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
#include "physics_core/matter_systems.h"

using namespace physics_core;

void test_rigid_body_system() {
    std::cout << "Testing RigidBodySystem..." << std::endl;
    
    RigidBodySystem system;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(1, Vec3(0, 0, 0), 1.0f, Mat3x3::identity());
    EntityID id = system.add_body(body);
    
    assert(system.get_body_count() == 1);
    assert(system.get_body(id) != nullptr);
    
    system.apply_force(id, Vec3(10, 0, 0));
    system.update(1.0f / 60.0f);
    
    assert(system.get_body_count() == 1);
    
    system.remove_body(id);
    assert(system.get_body_count() == 0);
    
    std::cout << "✓ RigidBodySystem test passed" << std::endl;
}

void test_fluid_system() {
    std::cout << "Testing FluidSystem..." << std::endl;
    
    FluidSystem system;
    
    // Initialize with water particles in a cube
    std::vector<std::array<float, 3>> positions;
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            for (int z = 0; z < 10; ++z) {
                positions.push_back({x * 0.01f, y * 0.01f, z * 0.01f});
            }
        }
    }
    
    system.initialize_fluid(FluidType::WATER, positions);
    
    assert(system.get_body_count() == 1000);
    
    // Test density calculation
    float initial_density = system.get_average_density();
    std::cout << "Initial average density: " << initial_density << std::endl;
    
    system.update(1.0f / 60.0f);
    
    float updated_density = system.get_average_density();
    std::cout << "Updated average density: " << updated_density << std::endl;
    
    // Density should be close to water density (1000 kg/m³)
    assert(updated_density > 800.0f && updated_density < 1200.0f);
    
    std::cout << "✓ FluidSystem test passed" << std::endl;
}

void test_gas_system() {
    std::cout << "Testing GasSystem..." << std::endl;
    
    GasSystem system;
    
    PhysicsBody particle = PhysicsBody::create_rigid_body(1, Vec3(0, 0, 0), 0.05f, Mat3x3::identity());
    EntityID id = system.add_body(particle);
    
    assert(system.get_body_count() == 1);
    
    system.update(1.0f / 60.0f);
    
    system.remove_body(id);
    assert(system.get_body_count() == 0);
    
    std::cout << "✓ GasSystem test passed" << std::endl;
}

int main() {
    std::cout << "\n=== Matter Systems Tests ===" << std::endl;
    std::cout << "Testing physics subsystems...\n" << std::endl;
    
    try {
        test_rigid_body_system();
        test_fluid_system();
        test_gas_system();
        
        std::cout << "\n✓ All matter system tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
