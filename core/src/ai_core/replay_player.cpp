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
#include "ai_core/replay_player.h"
#include <fstream>
#include <iostream>
#include <iterator>

ReplayPlayer::ReplayPlayer(const Config& cfg)
    : config_(cfg), is_loaded_(false), rng_(0) {
    
    state_.mode = ReplayMode::Idle;
    state_.is_paused = false;
    state_.current_frame = 0;
    state_.total_frames = 0;
    state_.playback_speed = 1.0f;
    state_.has_desync = false;
    state_.desync_frame = 0;
}

ReplayPlayer::ReplayPlayer()
    : ReplayPlayer(Config()) {
}

ReplayPlayer::~ReplayPlayer() {
    UnloadReplay();
}

bool ReplayPlayer::LoadReplay(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Replay: Failed to open file: " << filename << "\n";
        return false;
    }

    // Read header
    file.read(reinterpret_cast<char*>(&header_), ReplayHeader::SERIALIZED_SIZE);
    if (!file.good()) {
        std::cerr << "Replay: Failed to read header\n";
        return false;
    }

    // Validate magic number
    if (header_.magic != REPLAY_MAGIC) {
        std::cerr << "Replay: Invalid magic number\n";
        return false;
    }

    // Validate format version
    if (header_.format_version != REPLAY_FORMAT_VERSION) {
        std::cerr << "Replay: Format version mismatch. Expected " << REPLAY_FORMAT_VERSION
                  << ", got " << header_.format_version << "\n";
    }

    std::cout << "Replay: Loaded header - " << header_.frame_count
              << " frames, seed " << header_.seed << "\n";

    // Set RNG seed from replay
    rng_.set_base_seed(header_.seed);

    // Read the remainder of the file in one pass to avoid partial-frame boundary issues.
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    file.close();

    frames_.clear();
    size_t pos = 0;
    while (pos < data.size()) {
        size_t next_pos = pos;
        ReplayFrame frame = DeserializeFrame(
            std::span<const uint8_t>(data.data() + pos, data.size() - pos),
            next_pos
        );

        if (next_pos == pos || next_pos > data.size()) {
            break;
        }

        frames_.push_back(frame);
        pos = next_pos;
    }

    state_.total_frames = static_cast<uint32_t>(frames_.size());
    state_.current_frame = 0;
    state_.has_desync = false;
    state_.desync_frame = 0;

    is_loaded_ = true;
    std::cout << "Replay: Loaded " << frames_.size() << " frames from " << filename << "\n";

    return true;
}

void ReplayPlayer::UnloadReplay() {
    frames_.clear();
    is_loaded_ = false;
    state_.mode = ReplayMode::Idle;
    state_.current_frame = 0;
    state_.total_frames = 0;
}

void ReplayPlayer::Play() {
    if (!is_loaded_) {
        std::cerr << "Replay: No replay loaded\n";
        return;
    }

    state_.mode = ReplayMode::Playing;
    state_.is_paused = false;
    std::cout << "Replay: Starting playback\n";
}

void ReplayPlayer::Pause() {
    if (state_.mode == ReplayMode::Playing) {
        state_.mode = ReplayMode::Idle;
        state_.is_paused = true;
        std::cout << "Replay: Paused at frame " << state_.current_frame << "\n";
    }
}

void ReplayPlayer::StepFrame() {
    if (!is_loaded_ || state_.total_frames == 0) {
        return;
    }

    if (state_.current_frame + 1 < state_.total_frames) {
        ++state_.current_frame;
    } else {
        state_.current_frame = state_.total_frames - 1;
        state_.is_paused = true;
    }

    rng_.set_frame(state_.current_frame);
}

void ReplayPlayer::UpdateFrame() {
    if (!is_loaded_ || state_.mode != ReplayMode::Playing || state_.is_paused || state_.total_frames == 0) {
        return;
    }

    if (state_.current_frame + 1 < state_.total_frames) {
        ++state_.current_frame;
        rng_.set_frame(state_.current_frame);
    } else {
        state_.current_frame = state_.total_frames - 1;
        state_.is_paused = true;
    }
}

void ReplayPlayer::JumpToFrame(uint32_t frame_number) {
    if (!is_loaded_) {
        return;
    }

    state_.current_frame = std::min(frame_number, state_.total_frames - 1);
    rng_.set_frame(state_.current_frame);
    std::cout << "Replay: Jumped to frame " << state_.current_frame << "\n";
}

void ReplayPlayer::ReportDesync(uint32_t frame_index, uint64_t expected, uint64_t actual) {
    state_.has_desync = true;
    state_.desync_frame = frame_index;

    std::cerr << "Replay: Desync at frame " << frame_index
              << "! Expected checksum 0x" << std::hex << expected
              << ", got 0x" << actual << std::dec << "\n";

    if (config_.pause_on_desync) {
        Pause();
    }
}

ReplayFrame ReplayPlayer::DeserializeFrame(std::span<const uint8_t> buffer, size_t& read_pos) {
    ReplayFrame frame;

    // Attempt to deserialize from buffer starting at read_pos
    size_t pos = read_pos;
    size_t bytes_read = frame.Deserialize(buffer);

    if (bytes_read > 0) {
        read_pos += bytes_read;
        return frame;
    }

    return frame;
}
