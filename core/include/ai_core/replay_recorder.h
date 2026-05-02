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

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include "ai_core/replay_types.h"

/**
 * Asynchronous replay recorder
 * 
 * Records game frames in a deterministic, compressed format.
 * Uses ring-buffered async writes to disk to minimize main-thread impact.
 */
class ReplayRecorder {
public:
    /**
     * Configuration for replay recording
     */
    struct Config {
        size_t ring_buffer_size = 65536;           // Ring buffer size in bytes
        size_t flush_threshold = 32768;            // Flush when buffer > this
        bool enable_compression = true;            // Enable zlib compression
        bool enable_checksums = true;              // Enable world state checksums
        std::string output_directory = "./replays"; // Output directory
    };

    /**
     * Constructor
     * @param seed RNG seed for this replay session
     * @param cfg Recorder configuration
     * @param game_version Version string for compatibility
     */
    explicit ReplayRecorder(uint64_t seed, const Config& cfg, const char* game_version = "1.0.0");
    explicit ReplayRecorder(uint64_t seed, const char* game_version = "1.0.0");

    /**
     * Destructor - flushes any pending data
     */
    ~ReplayRecorder();

    // Delete copy/move
    ReplayRecorder(const ReplayRecorder&) = delete;
    ReplayRecorder& operator=(const ReplayRecorder&) = delete;

    /**
     * Start recording a new session
     * @param filename Output filename (without path)
     * @return true if successful
     */
    bool StartRecording(const std::string& filename);

    /**
     * Stop recording and close the file
     */
    void StopRecording();

    /**
     * Record a single frame
     * @param frame_number Frame ID
     * @param delta_time Frame delta time
     * @param player_inputs Player inputs for this frame
     * @param ai_decisions AI decisions made
     * @param units Unit state for checksum
     */
    void RecordFrame(uint32_t frame_number, float delta_time,
                     std::span<const CompactInput> player_inputs,
                     std::span<const DecisionRecord> ai_decisions,
                     const UnitSoA& units);

    /**
     * Check if currently recording
     */
    [[nodiscard]] bool IsRecording() const noexcept {
        return is_recording_;
    }

    /**
     * Get current frame count recorded
     */
    [[nodiscard]] uint32_t GetFrameCount() const noexcept {
        return frame_count_;
    }

    /**
     * Get RNG seed for this replay
     */
    [[nodiscard]] uint64_t GetSeed() const noexcept {
        return seed_;
    }

    /**
     * Flush all buffered data to disk
     * Safe to call during gameplay (async)
     */
    void Flush();

private:
    // Internal frame buffer for async writing
    struct FrameBuffer {
        std::vector<uint8_t> data;
        size_t write_pos = 0;

        void Reset() { write_pos = 0; }
        bool HasSpace(size_t needed) const { return write_pos + needed <= data.size(); }
        size_t GetUsedSize() const { return write_pos; }
    };

    // Serialize a frame to internal buffer
    size_t SerializeFrame(const ReplayFrame& frame, FrameBuffer& buf);

    // Write buffer to disk (potentially async)
    void WriteToFile(const uint8_t* data, size_t size);

    uint64_t seed_;
    Config config_;
    std::string game_version_;
    std::string current_filename_;

    bool is_recording_ = false;
    uint32_t frame_count_ = 0;

    std::unique_ptr<std::ofstream> file_;
    std::unique_ptr<FrameBuffer> frame_buffer_;
    size_t total_bytes_written_ = 0;
    uint64_t start_timestamp_ = 0;

    // Lambda capture for deferred operations if needed
    bool header_written_ = false;

    void WriteHeader();
};
