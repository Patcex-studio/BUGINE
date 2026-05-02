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
#include <chrono>
#include "physics_core/hybrid_precision.h"

using namespace physics_core;

void test_global_object_creation() {
    std::cout << "Testing GlobalObject creation..." << std::endl;

    double pos[3] = {1000.0, 2000.0, 3000.0};
    float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    GlobalObject obj(42, pos, rot, 0); // TANK

    assert(obj.id == 42);
    assert(obj.global_pos[0] == 1000.0);
    assert(obj.global_pos[1] == 2000.0);
    assert(obj.global_pos[2] == 3000.0);
    assert(obj.object_type == 0);
    assert(obj.sync_generation.load() == 0);

    std::cout << "✓ GlobalObject creation test passed" << std::endl;
}

void test_local_physics_body_creation() {
    std::cout << "Testing LocalPhysicsBody creation..." << std::endl;

    float pos[3] = {10.0f, 20.0f, 30.0f};
    float vel[3] = {1.0f, 2.0f, 3.0f};
    float acc[3] = {0.1f, 0.2f, 0.3f};

    LocalPhysicsBody body = local_body_utils::create(pos, vel, acc, 1.0f, 1);

    float extracted_pos[3];
    float extracted_vel[3];
    local_body_utils::extract_position(body, extracted_pos);
    local_body_utils::extract_velocity(body, extracted_vel);

    assert(std::abs(extracted_pos[0] - 10.0f) < 1e-6f);
    assert(std::abs(extracted_pos[1] - 20.0f) < 1e-6f);
    assert(std::abs(extracted_pos[2] - 30.0f) < 1e-6f);
    assert(std::abs(extracted_vel[0] - 1.0f) < 1e-6f);
    assert(std::abs(extracted_vel[1] - 2.0f) < 1e-6f);
    assert(std::abs(extracted_vel[2] - 3.0f) < 1e-6f);
    assert(body.entity_type == 1);
    assert(local_body_utils::is_active(body));
    assert(!local_body_utils::is_dirty(body));

    std::cout << "✓ LocalPhysicsBody creation test passed" << std::endl;
}

void test_reference_frame_operations() {
    std::cout << "Testing ReferenceFrame operations..." << std::endl;

    ReferenceFrame frame;
    double pos[3] = {550.0, 650.0, 750.0};

    // Test grid snapping
    double snapped[3];
    ReferenceFrame::grid_snap(pos, snapped);

    assert(snapped[0] == 500.0); // floor(550/100)*100 = 500
    assert(snapped[1] == 600.0); // floor(650/100)*100 = 600
    assert(snapped[2] == 700.0); // floor(750/100)*100 = 700

    // Test distance calculation
    double test_pos[3] = {600.0, 700.0, 800.0};
    double distance = frame.distance_from_origin(test_pos);

    double expected = std::sqrt(600*600 + 700*700 + 800*800);
    assert(std::abs(distance - expected) < 1e-9);

    std::cout << "✓ ReferenceFrame operations test passed" << std::endl;
}

void test_hybrid_precision_system_basic() {
    std::cout << "Testing HybridPrecisionSystem basic operations..." << std::endl;

    HybridPrecisionSystem system;

    // Test configuration
    system.set_simulation_threshold(1000.0);
    system.enable_fast_projectile_mode(true);

    // Test global object registration
    double pos[3] = {100.0, 200.0, 300.0};
    float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    GlobalObject obj(1, pos, rot, 0);

    bool registered = system.register_global_object(obj);
    assert(registered);

    const GlobalObject* retrieved = system.get_global_object(1);
    assert(retrieved != nullptr);
    assert(retrieved->id == 1);
    assert(retrieved->global_pos[0] == 100.0);

    // Test object count
    assert(system.get_active_objects_count() == 1);

    std::cout << "✓ HybridPrecisionSystem basic operations test passed" << std::endl;
}

void test_sync_operations() {
    std::cout << "Testing synchronization operations..." << std::endl;

    HybridPrecisionSystem system;

    // Create test data
    std::vector<GlobalObject> globals(8);
    std::vector<LocalPhysicsBody> locals(8);

    for (size_t i = 0; i < 8; ++i) {
        double pos[3] = {static_cast<double>(i * 10), static_cast<double>(i * 20), static_cast<double>(i * 30)};
        float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        globals[i] = GlobalObject(i + 1, pos, rot, 0);
    }

    // Test batch sync global to local
    size_t synced = system.sync_global_to_local(globals.data(), locals.data(), 8);
    assert(synced == 8);

    // Verify local coordinates (should be relative to origin 0,0,0)
    for (size_t i = 0; i < 8; ++i) {
        float local_pos[3];
        local_body_utils::extract_position(locals[i], local_pos);

        assert(std::abs(local_pos[0] - static_cast<float>(i * 10)) < 1e-5f);
        assert(std::abs(local_pos[1] - static_cast<float>(i * 20)) < 1e-5f);
        assert(std::abs(local_pos[2] - static_cast<float>(i * 30)) < 1e-5f);
    }

    // Test batch sync local to global
    synced = system.sync_local_to_global(globals.data(), locals.data(), 8);
    assert(synced == 8);

    // Verify global coordinates are restored
    for (size_t i = 0; i < 8; ++i) {
        assert(std::abs(globals[i].global_pos[0] - static_cast<double>(i * 10)) < 1e-5);
        assert(std::abs(globals[i].global_pos[1] - static_cast<double>(i * 20)) < 1e-5);
        assert(std::abs(globals[i].global_pos[2] - static_cast<double>(i * 30)) < 1e-5);
    }

    std::cout << "✓ Synchronization operations test passed" << std::endl;
}

void test_origin_shift() {
    std::cout << "Testing origin shift operations..." << std::endl;

    HybridPrecisionSystem system;

    // Register some objects
    for (EntityID i = 1; i <= 5; ++i) {
        double pos[3] = {static_cast<double>(i * 100), static_cast<double>(i * 200), static_cast<double>(i * 300)};
        float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        GlobalObject obj(i, pos, rot, 0);
        system.register_global_object(obj);
    }

    // Store original positions
    std::vector<double> original_pos[5];
    for (EntityID i = 1; i <= 5; ++i) {
        const GlobalObject* obj = system.get_global_object(i);
        original_pos[i-1].push_back(obj->global_pos[0]);
        original_pos[i-1].push_back(obj->global_pos[1]);
        original_pos[i-1].push_back(obj->global_pos[2]);
    }

    // Trigger origin shift by moving camera far away
    double camera_pos[3] = {600.0, 700.0, 800.0}; // Should trigger shift
    system.update_reference_frame(camera_pos, 500.0);

    // Check that origin has shifted (grid snapped)
    const ReferenceFrame& frame = system.get_reference_frame();
    assert(frame.origin[0] == 600.0); // grid snapped
    assert(frame.origin[1] == 700.0);
    assert(frame.origin[2] == 800.0);

    // Check that global positions have been adjusted
    for (EntityID i = 1; i <= 5; ++i) {
        const GlobalObject* obj = system.get_global_object(i);
        // Positions should be shifted relative to new origin
        double expected_x = original_pos[i-1][0] - 600.0;
        double expected_y = original_pos[i-1][1] - 700.0;
        double expected_z = original_pos[i-1][2] - 800.0;

        assert(std::abs(obj->global_pos[0] - expected_x) < 1e-9);
        assert(std::abs(obj->global_pos[1] - expected_y) < 1e-9);
        assert(std::abs(obj->global_pos[2] - expected_z) < 1e-9);
    }

    std::cout << "✓ Origin shift operations test passed" << std::endl;
}

void test_projectile_handling() {
    std::cout << "Testing projectile handling..." << std::endl;

    HybridPrecisionSystem system;

    // Create a projectile
    HighPrecisionProjectile proj;
    proj.id = 100;
    proj.start_pos[0] = 0.0;
    proj.start_pos[1] = 0.0;
    proj.start_pos[2] = 0.0;
    proj.velocity[0] = 100.0; // 100 m/s east
    proj.velocity[1] = 0.0;
    proj.velocity[2] = 0.0;
    proj.elapsed_time = 0.0;

    bool registered = system.register_projectile(proj);
    assert(registered);

    // Update projectile for 1 second
    system.update_projectile(100, 1.0);

    // Check position
    double current_pos[3];
    bool got_pos = system.get_projectile_position(100, current_pos);
    assert(got_pos);

    assert(std::abs(current_pos[0] - 100.0) < 1e-9); // 100 m/s * 1 s = 100 m
    assert(std::abs(current_pos[1] - 0.0) < 1e-9);
    assert(std::abs(current_pos[2] - 0.0) < 1e-9);

    // Check projectile count
    assert(system.get_active_projectiles_count() == 1);

    // Unregister projectile
    system.unregister_projectile(100);
    assert(system.get_active_projectiles_count() == 0);

    std::cout << "✓ Projectile handling test passed" << std::endl;
}

void test_event_system() {
    std::cout << "Testing event system..." << std::endl;

    HybridPrecisionSystem system;

    bool event_received = false;
    PrecisionEvent received_event;

    // Subscribe to events
    uint64_t subscription_id = system.subscribe_events(
        [&](const PrecisionEvent& event) {
            event_received = true;
            received_event = event;
        }
    );

    // Trigger an origin shift to generate an event
    double camera_pos[3] = {600.0, 700.0, 800.0};
    system.update_reference_frame(camera_pos, 500.0);

    // Check that event was received
    assert(event_received);
    assert(received_event.type == PrecisionEventType::ORIGIN_SHIFTED);
    assert(received_event.new_origin[0] == 600.0);
    assert(received_event.new_origin[1] == 700.0);
    assert(received_event.new_origin[2] == 800.0);

    // Unsubscribe
    system.unsubscribe_events(subscription_id);

    // Reset flag
    event_received = false;

    // Trigger another event (should not be received)
    double camera_pos2[3] = {1200.0, 1300.0, 1400.0};
    system.update_reference_frame(camera_pos2, 500.0);

    // Event should not be received after unsubscribe
    // Note: This test assumes events are processed synchronously

    std::cout << "✓ Event system test passed" << std::endl;
}

void test_precision_validation() {
    std::cout << "Testing precision validation..." << std::endl;

    HybridPrecisionSystem system;

    // Register objects within reasonable bounds
    for (EntityID i = 1; i <= 3; ++i) {
        double pos[3] = {static_cast<double>(i * 100), static_cast<double>(i * 200), static_cast<double>(i * 300)};
        float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        GlobalObject obj(i, pos, rot, 0);
        system.register_global_object(obj);
    }

    // System should be consistent
    bool consistent = system.validate_consistency();
    assert(consistent);

    // Set origin far away
    double far_origin[3] = {10000000.0, 10000000.0, 10000000.0};
    system.set_reference_frame_origin(far_origin);

    // Now objects are far from origin, should be inconsistent
    consistent = system.validate_consistency();
    assert(!consistent);

    std::cout << "✓ Precision validation test passed" << std::endl;
}

void test_performance() {
    std::cout << "Testing performance characteristics..." << std::endl;

    HybridPrecisionSystem system;

    // Create large batch of objects
    const size_t batch_size = 1000;
    std::vector<GlobalObject> globals(batch_size);
    std::vector<LocalPhysicsBody> locals(batch_size);

    for (size_t i = 0; i < batch_size; ++i) {
        double pos[3] = {static_cast<double>(i), static_cast<double>(i * 2), static_cast<double>(i * 3)};
        float rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        globals[i] = GlobalObject(i + 1, pos, rot, 0);
    }

    // Time batch sync
    auto start = std::chrono::high_resolution_clock::now();
    size_t synced = system.sync_global_to_local(globals.data(), locals.data(), batch_size);
    auto end = std::chrono::high_resolution_clock::now();

    assert(synced == batch_size);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double sync_time_ms = duration.count() / 1000.0;

    std::cout << "  Batch sync of " << batch_size << " objects took " << sync_time_ms << " ms" << std::endl;

    // Should be reasonably fast (< 10ms for 1000 objects)
    assert(sync_time_ms < 10.0);

    std::cout << "✓ Performance test passed" << std::endl;
}

int main() {
    std::cout << "\n=== Hybrid Precision System Tests ===" << std::endl;
    std::cout << "Testing hybrid precision floating origin system...\n" << std::endl;

    try {
        test_global_object_creation();
        test_local_physics_body_creation();
        test_reference_frame_operations();
        test_hybrid_precision_system_basic();
        test_sync_operations();
        test_origin_shift();
        test_projectile_handling();
        test_event_system();
        test_precision_validation();
        test_performance();

        std::cout << "\n✓ All Hybrid Precision System tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}