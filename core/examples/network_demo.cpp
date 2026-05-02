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
#include "network_core/network_core.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Historica Universalis - Network System Demo" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Get network system instance
    NetworkSystem& network = NetworkSystem::get_instance();
    
    // Create a simple server instance
    GameServer server(12345, 64);
    
    std::cout << "Starting game server..." << std::endl;
    server.start();
    
    // Simulate some clients connecting
    std::cout << "Simulating client connections..." << std::endl;
    server.add_client(1, "192.168.1.100:54321");
    server.add_client(2, "192.168.1.101:54322");
    server.add_client(3, "192.168.1.102:54323");
    
    // Create some test entities
    NetworkEntityState entity1;
    entity1.entity_id = 1001;
    entity1.state_version = 1;
    entity1.position = _mm256_set_ps(0, 0, 0, 1, 10, 0, 0, 0); // Position at (10, 0, 0)
    entity1.rotation = _mm256_set_ps(0, 0, 0, 1, 0, 0, 0, 0); // Identity rotation
    entity1.health = 100;
    entity1.timestamp = 0.0f;
    
    NetworkEntityState entity2 = entity1;
    entity2.entity_id = 1002;
    entity2.position = _mm256_set_ps(0, 0, 0, 1, 20, 0, 0, 0); // Position at (20, 0, 0)
    
    // Update server with entity states
    server.update_entity_state(1001, entity1);
    server.update_entity_state(1002, entity2);
    
    // Simulate server updates
    std::cout << "Running server simulation..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        // Update entity positions (simulate movement)
        entity1.position = _mm256_add_ps(entity1.position, _mm256_set_ps(0, 0, 0, 0, 0.1f, 0, 0, 0));
        entity1.state_version++;
        entity1.timestamp += 0.016f; // 16ms
        
        entity2.position = _mm256_add_ps(entity2.position, _mm256_set_ps(0, 0, 0, 0, -0.1f, 0, 0, 0));
        entity2.state_version++;
        entity2.timestamp += 0.016f;
        
        server.update_entity_state(1001, entity1);
        server.update_entity_state(1002, entity2);
        
        // Broadcast updates
        server.broadcast_entity_update(1001);
        server.broadcast_entity_update(1002);
        
        // Server update
        server.update(0.016f);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Simulate client input
    std::cout << "Simulating client input..." << std::endl;
    InputCommand input;
    input.client_id = 1;
    input.entity_id = 1001;
    input.sequence_number = 1;
    input.command_type = 1; // Move forward
    input.input_vector = _mm256_set_ps(0, 0, 1, 0, 0, 0, 0, 0);
    input.timestamp = 1.0f;
    
    network.send_input_to_server(input, 1.0f);
    
    // Test delta compression
    std::cout << "Testing delta compression..." << std::endl;
    DeltaCompressor compressor;
    
    NetworkEntityState old_state = entity1;
    entity1.health = 80; // Simulate damage
    entity1.state_version++;
    
    EntityStateDelta delta = compressor.create_delta(old_state, entity1);
    NetworkEntityState reconstructed = compressor.apply_delta(old_state, delta);
    
    std::cout << "Original health: " << old_state.health << std::endl;
    std::cout << "Modified health: " << entity1.health << std::endl;
    std::cout << "Reconstructed health: " << reconstructed.health << std::endl;
    std::cout << "Delta compression test: " << (reconstructed.health == entity1.health ? "PASSED" : "FAILED") << std::endl;
    
    // Clean up
    std::cout << "Shutting down server..." << std::endl;
    server.stop();
    
    std::cout << "Network demo completed successfully!" << std::endl;
    return 0;
}