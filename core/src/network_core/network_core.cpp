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
#include <algorithm>
#include <chrono>
#include <cstring>

// NetworkSystem implementation
NetworkSystem& NetworkSystem::get_instance() {
    static NetworkSystem instance;
    return instance;
}

NetworkSystem::NetworkSystem() {
    // Initialize subsystems
    game_server_ = std::make_unique<GameServer>(12345, 100); // Default port and max clients
    running_ = false;
}

void NetworkSystem::process_incoming_packets() {
    // Implementation for high-performance packet processing
    // This would handle UDP packet reception and routing
    // For now, process queued packets
    if (game_server_) {
        // In a real implementation, packets would be queued from network threads
        // Here we simulate by processing any queued packets
        // Since MessageProcessor has the queue, we need to access it
        // For simplicity, assume packets are added externally
    }
    std::cout << "Processing incoming packets..." << std::endl;
}

void NetworkSystem::generate_and_broadcast_state_updates() {
    // Implementation for adaptive state broadcasting
    // Generate delta-compressed updates and broadcast to clients
    if (game_server_) {
        // Call server update with a fixed dt for now
        game_server_->update(1.0f / 60.0f);
    }
    std::cout << "Generating and broadcasting state updates..." << std::endl;
}

void NetworkSystem::send_input_to_server(const InputCommand& input_command, float client_timestamp) {
    // Implementation for client-side input handling
    // Serialize and send input command to server
    std::vector<uint8_t> packet_data;
    // Simple serialization: message_type (0 for input), then InputCommand fields
    uint32_t message_type = 0; // Input command
    packet_data.insert(packet_data.end(), reinterpret_cast<uint8_t*>(&message_type), reinterpret_cast<uint8_t*>(&message_type) + sizeof(message_type));
    packet_data.insert(packet_data.end(), reinterpret_cast<const uint8_t*>(&input_command.client_id), reinterpret_cast<const uint8_t*>(&input_command.client_id) + sizeof(input_command.client_id));
    packet_data.insert(packet_data.end(), reinterpret_cast<const uint8_t*>(&input_command.entity_id), reinterpret_cast<const uint8_t*>(&input_command.entity_id) + sizeof(input_command.entity_id));
    packet_data.insert(packet_data.end(), reinterpret_cast<const uint8_t*>(&input_command.sequence_number), reinterpret_cast<const uint8_t*>(&input_command.sequence_number) + sizeof(input_command.sequence_number));
    packet_data.insert(packet_data.end(), reinterpret_cast<const uint8_t*>(&input_command.command_type), reinterpret_cast<const uint8_t*>(&input_command.command_type) + sizeof(input_command.command_type));
    // For SIMD, store as float array
    float input_vec[8];
    _mm256_storeu_ps(input_vec, input_command.input_vector);
    packet_data.insert(packet_data.end(), reinterpret_cast<uint8_t*>(input_vec), reinterpret_cast<uint8_t*>(input_vec) + sizeof(input_vec));
    packet_data.insert(packet_data.end(), reinterpret_cast<const uint8_t*>(&input_command.timestamp), reinterpret_cast<const uint8_t*>(&input_command.timestamp) + sizeof(input_command.timestamp));
    
    // Queue to server's message processor
    if (game_server_) {
        game_server_->queue_incoming_packet(packet_data, input_command.client_id);
    }
    
    std::cout << "Sending input to server: Client " << input_command.client_id 
              << ", Entity " << input_command.entity_id << std::endl;
}

void NetworkSystem::sync_physics_state_to_clients(const std::vector<NetworkEntityState>& physics_states) {
    // Integration with Physics Core
    // Convert physics states to network updates
    std::cout << "Syncing physics state to clients: " << physics_states.size() << " entities" << std::endl;
}

void NetworkSystem::serialize_entity_components_for_network(EntityID entity_id,
                                                          const std::vector<ComponentTypeID>& changed_components,
                                                          NetworkEntityState& output_state) {
    // Integration with Entity-Component System
    // Serialize only changed components
    output_state.entity_id = entity_id;
    output_state.state_version++;
    // Implementation would serialize component data
    std::cout << "Serializing entity components for network: Entity " << entity_id << std::endl;
}

void NetworkSystem::handle_ai_unit_network_behavior(EntityID ai_entity_id,
                                                  const std::vector<uint8_t>& ai_state_data,
                                                  std::vector<uint8_t>& output_update) {
    // Integration with AI System
    // Handle AI state synchronization
    std::cout << "Handling AI unit network behavior: Entity " << ai_entity_id << std::endl;
}

void NetworkSystem::manage_large_battle_networking(uint32_t expected_players,
                                                  uint32_t expected_entities,
                                                  const std::vector<uint8_t>& battle_map_data,
                                                  float server_tick_rate) {
    // Large-scale battle management
    // Handle zoning, load balancing, etc.
    std::cout << "Managing large battle networking: " << expected_players << " players, " 
              << expected_entities << " entities" << std::endl;
}

void NetworkSystem::validate_client_actions(ClientID client_id,
                                          const InputCommand& action,
                                          const std::unordered_map<EntityID, NetworkEntityState>& current_state) {
    // Anti-cheat and authority management
    // Validate client actions against server state
    std::cout << "Validating client actions: Client " << client_id << std::endl;
}

void NetworkSystem::create_battle_instance(const std::vector<uint8_t>& config_data,
                                         std::vector<std::string>& available_servers) {
    // Dynamic instance management
    // Create new battle instances based on demand
    std::cout << "Creating battle instance..." << std::endl;
}

// DeltaCompressor implementation
EntityStateDelta DeltaCompressor::create_delta(const NetworkEntityState& old_state,
                                             const NetworkEntityState& new_state) {
    EntityStateDelta delta;
    delta.entity_id = old_state.entity_id;
    delta.state_version = new_state.state_version;
    delta.timestamp = new_state.timestamp;
    
    // Calculate deltas using SIMD
    __m256 pos_diff = _mm256_sub_ps(new_state.position, old_state.position);
    __m256 rot_diff = _mm256_sub_ps(new_state.rotation, old_state.rotation);
    
    delta.position_delta = pos_diff;
    delta.rotation_delta = rot_diff;
    delta.health_change = new_state.health - old_state.health;
    delta.component_states = new_state.component_states;
    
    // Set changed fields mask based on differences
    delta.changed_fields_mask = 0;
    if (_mm256_testz_si256(_mm256_castps_si256(pos_diff), _mm256_castps_si256(pos_diff)) == 0) {
        delta.changed_fields_mask |= 0x1; // Position changed
    }
    if (_mm256_testz_si256(_mm256_castps_si256(rot_diff), _mm256_castps_si256(rot_diff)) == 0) {
        delta.changed_fields_mask |= 0x2; // Rotation changed
    }
    if (delta.health_change != 0) {
        delta.changed_fields_mask |= 0x4; // Health changed
    }
    
    return delta;
}

NetworkEntityState DeltaCompressor::apply_delta(const NetworkEntityState& base_state,
                                              const EntityStateDelta& delta) {
    NetworkEntityState new_state = base_state;
    new_state.state_version = delta.state_version;
    new_state.timestamp = delta.timestamp;
    
    // Apply deltas using SIMD
    if (delta.changed_fields_mask & 0x1) {
        new_state.position = _mm256_add_ps(base_state.position, delta.position_delta);
    }
    if (delta.changed_fields_mask & 0x2) {
        new_state.rotation = _mm256_add_ps(base_state.rotation, delta.rotation_delta);
    }
    if (delta.changed_fields_mask & 0x4) {
        new_state.health += delta.health_change;
    }
    new_state.component_states = delta.component_states;
    
    return new_state;
}

float DeltaCompressor::quantize_position(float value, float precision) {
    return std::round(value / precision) * precision;
}

uint16_t DeltaCompressor::quantize_rotation(float radians, uint16_t bits) {
    const float max_val = 2.0f * 3.14159f;
    float normalized = radians / max_val;
    uint16_t quantized = static_cast<uint16_t>(normalized * ((1 << bits) - 1));
    return quantized;
}

uint8_t DeltaCompressor::pack_flags(const std::vector<bool>& flags) {
    uint8_t packed = 0;
    for (size_t i = 0; i < std::min(flags.size(), size_t(8)); ++i) {
        if (flags[i]) {
            packed |= (1 << i);
        }
    }
    return packed;
}

// PredictionSystem implementation
void PredictionSystem::predict_next_state(ClientPredictionState& pred_state,
                                        const InputCommand& input,
                                        float delta_time) {
    // Simple prediction based on input
    // In a real implementation, this would use physics simulation
    __m256 velocity = _mm256_set1_ps(10.0f * delta_time); // Example velocity
    pred_state.predicted_position = _mm256_add_ps(pred_state.predicted_position, velocity);
    pred_state.prediction_error = 0.0f; // Reset error
    pred_state.last_input = input;
}

void PredictionSystem::reconcile_with_server(EntityID entity_id,
                                           const NetworkEntityState& server_state) {
    auto it = prediction_states_.find(entity_id);
    if (it != prediction_states_.end()) {
        ClientPredictionState& pred_state = it->second;
        
        // Calculate prediction error
        __m256 error_vec = _mm256_sub_ps(pred_state.predicted_position, server_state.position);
        // Simple error magnitude calculation (sum of squares)
        float error[8];
        _mm256_storeu_ps(error, _mm256_mul_ps(error_vec, error_vec));
        pred_state.prediction_error = error[0] + error[1] + error[2]; // Simplified
        
        // Update server position
        pred_state.server_position = server_state.position;
        pred_state.last_update_time = server_state.timestamp;
        
        // Adjust prediction if error is too large
        if (pred_state.prediction_error > 1.0f) { // Threshold
            pred_state.predicted_position = server_state.position;
        }
    }
}

__m256 PredictionSystem::interpolate_position(EntityID entity_id, float interpolation_time) {
    auto it = prediction_states_.find(entity_id);
    if (it != prediction_states_.end()) {
        const ClientPredictionState& pred_state = it->second;
        
        // Simple linear interpolation between server and predicted position
        __m256 time_factor = _mm256_set1_ps(interpolation_time);
        __m256 interpolated = _mm256_add_ps(
            _mm256_mul_ps(pred_state.server_position, _mm256_sub_ps(_mm256_set1_ps(1.0f), time_factor)),
            _mm256_mul_ps(pred_state.predicted_position, time_factor)
        );
        
        return interpolated;
    }
    
    return _mm256_setzero_ps(); // Default
}

// MessageProcessor implementation
void MessageProcessor::process_incoming_packet(const std::vector<uint8_t>& packet_data,
                                             ClientID client_id) {
    // Parse packet header and route to appropriate handler
    if (packet_data.size() < 4) return; // Minimum packet size
    
    uint32_t message_type = *reinterpret_cast<const uint32_t*>(packet_data.data());
    
    // Route based on message type
    if (message_type < message_handlers_.size()) {
        message_handlers_[message_type](packet_data, client_id);
    }
}

void MessageProcessor::queue_incoming_packet(const std::vector<uint8_t>& packet_data, ClientID client_id) {
    incoming_queue_.emplace(packet_data, client_id);
}

bool MessageProcessor::has_incoming_packet() const {
    return !incoming_queue_.empty();
}

std::pair<std::vector<uint8_t>, ClientID> MessageProcessor::pop_next_incoming_packet() {
    auto packet = std::move(incoming_queue_.front());
    incoming_queue_.pop();
    return packet;
}

void MessageProcessor::queue_outgoing_update(const NetworkEntityState& state_update,
                                           const std::vector<ClientID>& target_clients) {
    // Queue update for sending to specific clients
    std::cout << "Queueing outgoing update for " << target_clients.size() << " clients" << std::endl;
}

// StateSnapshotGenerator implementation
std::vector<EntityStateDelta> StateSnapshotGenerator::generate_snapshot_deltas(
    const std::unordered_map<EntityID, NetworkEntityState>& current_states) {
    
    std::vector<EntityStateDelta> deltas;
    
    for (const auto& [entity_id, current_state] : current_states) {
        const auto& baseline_states = compressor_.get_baseline_states();
        auto baseline_it = baseline_states.find(entity_id);
        if (baseline_it != baseline_states.end()) {
            EntityStateDelta delta = compressor_.create_delta(baseline_it->second, current_state);
            if (delta.changed_fields_mask != 0) { // Only include if something changed
                deltas.push_back(delta);
            }
        }
        // Update baseline
        compressor_.update_baseline_state(entity_id, current_state);
    }
    
    return deltas;
}

// ClientUpdateManager implementation
void ClientUpdateManager::broadcast_state_updates(const std::vector<EntityStateDelta>& deltas) {
    // Broadcast deltas to all connected clients
    std::cout << "Broadcasting " << deltas.size() << " state updates" << std::endl;
}

void ClientUpdateManager::send_reliable_update(ClientID client_id, const EntityStateDelta& delta) {
    // Send reliable update to specific client
    client_queues_[client_id].push_back(delta);
    std::cout << "Sending reliable update to client " << client_id << std::endl;
}

// NetworkMetrics implementation
void NetworkMetrics::record_packet_sent(size_t bytes) {
    total_packets_sent++;
    total_bytes_sent += bytes;
}

void NetworkMetrics::record_packet_received(size_t bytes) {
    total_packets_received++;
    total_bytes_received += bytes;
}

void NetworkMetrics::update_latency(float latency_ms) {
    // Simple moving average
    average_latency_ms = (average_latency_ms + latency_ms) * 0.5f;
}

// GameServer implementation
GameServer::GameServer(uint16_t port, uint32_t max_clients)
    : server_port_(port), max_clients_(max_clients), running_(false) {
    // Initialize network infrastructure
    // In a real implementation, this would set up ASIO contexts and listeners
}

GameServer::~GameServer() {
    stop();
}

void GameServer::start() {
    if (running_) return;
    
    running_ = true;
    
    // Start network threads
    for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
        io_contexts_.push_back(std::make_unique<asio::io_context>());
        network_threads_.emplace_back([this, i]() {
            // Run IO context
            try {
                io_contexts_[i]->run();
            } catch (const std::exception& e) {
                std::cerr << "Network thread " << i << " error: " << e.what() << std::endl;
            }
        });
    }
    
    std::cout << "Game server started on port " << server_port_ << std::endl;
}

void GameServer::run() {
    if (!running_) return;
    
    auto start_time = std::chrono::steady_clock::now();
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - start_time).count();
        start_time = now;
        
        update(dt);
        
        // Sleep to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz
    }
}

void GameServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Stop IO contexts
    for (auto& context : io_contexts_) {
        context->stop();
    }
    
    // Join threads
    for (auto& thread : network_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    network_threads_.clear();
    io_contexts_.clear();
    
    std::cout << "Game server stopped" << std::endl;
}

void GameServer::update(float delta_time) {
    if (!running_) return;
    
    static constexpr float FIXED_DT = 1.0f / 60.0f; // 60 Hz
    static float accumulator = 0.0f;
    
    accumulator += delta_time;
    
    while (accumulator >= FIXED_DT) {
        // Process incoming messages from queue
        while (has_incoming_packet()) {
            auto [packet_data, client_id] = pop_next_incoming_packet();
            
            // Deserialize InputCommand
            if (packet_data.size() >= sizeof(uint32_t) + sizeof(InputCommand)) {
                size_t offset = sizeof(uint32_t); // Skip message_type
                InputCommand cmd;
                std::memcpy(&cmd.client_id, packet_data.data() + offset, sizeof(cmd.client_id)); offset += sizeof(cmd.client_id);
                std::memcpy(&cmd.entity_id, packet_data.data() + offset, sizeof(cmd.entity_id)); offset += sizeof(cmd.entity_id);
                std::memcpy(&cmd.sequence_number, packet_data.data() + offset, sizeof(cmd.sequence_number)); offset += sizeof(cmd.sequence_number);
                std::memcpy(&cmd.command_type, packet_data.data() + offset, sizeof(cmd.command_type)); offset += sizeof(cmd.command_type);
                float input_vec[8];
                std::memcpy(input_vec, packet_data.data() + offset, sizeof(input_vec)); offset += sizeof(input_vec);
                cmd.input_vector = _mm256_loadu_ps(input_vec);
                std::memcpy(&cmd.timestamp, packet_data.data() + offset, sizeof(cmd.timestamp));
                
                // Apply command to entity (simple movement for demo)
                auto it = authoritative_state_.find(cmd.entity_id);
                if (it != authoritative_state_.end()) {
                    NetworkEntityState& state = it->second;
                    // Assume command_type 1 is move, input_vector has dx,dy,dz
                    float move[8];
                    _mm256_storeu_ps(move, cmd.input_vector);
                    __m256 new_pos = _mm256_add_ps(state.position, _mm256_set_ps(0,0,0,0, move[2]*FIXED_DT, move[1]*FIXED_DT, move[0]*FIXED_DT, 0));
                    state.position = new_pos;
                    state.state_version++;
                    state.timestamp += FIXED_DT;
                }
            }
        }
        
        // Generate and broadcast state deltas
        auto deltas = snapshot_generator_.generate_snapshot_deltas(authoritative_state_);
        update_manager_.broadcast_state_updates(deltas);
        
        accumulator -= FIXED_DT;
    }
    
    // Update metrics
    metrics_.active_connections = active_client_count_.load();
}

void GameServer::queue_incoming_packet(const std::vector<uint8_t>& packet_data, ClientID client_id) {
    message_processor_.queue_incoming_packet(packet_data, client_id);
}

bool GameServer::has_incoming_packet() const {
    return message_processor_.has_incoming_packet();
}

std::pair<std::vector<uint8_t>, ClientID> GameServer::pop_next_incoming_packet() {
    return message_processor_.pop_next_incoming_packet();
}

bool GameServer::add_client(ClientID client_id, const std::string& address) {
    if (active_client_count_.load() >= max_clients_) {
        return false;
    }
    
    ClientSession session;
    session.client_id = client_id;
    session.client_address = address;
    session.last_sequence_received = 0;
    session.last_sequence_acknowledged = 0;
    session.connection_quality = 1.0f;
    session.last_update_time = std::chrono::steady_clock::now();
    
    connected_clients_[client_id] = session;
    active_client_count_++;
    
    std::cout << "Client " << client_id << " connected from " << address << std::endl;
    return true;
}

void GameServer::remove_client(ClientID client_id) {
    if (connected_clients_.erase(client_id) > 0) {
        active_client_count_--;
        std::cout << "Client " << client_id << " disconnected" << std::endl;
    }
}

void GameServer::update_entity_state(EntityID entity_id, const NetworkEntityState& state) {
    authoritative_state_[entity_id] = state;
}

void GameServer::broadcast_entity_update(EntityID entity_id) {
    auto it = authoritative_state_.find(entity_id);
    if (it != authoritative_state_.end()) {
        // Broadcast update to all clients
        std::vector<ClientID> all_clients;
        for (const auto& client : connected_clients_) {
            all_clients.push_back(client.first);
        }
        message_processor_.queue_outgoing_update(it->second, all_clients);
    }
}