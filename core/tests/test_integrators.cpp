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
#include <vector>
#include "physics_core/integrators.h"

using namespace physics_core;

void test_euler_integrator() {
    std::cout << "Testing Euler integrator..." << std::endl;
    
    EulerIntegrator integrator;
    std::vector<PhysicsBody> bodies;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(1, Vec3(0, 0, 0), 1.0f, Mat3x3::identity());
    body.velocity = Vec3(1, 0, 0);  // Moving in X direction
    body.acceleration = Vec3(0, -9.81f, 0);  // Gravity
    bodies.push_back(body);
    
    integrator.integrate(bodies, 1.0f / 60.0f);  // 1 frame at 60 FPS
    
    // After one frame, velocity should increase due to gravity
    assert(bodies[0].velocity.y < 0);
    
    // Position should move
    assert(bodies[0].position.x > 0);
    
    std::cout << "✓ Euler integrator test passed" << std::endl;
}

void test_verlet_integrator() {
    std::cout << "Testing Verlet integrator..." << std::endl;
    
    VerletIntegrator integrator;
    std::vector<PhysicsBody> bodies;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(1, Vec3(0, 10, 0), 1.0f, Mat3x3::identity());
    body.velocity = Vec3(0, 0, 0);
    body.acceleration = Vec3(0, -9.81f, 0);  // Gravity
    bodies.push_back(body);
    
    float dt = 1.0f / 60.0f;
    
    // Integrate multiple steps
    for (int i = 0; i < 60; ++i) {
        integrator.integrate(bodies, dt);
    }
    
    // After 1 second, should have fallen significantly
    assert(bodies[0].position.y < 10);
    
    std::cout << "✓ Verlet integrator test passed" << std::endl;
}

void test_rk4_integrator() {
    std::cout << "Testing RK4 integrator..." << std::endl;
    
    RK4Integrator integrator;
    std::vector<PhysicsBody> bodies;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(1, Vec3(0, 0, 0), 1.0f, Mat3x3::identity());
    body.velocity = Vec3(10, 0, 0);
    body.acceleration = Vec3(0, -9.81f, 0);
    bodies.push_back(body);
    
    integrator.integrate(bodies, 1.0f / 60.0f);
    
    // RK4 should produce smooth trajectories
    assert(bodies[0].position.x > 0);
    assert(bodies[0].velocity.y < 0);
    
    std::cout << "✓ RK4 integrator test passed" << std::endl;
}

int main() {
    std::cout << "\n=== Physics Integrators Tests ===" << std::endl;
    std::cout << "Testing numerical integration methods...\n" << std::endl;
    
    try {
        test_euler_integrator();
        test_verlet_integrator();
        test_rk4_integrator();
        
        std::cout << "\n✓ All integrator tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
