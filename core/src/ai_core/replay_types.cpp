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
#include "ai_core/replay_types.h"
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>

// ============================================================================
// CompactCommand Encoding/Decoding
// ============================================================================

CompactCommand CompactCommand::Encode(const Command& cmd, const CompactCommand* prev) {
    CompactCommand result;
    result.type = static_cast<uint16_t>(cmd.type);
    result.flags = 0;

    // Simple encoding: always include all parameters for now
    // In production, would delta-encode against prev
    result.flags |= 0x0FFF;  // All parameter bits set

    // Store command type and priority in first bytes
    std::memcpy(result.payload.data(), &cmd.type, sizeof(uint16_t));
    std::memcpy(result.payload.data() + 2, &cmd.priority, sizeof(float));
    std::memcpy(result.payload.data() + 6, &cmd.target_id, sizeof(uint32_t));
    std::memcpy(result.payload.data() + 10, cmd.parameters, sizeof(cmd.parameters[0]) * 4);

    return result;
}

Command CompactCommand::Decode(const CompactCommand* prev) const {
    Command result;

    uint16_t cmd_type = 0;
    std::memcpy(&cmd_type, payload.data(), sizeof(uint16_t));
    result.type = static_cast<CommandType>(cmd_type);

    std::memcpy(&result.priority, payload.data() + 2, sizeof(float));
    std::memcpy(&result.target_id, payload.data() + 6, sizeof(uint32_t));

    // Copy back parameters (check bounds first)
    if (payload.size() >= 10 + sizeof(float) * 4) {
        std::memcpy(result.parameters, payload.data() + 10, sizeof(result.parameters));
    }

    return result;
}

// ============================================================================
// ReplayFrame Serialization
// ============================================================================

size_t ReplayFrame::Serialize(std::span<uint8_t> buffer) const {
    if (buffer.size() < EstimateSize()) {
        return 0;  // Buffer too small
    }

    size_t pos = 0;

    // Frame header
    std::memcpy(buffer.data() + pos, &frame_number, sizeof(frame_number));
    pos += sizeof(frame_number);

    std::memcpy(buffer.data() + pos, &delta_time, sizeof(delta_time));
    pos += sizeof(delta_time);

    // Player inputs count + data
    uint32_t input_count = static_cast<uint32_t>(player_inputs.size());
    std::memcpy(buffer.data() + pos, &input_count, sizeof(input_count));
    pos += sizeof(input_count);

    for (const auto& input : player_inputs) {
        std::memcpy(buffer.data() + pos, &input, sizeof(input));
        pos += sizeof(input);
    }

    // AI decisions count + data
    uint32_t decision_count = static_cast<uint32_t>(ai_decisions.size());
    std::memcpy(buffer.data() + pos, &decision_count, sizeof(decision_count));
    pos += sizeof(decision_count);

    for (const auto& decision : ai_decisions) {
        std::memcpy(buffer.data() + pos, &decision.entity_id, sizeof(decision.entity_id));
        pos += sizeof(decision.entity_id);

        uint8_t source = static_cast<uint8_t>(decision.source);
        std::memcpy(buffer.data() + pos, &source, sizeof(source));
        pos += sizeof(source);

        std::memcpy(buffer.data() + pos, &decision.bt_node_index, sizeof(decision.bt_node_index));
        pos += sizeof(decision.bt_node_index);

        std::memcpy(buffer.data() + pos, &decision.command.type, sizeof(decision.command.type));
        pos += sizeof(decision.command.type);

        std::memcpy(buffer.data() + pos, &decision.command.flags, sizeof(decision.command.flags));
        pos += sizeof(decision.command.flags);

        std::memcpy(buffer.data() + pos, decision.command.payload.data(),
                    decision.command.payload.size());
        pos += decision.command.payload.size();

        std::memcpy(buffer.data() + pos, &decision.confidence, sizeof(decision.confidence));
        pos += sizeof(decision.confidence);
    }

    // World state checksum
    std::memcpy(buffer.data() + pos, &world_state_checksum, sizeof(world_state_checksum));
    pos += sizeof(world_state_checksum);

    return pos;
}

size_t ReplayFrame::Deserialize(std::span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(frame_number) + sizeof(delta_time)) {
        return 0;
    }

    size_t pos = 0;

    // Frame header
    std::memcpy(&frame_number, buffer.data() + pos, sizeof(frame_number));
    pos += sizeof(frame_number);

    std::memcpy(&delta_time, buffer.data() + pos, sizeof(delta_time));
    pos += sizeof(delta_time);

    // Player inputs
    uint32_t input_count = 0;
    if (pos + sizeof(input_count) > buffer.size()) return 0;
    std::memcpy(&input_count, buffer.data() + pos, sizeof(input_count));
    pos += sizeof(input_count);

    player_inputs.clear();
    player_inputs.reserve(input_count);
    for (uint32_t i = 0; i < input_count; ++i) {
        if (pos + sizeof(CompactInput) > buffer.size()) return 0;
        player_inputs.emplace_back();
        std::memcpy(&player_inputs.back(), buffer.data() + pos, sizeof(CompactInput));
        pos += sizeof(CompactInput);
    }

    // AI decisions
    uint32_t decision_count = 0;
    if (pos + sizeof(decision_count) > buffer.size()) return 0;
    std::memcpy(&decision_count, buffer.data() + pos, sizeof(decision_count));
    pos += sizeof(decision_count);

    ai_decisions.clear();
    ai_decisions.reserve(decision_count);

    for (uint32_t i = 0; i < decision_count; ++i) {
        if (pos + sizeof(DecisionRecord) > buffer.size()) return 0;

        auto& decision = ai_decisions.emplace_back();

        std::memcpy(&decision.entity_id, buffer.data() + pos, sizeof(decision.entity_id));
        pos += sizeof(decision.entity_id);

        uint8_t source = 0;
        std::memcpy(&source, buffer.data() + pos, sizeof(source));
        decision.source = static_cast<DecisionSource>(source);
        pos += sizeof(source);

        std::memcpy(&decision.bt_node_index, buffer.data() + pos, sizeof(decision.bt_node_index));
        pos += sizeof(decision.bt_node_index);

        std::memcpy(&decision.command.type, buffer.data() + pos, sizeof(decision.command.type));
        pos += sizeof(decision.command.type);

        std::memcpy(&decision.command.flags, buffer.data() + pos, sizeof(decision.command.flags));
        pos += sizeof(decision.command.flags);

        std::memcpy(decision.command.payload.data(), buffer.data() + pos,
                    decision.command.payload.size());
        pos += decision.command.payload.size();

        std::memcpy(&decision.confidence, buffer.data() + pos, sizeof(decision.confidence));
        pos += sizeof(decision.confidence);
    }

    // World state checksum
    if (pos + sizeof(world_state_checksum) > buffer.size()) return 0;
    std::memcpy(&world_state_checksum, buffer.data() + pos, sizeof(world_state_checksum));
    pos += sizeof(world_state_checksum);

    return pos;
}

// ============================================================================
// Utility Functions
// ============================================================================

uint64_t ComputeWorldStateChecksum(const UnitSoA& units) noexcept {
    constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    uint64_t hash = FNV_OFFSET;

    auto hash_bytes = [&](const void* data, size_t bytes) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
        for (size_t i = 0; i < bytes; ++i) {
            hash ^= ptr[i];
            hash *= FNV_PRIME;
        }
    };

    // Hash a scalar entity ID if available.
    hash_bytes(&units.entity_id, sizeof(units.entity_id));

    // Hash all deterministic per-unit arrays present in UnitSoA.
    auto hash_span = [&](auto&& span) {
        using SpanType = std::decay_t<decltype(span)>;
        hash_bytes(span.data(), span.size() * sizeof(typename SpanType::value_type));
    };

    if (!units.positions_x.empty()) hash_span(units.positions_x);
    if (!units.positions_y.empty()) hash_span(units.positions_y);
    if (!units.positions_z.empty()) hash_span(units.positions_z);
    if (!units.velocities_x.empty()) hash_span(units.velocities_x);
    if (!units.velocities_y.empty()) hash_span(units.velocities_y);
    if (!units.velocities_z.empty()) hash_span(units.velocities_z);
    if (!units.health.empty()) hash_span(units.health);
    if (!units.fatigue.empty()) hash_span(units.fatigue);
    if (!units.stress.empty()) hash_span(units.stress);
    if (!units.morale.empty()) hash_span(units.morale);
    if (!units.stance.empty()) hash_span(units.stance);
    if (!units.stance_transition_progress.empty()) hash_span(units.stance_transition_progress);
    if (!units.stance_transition_state.empty()) hash_span(units.stance_transition_state);

    if (!units.perception_states.empty()) {
        for (const auto& state : units.perception_states) {
            hash_bytes(&state, sizeof(state));
        }
    }

    return hash;
}

std::string CreateReplayFilename(uint64_t timestamp, const char* version) {
    std::ostringstream ss;
    ss << "replay_" << timestamp << "_v" << version << ".rpl";
    return ss.str();
}
