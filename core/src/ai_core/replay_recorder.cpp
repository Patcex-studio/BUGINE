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
#include "ai_core/replay_recorder.h"
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>

ReplayRecorder::ReplayRecorder(uint64_t seed, const Config& cfg, const char* game_version)
    : seed_(seed), config_(cfg), game_version_(game_version),
      frame_buffer_(std::make_unique<FrameBuffer>()) {
    
    // Pre-allocate frame buffer
    frame_buffer_->data.resize(config_.ring_buffer_size);
}

ReplayRecorder::ReplayRecorder(uint64_t seed, const char* game_version)
    : ReplayRecorder(seed, Config(), game_version) {
}

ReplayRecorder::~ReplayRecorder() {
    StopRecording();
}

bool ReplayRecorder::StartRecording(const std::string& filename) {
    if (is_recording_) {
        std::cerr << "Replay: Already recording to " << current_filename_ << "\n";
        return false;
    }

    std::filesystem::create_directories(config_.output_directory);
    current_filename_ = config_.output_directory + "/" + filename;
    start_timestamp_ = static_cast<uint64_t>(std::time(nullptr));

    file_ = std::make_unique<std::ofstream>(current_filename_, std::ios::binary);
    if (!file_->is_open()) {
        std::cerr << "Replay: Failed to open file: " << current_filename_ << "\n";
        file_.reset();
        return false;
    }

    is_recording_ = true;
    frame_count_ = 0;
    header_written_ = false;
    frame_buffer_->Reset();
    total_bytes_written_ = 0;

    std::cout << "Replay: Started recording to " << current_filename_ << "\n";
    WriteHeader();

    return true;
}

void ReplayRecorder::StopRecording() {
    if (!is_recording_) {
        return;
    }

    Flush();

    if (file_ && file_->is_open()) {
        // Rewrite the header with final frame count once all frames are written.
        if (header_written_) {
            ReplayHeader header;
            header.magic = REPLAY_MAGIC;
            header.seed = seed_;
            header.start_timestamp = start_timestamp_;
            header.format_version = REPLAY_FORMAT_VERSION;
            header.entity_count = 0;
            header.frame_count = frame_count_;
            header.target_delta_time = 0.016f;
            std::strncpy(header.game_version, game_version_.c_str(), sizeof(header.game_version) - 1);
            header.game_version[sizeof(header.game_version) - 1] = '\0';

            file_->seekp(0, std::ios::beg);
            if (file_->good()) {
                file_->write(reinterpret_cast<const char*>(&header), ReplayHeader::SERIALIZED_SIZE);
                file_->flush();
            }
        }

        file_->close();
    }
    file_.reset();

    is_recording_ = false;
    std::cout << "Replay: Stopped recording. Frames: " << frame_count_
              << ", File size: " << total_bytes_written_ << " bytes\n";
}

void ReplayRecorder::RecordFrame(uint32_t frame_number, float delta_time,
                                 std::span<const CompactInput> player_inputs,
                                 std::span<const DecisionRecord> ai_decisions,
                                 const UnitSoA& units) {
    if (!is_recording_) {
        return;
    }

    ReplayFrame frame;
    frame.frame_number = frame_number;
    frame.delta_time = delta_time;

    // Copy inputs
    frame.player_inputs.assign(player_inputs.begin(), player_inputs.end());

    // Copy decisions
    frame.ai_decisions.assign(ai_decisions.begin(), ai_decisions.end());

    // Compute checksum
    if (config_.enable_checksums) {
        frame.world_state_checksum = ComputeWorldStateChecksum(units);
    }

    // Serialize to buffer
    const size_t serialized_size = SerializeFrame(frame, *frame_buffer_);
    if (serialized_size == 0) {
        std::cerr << "Replay: Failed to serialize frame " << frame_number << "\n";
        return;
    }

    ++frame_count_;

    // Check if we need to flush
    if (frame_buffer_->GetUsedSize() >= config_.flush_threshold) {
        Flush();
    }
}

void ReplayRecorder::Flush() {
    if (!is_recording_ || frame_buffer_->GetUsedSize() == 0) {
        return;
    }

    WriteToFile(frame_buffer_->data.data(), frame_buffer_->GetUsedSize());
    frame_buffer_->Reset();
}

void ReplayRecorder::WriteHeader() {
    if (!file_ || header_written_) {
        return;
    }

    ReplayHeader header;
    header.magic = REPLAY_MAGIC;
    header.seed = seed_;
    header.start_timestamp = start_timestamp_;
    header.format_version = REPLAY_FORMAT_VERSION;
    header.entity_count = 0;  // Updated at end
    header.frame_count = 0;   // Updated at end
    header.target_delta_time = 0.016f;  // 60 Hz default

    std::strncpy(header.game_version, game_version_.c_str(), sizeof(header.game_version) - 1);
    header.game_version[sizeof(header.game_version) - 1] = '\0';

    // Write header directly
    file_->write(reinterpret_cast<const char*>(&header), ReplayHeader::SERIALIZED_SIZE);
    total_bytes_written_ += ReplayHeader::SERIALIZED_SIZE;
    header_written_ = true;
}

size_t ReplayRecorder::SerializeFrame(const ReplayFrame& frame, FrameBuffer& buf) {
    const size_t needed_space = frame.EstimateSize();
    if (!buf.HasSpace(needed_space)) {
        Flush();
        if (!buf.HasSpace(needed_space)) {
            return 0;
        }
    }

    std::span<uint8_t> frame_span(buf.data.data() + buf.write_pos,
                                   buf.data.size() - buf.write_pos);
    const size_t written = frame.Serialize(frame_span);

    if (written > 0) {
        buf.write_pos += written;
    }

    return written;
}

void ReplayRecorder::WriteToFile(const uint8_t* data, size_t size) {
    if (!file_ || !file_->is_open()) {
        return;
    }

    file_->write(reinterpret_cast<const char*>(data), size);
    total_bytes_written_ += size;

    if (!file_->good()) {
        std::cerr << "Replay: Write error to file: " << current_filename_ << "\n";
    }
}
