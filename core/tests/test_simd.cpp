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
#include "physics_core/simd_processor.h"

using namespace physics_core;

void test_simd_detection() {
    std::cout << "Testing SIMD detection..." << std::endl;
    
    SIMDProcessor processor;
    
    std::cout << "  - AVX2 available: " << (processor.has_avx2() ? "Yes" : "No") << std::endl;
    std::cout << "  - AVX-512 available: " << (processor.has_avx512() ? "Yes" : "No") << std::endl;
    std::cout << "  - Optimal batch size: " << processor.get_optimal_batch_size() << std::endl;
    
    std::cout << "✓ SIMD detection test passed" << std::endl;
}

void test_gravity_batch() {
    std::cout << "Testing SIMD gravity batch..." << std::endl;
    
    SIMDProcessor processor;
    
    std::vector<PhysicsBody> bodies;
    for (int i = 0; i < 10; ++i) {
        PhysicsBody body = PhysicsBody::create_rigid_body(i, Vec3(0, i, 0), 1.0f, Mat3x3::identity());
        bodies.push_back(body);
    }
    
    Vec3 gravity(0, -9.81f, 0);
    processor.apply_gravity_batch(bodies, gravity, 1.0f / 60.0f);
    
    // All bodies should have acceleration from gravity
    for (const auto& body : bodies) {
        assert(body.acceleration.y < 0);
    }
    
    std::cout << "✓ Gravity batch test passed" << std::endl;
}

void test_damping_batch() {
    std::cout << "Testing SIMD damping batch..." << std::endl;
    
    SIMDProcessor processor;
    
    std::vector<PhysicsBody> bodies;
    for (int i = 0; i < 8; ++i) {
        PhysicsBody body = PhysicsBody::create_rigid_body(i, Vec3(0, 0, 0), 1.0f, Mat3x3::identity());
        body.velocity = Vec3(10, 10, 10);
        body.linear_damping = 0.1f;
        bodies.push_back(body);
    }
    
    std::vector<double> initial_speeds;
    initial_speeds.reserve(bodies.size());
    for (const auto& body : bodies) {
        initial_speeds.push_back(body.velocity.magnitude());
    }

    processor.apply_damping_batch(bodies, 1.0f / 60.0f);
    
    // All velocities should be reduced relative to their initial speed
    for (size_t i = 0; i < bodies.size(); ++i) {
        assert(bodies[i].velocity.magnitude() < initial_speeds[i]);
    }
    
    std::cout << "✓ Damping batch test passed" << std::endl;
}

int main() {
    std::cout << "\n=== SIMD Processor Tests ===" << std::endl;
    std::cout << "Testing SIMD optimizations...\n" << std::endl;
    
    try {
        test_simd_detection();
        test_gravity_batch();
        test_damping_batch();
        
        std::cout << "\n✓ All SIMD tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
