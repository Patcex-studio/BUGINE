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
#include <random>
#include <limits>

/**
 * Deterministic RNG for replaying decision sequences
 * 
 * Ensures that with the same frame_seed and frame_id,
 * the same sequence of random numbers is generated.
 * Essential for replay system determinism.
 */
class DeterministicRNG {
public:
    /**
     * Constructor with base seed
     * @param base_seed Initial seed value
     */
    explicit DeterministicRNG(uint64_t base_seed = 12345) 
        : engine_(base_seed), frame_seed_offset_(0) 
    {
    }

    /**
     * Set the current frame for deterministic operation
     * All random numbers after this call will be based on this frame
     * 
     * @param frame_id Current frame ID
     */
    void set_frame(uint64_t frame_id) noexcept {
        if (frame_id != frame_seed_offset_) {
            // Reseed based on frame offset
            const uint64_t new_seed = base_seed_ + frame_id * 73856093ULL; // FNV-style offset
            engine_.seed(new_seed);
            frame_seed_offset_ = frame_id;
        }
    }

    /**
     * Set frame with per-entity offset for deterministic but unique sequences
     * 
     * @param frame_id Current frame ID
     * @param entity_id Entity ID for unique but deterministic offset
     */
    void set_frame_seed(uint64_t frame_id, uint64_t entity_id) noexcept {
        // Combine frame and entity for unique but reproducible sequence
        const uint64_t combined_seed =
            base_seed_ + frame_id * 73856093ULL + entity_id * 19349663ULL;
        engine_.seed(combined_seed);
        frame_seed_offset_ = frame_id;
    }

    /**
     * Get next random float in [0, 1)
     * @return Random float value
     */
    [[nodiscard]] float next_float() noexcept {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(engine_);
    }

    /**
     * Get next random float in [min, max)
     * @param min Minimum value
     * @param max Maximum value
     * @return Random float in range
     */
    [[nodiscard]] float next_float(float min, float max) noexcept {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine_);
    }

    /**
     * Get next random integer in [0, max)
     * @param max Upper bound (exclusive)
     * @return Random integer
     */
    [[nodiscard]] uint32_t next_uint32(uint32_t max = std::numeric_limits<uint32_t>::max()) noexcept {
        if (max == std::numeric_limits<uint32_t>::max()) {
            return static_cast<uint32_t>(engine_());
        }
        std::uniform_int_distribution<uint32_t> dist(0, max - 1);
        return dist(engine_);
    }

    /**
     * Get next random integer in [min, max)
     * @param min Minimum value
     * @param max Maximum value
     * @return Random integer in range
     */
    [[nodiscard]] int32_t next_int32(int32_t min, int32_t max) noexcept {
        std::uniform_int_distribution<int32_t> dist(min, max - 1);
        return dist(engine_);
    }

    /**
     * Save current engine state
     * @return Opaque state value
     */
    [[nodiscard]] uint64_t save_state() const noexcept {
        std::mt19937_64 copy = engine_;
        return copy();
    }

    /**
     * Load engine state
     * @param state Previously saved state
     */
    void load_state(uint64_t state) noexcept {
        engine_.seed(state);
    }

    /**
     * Get current frame offset
     */
    [[nodiscard]] uint64_t get_frame_offset() const noexcept {
        return frame_seed_offset_;
    }

    /**
     * Set base seed (typically loaded from replay file)
     * @param seed Base seed for the session
     */
    void set_base_seed(uint64_t seed) noexcept {
        base_seed_ = seed;
        engine_.seed(seed);
        frame_seed_offset_ = 0;
    }

    /**
     * Get base seed
     */
    [[nodiscard]] uint64_t get_base_seed() const noexcept {
        return base_seed_;
    }

private:
    std::mt19937_64 engine_;        // Mersenne Twister 64-bit RNG
    uint64_t base_seed_;             // Original seed value
    uint64_t frame_seed_offset_;    // Current frame offset
};
