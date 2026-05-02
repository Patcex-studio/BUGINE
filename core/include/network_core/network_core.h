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
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <queue>
#include <immintrin.h> // For SIMD types
#include <cmath> // For std::round

// Forward declarations for ASIO (only if available)
#ifdef ASIO_STANDALONE
#include <asio.hpp>
namespace asio {
class io_context;
}
#elif defined(NO_ASIO)
// Stub declarations when ASIO is not available
namespace asio {
class io_context {
public:
    void run() {}
    void stop() {}
};
}
#else
// Forward declaration
namespace asio {
class io_context;
}
#endif

// Type definitions
using EntityID = uint64_t;
using ClientID = uint32_t;
using ComponentTypeID = uint16_t;

// Core network types
struct NetworkEntityState {
    EntityID entity_id;
    uint32_t state_version;
    __m256 position;        // SIMD position (x,y,z,w)
    __m256 rotation;        // SIMD rotation quaternion
    uint32_t health;
    uint32_t component_states;
    float timestamp;
};

struct EntityStateDelta {
    EntityID entity_id;
    uint32_t state_version;
    uint32_t changed_fields_mask;
    __m256 position_delta;
    __m256 rotation_delta;
    uint32_t health_change;
    uint32_t component_states;
    float timestamp;
};

struct InputCommand {
    ClientID client_id;
    EntityID entity_id;
    uint32_t sequence_number;
    uint32_t command_type;
    __m256 input_vector;    // SIMD input data
    float timestamp;
};

struct ClientSession {
    ClientID client_id;
    std::string client_address;
    uint32_t last_sequence_received;
    uint32_t last_sequence_acknowledged;
    float connection_quality; // 0.0 to 1.0
    std::chrono::steady_clock::time_point last_update_time;
    std::vector<InputCommand> pending_inputs;
};

struct ClientPredictionState {
    EntityID entity_id;
    __m256 predicted_position;
    __m256 predicted_velocity;
    __m256 server_position;
    float prediction_error;
    float last_update_time;
    bool is_locally_controlled;
    InputCommand last_input;
};

// Network System Classes
class DeltaCompressor {
public:
    EntityStateDelta create_delta(const NetworkEntityState& old_state,
                                const NetworkEntityState& new_state);
    
    NetworkEntityState apply_delta(const NetworkEntityState& base_state,
                                 const EntityStateDelta& delta);
    
    // Accessor methods for StateSnapshotGenerator
    const std::unordered_map<EntityID, NetworkEntityState>& get_baseline_states() const {
        return baseline_states_;
    }
    
    void update_baseline_state(EntityID entity_id, const NetworkEntityState& state) {
        baseline_states_[entity_id] = state;
    }
    
private:
    std::unordered_map<EntityID, NetworkEntityState> baseline_states_;
    std::vector<EntityStateDelta> pending_deltas_;
    
    float quantize_position(float value, float precision = 0.01f);
    uint16_t quantize_rotation(float radians, uint16_t bits = 16);
    uint8_t pack_flags(const std::vector<bool>& flags);
};

class PredictionSystem {
public:
    void predict_next_state(ClientPredictionState& pred_state,
                          const InputCommand& input,
                          float delta_time);
    
    void reconcile_with_server(EntityID entity_id,
                             const NetworkEntityState& server_state);
    
    __m256 interpolate_position(EntityID entity_id,
                              float interpolation_time);
    
private:
    std::unordered_map<EntityID, ClientPredictionState> prediction_states_;
};

class MessageProcessor {
public:
    void process_incoming_packet(const std::vector<uint8_t>& packet_data,
                               ClientID client_id);
    
    void queue_outgoing_update(const NetworkEntityState& state_update,
                             const std::vector<ClientID>& target_clients);
    
    // Add incoming packet to queue
    void queue_incoming_packet(const std::vector<uint8_t>& packet_data, ClientID client_id);

    // Access queued incoming packets
    bool has_incoming_packet() const;
    std::pair<std::vector<uint8_t>, ClientID> pop_next_incoming_packet();

private:
    std::vector<std::function<void(const std::vector<uint8_t>&, ClientID)>> message_handlers_;
    std::queue<std::pair<std::vector<uint8_t>, ClientID>> incoming_queue_;
};

class StateSnapshotGenerator {
public:
    std::vector<EntityStateDelta> generate_snapshot_deltas(
        const std::unordered_map<EntityID, NetworkEntityState>& current_states);
    
private:
    DeltaCompressor compressor_;
};

class ClientUpdateManager {
public:
    void broadcast_state_updates(const std::vector<EntityStateDelta>& deltas);
    void send_reliable_update(ClientID client_id, const EntityStateDelta& delta);
    
private:
    std::unordered_map<ClientID, std::vector<EntityStateDelta>> client_queues_;
};

class NetworkMetrics {
public:
    std::atomic<uint64_t> total_packets_sent{0};
    std::atomic<uint64_t> total_packets_received{0};
    std::atomic<uint64_t> total_bytes_sent{0};
    std::atomic<uint64_t> total_bytes_received{0};
    std::atomic<uint32_t> active_connections{0};
    std::atomic<float> average_latency_ms{0.0f};
    
    void record_packet_sent(size_t bytes);
    void record_packet_received(size_t bytes);
    void update_latency(float latency_ms);
};

class GameServer {
public:
    GameServer(uint16_t port, uint32_t max_clients);
    ~GameServer();
    
    void start();
    void run();
    void stop();
    void update(float delta_time);
    void queue_incoming_packet(const std::vector<uint8_t>& packet_data, ClientID client_id);
    bool has_incoming_packet() const;
    std::pair<std::vector<uint8_t>, ClientID> pop_next_incoming_packet();
    
    // Client management
    bool add_client(ClientID client_id, const std::string& address);
    void remove_client(ClientID client_id);
    
    // State management
    void update_entity_state(EntityID entity_id, const NetworkEntityState& state);
    void broadcast_entity_update(EntityID entity_id);
    
private:
    // Authoritative game state
    std::unordered_map<EntityID, NetworkEntityState> authoritative_state_;
    
    // Client management
    std::unordered_map<ClientID, ClientSession> connected_clients_;
    std::atomic<uint32_t> active_client_count_{0};
    
    // Network infrastructure
    std::vector<std::thread> network_threads_;
    std::vector<std::unique_ptr<asio::io_context>> io_contexts_;
    // UDP/TCP listeners would be implemented here
    
    // Message processing
    MessageProcessor message_processor_;
    StateSnapshotGenerator snapshot_generator_;
    ClientUpdateManager update_manager_;
    
    // Performance monitoring
    NetworkMetrics metrics_;
    
    bool running_;
    uint16_t server_port_;
    uint32_t max_clients_;
};

class NetworkSystem {
public:
    static NetworkSystem& get_instance();
    
    // Server-side functions
    void process_incoming_packets();
    void generate_and_broadcast_state_updates();
    
    // Client-side functions
    void send_input_to_server(const InputCommand& input_command, float client_timestamp);
    
    // Integration functions
    void sync_physics_state_to_clients(const std::vector<NetworkEntityState>& physics_states);
    void serialize_entity_components_for_network(EntityID entity_id,
                                               const std::vector<ComponentTypeID>& changed_components,
                                               NetworkEntityState& output_state);
    void handle_ai_unit_network_behavior(EntityID ai_entity_id,
                                       const std::vector<uint8_t>& ai_state_data,
                                       std::vector<uint8_t>& output_update);
    
    // Military simulator specific
    void manage_large_battle_networking(uint32_t expected_players,
                                       uint32_t expected_entities,
                                       const std::vector<uint8_t>& battle_map_data,
                                       float server_tick_rate);
    void validate_client_actions(ClientID client_id,
                               const InputCommand& action,
                               const std::unordered_map<EntityID, NetworkEntityState>& current_state);
    void create_battle_instance(const std::vector<uint8_t>& config_data,
                              std::vector<std::string>& available_servers);
    
private:
    NetworkSystem();
    ~NetworkSystem() = default;
    
    std::unique_ptr<GameServer> game_server_;
    PredictionSystem prediction_system_;
    DeltaCompressor delta_compressor_;
    
    // Threading and synchronization
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
};