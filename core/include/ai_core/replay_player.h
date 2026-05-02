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
#include "ai_core/deterministic_rng.h"

/**
 * Replay playback system
 * 
 * Loads and plays back recorded replay files, reproducing
 * exact gameplay sequences for testing and debugging.
 */
class ReplayPlayer {
public:
    /**
     * Configuration for replay playback
     */
    struct Config {
        bool enable_checksum_verification = true;  // Verify sync every frame
        uint32_t checksum_verification_interval = 60;  // ~1 sec at 60 Hz
        bool pause_on_desync = true;               // Auto-pause if desync detected
    };

    /**
     * Constructor
     * @param cfg Playback configuration
     */
    explicit ReplayPlayer(const Config& cfg);
    explicit ReplayPlayer();

    /**
     * Destructor
     */
    ~ReplayPlayer();

    // Delete copy/move
    ReplayPlayer(const ReplayPlayer&) = delete;
    ReplayPlayer& operator=(const ReplayPlayer&) = delete;

    /**
     * Load a replay file
     * @param filename Path to replay file
     * @return true if successful
     */
    bool LoadReplay(const std::string& filename);

    /**
     * Unload current replay
     */
    void UnloadReplay();

    /**
     * Start playback from beginning
     */
    void Play();

    /**
     * Pause playback
     */
    void Pause();

    /**
     * Advance to next frame
     */
    void StepFrame();

    /**
     * Jump to specific frame
     * @param frame_number Target frame (clamped to valid range)
     */
    void JumpToFrame(uint32_t frame_number);

    /**
     * Set playback speed
     * @param speed 1.0 = normal, 0.5 = half speed, 2.0 = double speed
     */
    void SetPlaybackSpeed(float speed) noexcept {
        playback_speed_ = speed;
    }

    /**
     * Get current replay state
     */
    [[nodiscard]] const ReplayControlState& GetState() const noexcept {
        return state_;
    }

    /**
     * Get current frame data
     * @return Ptr to current frame, nullptr if not loaded or at end
     */
    [[nodiscard]] const ReplayFrame* GetCurrentFrame() const noexcept {
        if (!is_loaded_ || state_.current_frame >= frames_.size()) {
            return nullptr;
        }
        return &frames_[state_.current_frame];
    }

    /**
     * Get frame by index
     * @param index Frame index
     * @return Ptr to frame, nullptr if out of range
     */
    [[nodiscard]] const ReplayFrame* GetFrame(uint32_t index) const noexcept {
        if (index >= frames_.size()) {
            return nullptr;
        }
        return &frames_[index];
    }

    /**
     * Get replay header
     */
    [[nodiscard]] const ReplayHeader& GetHeader() const noexcept {
        return header_;
    }

    /**
     * Get RNG for playback
     */
    [[nodiscard]] DeterministicRNG& GetRNG() noexcept {
        return rng_;
    }

    /**
     * Get the expected checksum for a frame (if available)
     * @param frame_index Frame index
     * @return Expected checksum, 0 if not available
     */
    [[nodiscard]] uint64_t GetExpectedChecksum(uint32_t frame_index) const noexcept {
        if (frame_index >= frames_.size()) {
            return 0;
        }
        return frames_[frame_index].world_state_checksum;
    }

    /**
     * Report detected desync
     * @param frame_index Frame where desync was detected
     * @param expected Expected checksum
     * @param actual Actual checksum
     */
    void ReportDesync(uint32_t frame_index, uint64_t expected, uint64_t actual);

    /**
     * Check if replay is loaded
     */
    [[nodiscard]] bool IsLoaded() const noexcept {
        return is_loaded_;
    }

    /**
     * Check if there's a detected desync
     */
    [[nodiscard]] bool HasDesync() const noexcept {
        return state_.has_desync;
    }

private:
    // Deserialize frame from binary buffer
    static ReplayFrame DeserializeFrame(std::span<const uint8_t> buffer, size_t& read_pos);

    // Advance frame counter with speed scaling
    void UpdateFrame();

    Config config_;
    bool is_loaded_ = false;
    ReplayControlState state_;
    ReplayHeader header_;
    std::vector<ReplayFrame> frames_;
    DeterministicRNG rng_;
    float playback_speed_ = 1.0f;
    float frame_accumulator_ = 0.0f;  // For fractional frame advancement
};
