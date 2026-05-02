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
#include "physics_core/world_space_manager.h"

using namespace physics_core;

void test_local_frame_creation() {
    std::cout << "Testing local frame creation..." << std::endl;
    
    WorldSpaceManager manager;
    Vec3 pos(1, 2, 3);
    
    EntityID frame1 = manager.create_local_frame(pos);
    assert(frame1 != 0);
    
    auto frame = manager.get_frame(frame1);
    assert(frame != nullptr);
    assert(std::abs(frame->get_position().x - 1) < 1e-6);
    
    std::cout << "✓ Local frame creation test passed" << std::endl;
}

void test_coordinate_transformation() {
    std::cout << "Testing coordinate transformations..." << std::endl;
    
    WorldSpaceManager manager;
    Vec3 local_pos(1, 0, 0);
    Vec3 world_pos(5, 5, 5);
    
    EntityID frame = manager.create_local_frame(world_pos);
    
    // Transform from local to world
    Vec3 result = manager.local_to_world(local_pos, frame);
    // Should be around (6, 5, 5)
    assert(std::abs(result.x - 6) < 1e-6);
    assert(std::abs(result.y - 5) < 1e-6);
    assert(std::abs(result.z - 5) < 1e-6);
    
    std::cout << "✓ Coordinate transformation test passed" << std::endl;
}

void test_frame_hierarchy() {
    std::cout << "Testing frame hierarchy..." << std::endl;
    
    WorldSpaceManager manager;
    
    // Create parent frame
    EntityID parent = manager.create_local_frame(Vec3(0, 0, 0));
    
    // Create child frame
    EntityID child = manager.create_local_frame(Vec3(1, 0, 0), Mat3x3::identity(), 1.0, parent);
    
    assert(manager.get_parent_frame(child) == parent);
    assert(manager.get_frame_depth(child) == 1);
    assert(manager.get_frame_depth(parent) == 0);
    
    std::cout << "✓ Frame hierarchy test passed" << std::endl;
}

void test_frame_destruction() {
    std::cout << "Testing frame destruction..." << std::endl;
    
    WorldSpaceManager manager;
    EntityID frame = manager.create_local_frame(Vec3(0, 0, 0));
    
    assert(manager.get_frame_count() == 1);
    
    manager.destroy_local_frame(frame);
    
    assert(manager.get_frame_count() == 0);
    assert(manager.get_frame(frame) == nullptr);
    
    std::cout << "✓ Frame destruction test passed" << std::endl;
}

int main() {
    std::cout << "\n=== World Space Manager Tests ===" << std::endl;
    std::cout << "Testing coordinate system management...\n" << std::endl;
    
    try {
        test_local_frame_creation();
        test_coordinate_transformation();
        test_frame_hierarchy();
        test_frame_destruction();
        
        std::cout << "\n✓ All world space tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
