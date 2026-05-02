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
#include <array>
#include <span>
#include "ai_core/decision_engine.h"

// ============================================================================
// Replay Format Constants
// ============================================================================

constexpr uint64_t REPLAY_MAGIC = 0x5245504C41593130ULL;  // "REPLAY10"
constexpr uint32_t REPLAY_FORMAT_VERSION = 1;

// ============================================================================
// Replay Header
// ============================================================================

struct ReplayHeader {
    uint64_t magic;                                // Must be REPLAY_MAGIC
    uint64_t seed;                                 // RNG seed for determinism
    uint64_t start_timestamp;                      // UNIX timestamp
    uint32_t format_version;                       // Current version
    uint32_t entity_count;                         // Number of entities in session
    char game_version[32];                         // Version string for compatibility
    uint32_t frame_count;                          // Total frame count
    float target_delta_time;                       // Expected delta time per frame

    // Serialization helpers
    static constexpr size_t SERIALIZED_SIZE = sizeof(magic) + sizeof(seed) +
                                              sizeof(start_timestamp) + sizeof(format_version) +
                                              sizeof(entity_count) + 32 + sizeof(frame_count) +
                                              sizeof(target_delta_time);
};

// ============================================================================
// Compact Input/Decision Records
// ============================================================================

/**
 * Decision source identifier
 */
enum class DecisionSource : uint8_t {
    NeuralNetwork = 0,
    UtilityAI = 1,
    BehaviorTree = 2,
    SafeDefault = 3,
    PlayerControl = 4
};

/**
 * Player input for a frame (simplified)
 */
struct CompactInput {
    uint16_t keys_pressed = 0;           // Bitmap of pressed keys
    float mouse_x = 0.0f;                // Mouse position X
    float mouse_y = 0.0f;                // Mouse position Y
    float analog_x = 0.0f;               // Analog stick X
    float analog_y = 0.0f;               // Analog stick Y
    uint8_t flags = 0;                   // Additional flags
};

/**
 * Compact command encoding (delte-encoded if previous frame available)
 */
struct CompactCommand {
    uint16_t type = 0;                   // CommandType enum
    uint16_t flags = 0;                  // Bit flags for which parameters are present
    std::array<uint8_t, 16> payload = {}; // Compact parameter storage (0 allocations)

    /**
     * Encode command to compact form
     * @param cmd Command to encode
     * @param prev Previous command for delta encoding (optional)
     * @return Compacted command
     */
    static CompactCommand Encode(const Command& cmd, const CompactCommand* prev = nullptr);

    /**
     * Decode compact command back to full form
     * @param prev Previous command for delta decoding (optional)
     * @return Full command
     */
    [[nodiscard]] Command Decode(const CompactCommand* prev = nullptr) const;

    /**
     * Get serialized size
     */
    [[nodiscard]] size_t size() const noexcept {
        return sizeof(type) + sizeof(flags) + payload.size();
    }
};

/**
 * Decision record for a single entity in a frame
 */
struct DecisionRecord {
    EntityId entity_id = 0;
    DecisionSource source = DecisionSource::SafeDefault;
    uint32_t bt_node_index = 0xFFFFFFFFU;  // 0xFFFFFFFF if not BT
    CompactCommand command;
    float confidence = 0.5f;               // For debugging NN/utility confidence
};

/**
 * Complete data for one frame
 */
struct ReplayFrame {
    uint32_t frame_number = 0;
    float delta_time = 0.016f;  // ~60 Hz

    // Player inputs (one per player)
    std::vector<CompactInput> player_inputs;  // small_vector<4> in practice

    // AI decisions (only for units that made decisions)
    std::vector<DecisionRecord> ai_decisions; // small_vector<64> in practice

    // World state checksum for sync detection
    uint64_t world_state_checksum = 0;

    /**
     * Serialize frame to binary buffer
     * @param buffer Output buffer
     * @return Bytes written
     */
    [[nodiscard]] size_t Serialize(std::span<uint8_t> buffer) const;

    /**
     * Deserialize frame from binary buffer
     * @param buffer Input buffer
     * @return Bytes read, 0 on error
     */
    [[nodiscard]] size_t Deserialize(std::span<const uint8_t> buffer);

    /**
     * Get approximate serialized size
     */
    [[nodiscard]] size_t EstimateSize() const noexcept {
        size_t size = 0;
        size += sizeof(frame_number);
        size += sizeof(delta_time);
        size += sizeof(uint32_t); // player input count
        size += player_inputs.size() * sizeof(CompactInput);
        size += sizeof(uint32_t); // decision count
        for (const auto& decision : ai_decisions) {
            size += sizeof(decision.entity_id);
            size += sizeof(decision.source);
            size += sizeof(decision.bt_node_index);
            size += decision.command.size();
            size += sizeof(decision.confidence);
        }
        size += sizeof(world_state_checksum);
        return size;
    }
};

// ============================================================================
// Replay Modes
// ============================================================================

enum class ReplayMode : uint8_t {
    Idle = 0,       // Not recording or playing
    Recording = 1,  // Recording live gameplay
    Playing = 2     // Playing back recorded gameplay
};

// ============================================================================
// Replay Control State (for UI)
// ============================================================================

struct ReplayControlState {
    ReplayMode mode = ReplayMode::Idle;
    bool is_paused = false;
    uint32_t current_frame = 0;
    uint32_t total_frames = 0;
    float playback_speed = 1.0f;  // 0.5 = half speed, 2.0 = double speed

    // For sync detection
    bool has_desync = false;
    uint32_t desync_frame = 0;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Compute FNV-1a hash of world state for sync detection
 * Should only include deterministic fields
 * 
 * @param units Reference to unit data
 * @return FNV-1a hash
 */
uint64_t ComputeWorldStateChecksum(const UnitSoA& units) noexcept;

/**
 * Create standardized replay filename
 * @param timestamp UNIX timestamp
 * @param version Game version string
 * @return Filename e.g., "replay_1234567890_v1.0.0.rhw"
 */
std::string CreateReplayFilename(uint64_t timestamp, const char* version);
