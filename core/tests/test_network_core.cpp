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
#include <gtest/gtest.h>
#include "network_core/network_core.h"

// Test DeltaCompressor
TEST(DeltaCompressorTest, CreateAndApplyDelta) {
    DeltaCompressor compressor;
    
    NetworkEntityState old_state;
    old_state.entity_id = 1;
    old_state.state_version = 1;
    old_state.position = _mm256_set_ps(0, 0, 0, 1, 0, 0, 0, 0); // x=0, y=0, z=0
    old_state.rotation = _mm256_set_ps(0, 0, 0, 1, 0, 0, 0, 0); // identity quaternion
    old_state.health = 100;
    old_state.timestamp = 0.0f;
    
    NetworkEntityState new_state = old_state;
    new_state.state_version = 2;
    new_state.position = _mm256_set_ps(0, 0, 1, 1, 0, 0, 0, 0); // x=1, y=0, z=0
    new_state.health = 90;
    new_state.timestamp = 1.0f;
    
    EntityStateDelta delta = compressor.create_delta(old_state, new_state);
    
    EXPECT_EQ(delta.entity_id, 1);
    EXPECT_EQ(delta.state_version, 2);
    EXPECT_EQ(delta.health_change, -10);
    EXPECT_NE(delta.changed_fields_mask & 0x1, 0); // Position changed
    EXPECT_NE(delta.changed_fields_mask & 0x4, 0); // Health changed
    
    NetworkEntityState reconstructed = compressor.apply_delta(old_state, delta);
    
    // Check that reconstructed state matches new_state
    float reconstructed_pos[8], new_pos[8];
    _mm256_storeu_ps(reconstructed_pos, reconstructed.position);
    _mm256_storeu_ps(new_pos, new_state.position);
    
    EXPECT_FLOAT_EQ(reconstructed_pos[0], new_pos[0]); // x coordinate
    EXPECT_EQ(reconstructed.health, new_state.health);
}

// Test PredictionSystem
TEST(PredictionSystemTest, BasicPrediction) {
    PredictionSystem predictor;
    
    ClientPredictionState pred_state;
    pred_state.entity_id = 1;
    pred_state.predicted_position = _mm256_set_ps(0, 0, 0, 1, 0, 0, 0, 0);
    pred_state.is_locally_controlled = true;
    
    InputCommand input;
    input.client_id = 1;
    input.entity_id = 1;
    input.input_vector = _mm256_set_ps(0, 0, 1, 0, 0, 0, 0, 0); // Forward input
    
    predictor.predict_next_state(pred_state, input, 0.016f); // 16ms frame
    
    float predicted_pos[8];
    _mm256_storeu_ps(predicted_pos, pred_state.predicted_position);
    
    // Position should have moved forward
    EXPECT_GT(predicted_pos[0], 0.0f);
}

// Test NetworkMetrics
TEST(NetworkMetricsTest, MetricsRecording) {
    NetworkMetrics metrics;
    
    metrics.record_packet_sent(100);
    metrics.record_packet_received(80);
    metrics.update_latency(50.0f);
    
    EXPECT_EQ(metrics.total_packets_sent.load(), 1);
    EXPECT_EQ(metrics.total_packets_received.load(), 1);
    EXPECT_EQ(metrics.total_bytes_sent.load(), 100);
    EXPECT_EQ(metrics.total_bytes_received.load(), 80);
    EXPECT_FLOAT_EQ(metrics.average_latency_ms.load(), 25.0f); // (0 + 50) * 0.5
}

// Test GameServer basic functionality
TEST(GameServerTest, ClientManagement) {
    GameServer server(12345, 64);
    
    EXPECT_TRUE(server.add_client(1, "127.0.0.1:1234"));
    EXPECT_TRUE(server.add_client(2, "127.0.0.1:1235"));
    
    // Try to add more than max clients (but we can't test this easily without mocking)
    
    server.remove_client(1);
    // Should be able to add another client now
    EXPECT_TRUE(server.add_client(3, "127.0.0.1:1236"));
}

// Test NetworkSystem singleton
TEST(NetworkSystemTest, Singleton) {
    NetworkSystem& instance1 = NetworkSystem::get_instance();
    NetworkSystem& instance2 = NetworkSystem::get_instance();
    
    EXPECT_EQ(&instance1, &instance2);
}