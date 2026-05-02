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

using namespace physics_core;

// ============================================================================
// Test Types
// ============================================================================

void test_vec3_operations() {
    std::cout << "Testing Vec3 operations..." << std::endl;
    
    Vec3 v1(1, 2, 3);
    Vec3 v2(4, 5, 6);
    
    // Addition
    Vec3 sum = v1 + v2;
    assert(std::abs(sum.x - 5) < 1e-6 && std::abs(sum.y - 7) < 1e-6 && std::abs(sum.z - 9) < 1e-6);
    
    // Subtraction
    Vec3 diff = v2 - v1;
    assert(std::abs(diff.x - 3) < 1e-6 && std::abs(diff.y - 3) < 1e-6 && std::abs(diff.z - 3) < 1e-6);
    
    // Scalar multiplication
    Vec3 scaled = v1 * 2.0;
    assert(std::abs(scaled.x - 2) < 1e-6 && std::abs(scaled.y - 4) < 1e-6 && std::abs(scaled.z - 6) < 1e-6);
    
    // Dot product
    double dot = v1.dot(v2);
    assert(std::abs(dot - 32) < 1e-6);  // 1*4 + 2*5 + 3*6 = 32
    
    // Cross product
    Vec3 cross = v1.cross(v2);
    assert(std::abs(cross.x - (-3)) < 1e-6 && std::abs(cross.y - 6) < 1e-6 && std::abs(cross.z - (-3)) < 1e-6);
    
    // Magnitude
    double mag = v1.magnitude();
    assert(std::abs(mag - std::sqrt(14)) < 1e-6);  // sqrt(1+4+9)
    
    // Normalize
    Vec3 norm = v1.normalized();
    double norm_mag = norm.magnitude();
    assert(std::abs(norm_mag - 1.0) < 1e-6);
    
    std::cout << "✓ Vec3 operations test passed" << std::endl;
}

void test_mat3x3_operations() {
    std::cout << "Testing Mat3x3 operations..." << std::endl;
    
    Mat3x3 m = Mat3x3::identity();
    Vec3 v(1, 2, 3);
    
    // Identity matrix should not change vector
    Vec3 result = m * v;
    assert(std::abs(result.x - v.x) < 1e-6);
    assert(std::abs(result.y - v.y) < 1e-6);
    assert(std::abs(result.z - v.z) < 1e-6);
    
    std::cout << "✓ Mat3x3 operations test passed" << std::endl;
}

void test_mat4x4_transformations() {
    std::cout << "Testing Mat4x4 transformations..." << std::endl;
    
    Mat4x4 m = Mat4x4::identity();
    Vec3 point(1, 2, 3);
    
    // Identity should not change point
    Vec3 result = m.transform_point(point);
    assert(std::abs(result.x - point.x) < 1e-6);
    assert(std::abs(result.y - point.y) < 1e-6);
    assert(std::abs(result.z - point.z) < 1e-6);
    
    std::cout << "✓ Mat4x4 transformations test passed" << std::endl;
}

void test_physics_body_creation() {
    std::cout << "Testing PhysicsBody creation..." << std::endl;
    
    Vec3 pos(0, 1, 0);
    PhysicsBody body = PhysicsBody::create_rigid_body(1, pos, 1.0f, Mat3x3::identity());
    
    assert(body.entity_id == 1);
    assert(std::abs(body.mass - 1.0f) < 1e-6);
    assert(std::abs(body.inv_mass - 1.0f) < 1e-6);
    assert(body.body_type == 1);  // Dynamic
    
    // Static body
    PhysicsBody static_body = PhysicsBody::create_static_body(2, pos);
    assert(static_body.body_type == 0);  // Static
    assert(static_body.inv_mass == 0.0f);
    
    std::cout << "✓ PhysicsBody creation test passed" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== Physics Core Unit Tests ===" << std::endl;
    std::cout << "Testing fundamental types and operations...\n" << std::endl;
    
    try {
        test_vec3_operations();
        test_mat3x3_operations();
        test_mat4x4_transformations();
        test_physics_body_creation();
        
        std::cout << "\n✓ All type tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
