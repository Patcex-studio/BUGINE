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
#include <chrono>
#include <algorithm>

// Entity priority levels for adaptive scheduling
enum class EntityPriority : uint8_t {
    High = 0,      // Always execute (60 Hz)
    Medium = 1,    // 30 Hz baseline
    Low = 2        // 20 Hz baseline, degradable
};

/**
 * Adaptive tick budget enforcer with deterministic priorities
 * 
 * Ensures AI processing stays within a fixed time budget (e.g., 1.5 ms)
 * by adaptively degrading update frequency for lower-priority entities
 * during overload conditions.
 */
class TickBudgetEnforcer {
public:
    /**
     * Configuration for priority intervals
     */
    struct PriorityConfig {
        uint32_t high_interval_frames = 1;      // 60 Hz (each frame)
        uint32_t medium_interval_frames = 2;    // 30 Hz (every 2 frames)
        uint32_t low_interval_frames = 3;       // 20 Hz (every 3 frames)
        uint32_t low_interval_max_frames = 12;  // Max degradation: ~5 Hz
    };

    /**
     * Constructor
     * @param ms_budget Time budget in milliseconds
     * @param cfg Priority interval configuration
     */
    explicit TickBudgetEnforcer(float ms_budget, PriorityConfig cfg)
        : budget_ns_(static_cast<uint64_t>(ms_budget * 1e6f)),
          config_(cfg),
          start_ns_(0),
          smoothed_usage_(0.0f),
          consecutive_overruns_(0)
    {
    }

    explicit TickBudgetEnforcer(float ms_budget)
        : TickBudgetEnforcer(ms_budget, PriorityConfig())
    {
    }

    /**
     * Reset the timer for a new tick
     * @param frame_id Current frame ID (for debugging)
     */
    void reset(uint64_t frame_id) {
        start_ns_ = now_ns();
        // EMA: 10% new, 90% old
        smoothed_usage_ = smoothed_usage_ * 0.9f + 0.1f * 0.0f;
    }

    /**
     * Alias for semantic clarity when beginning AI processing.
     */
    void begin_tick() {
        start_ns_ = now_ns();
    }

    /**
     * Check if an entity with given priority should execute this frame
     * 
     * @param priority Entity priority level
     * @param current_frame Current frame ID
     * @param last_tick_frame The last frame this entity was ticked
     * @return true if entity should execute, false to skip
     */
    [[nodiscard]] bool allow(EntityPriority priority, uint64_t current_frame,
                            uint64_t last_tick_frame) const noexcept {
        // High priority always executes
        if (priority == EntityPriority::High) {
            return true;
        }

        // Time budget hard stop
        if ((now_ns() - start_ns_) >= budget_ns_) {
            return false;
        }

        const uint32_t base_interval =
            (priority == EntityPriority::Medium)
                ? config_.medium_interval_frames
                : config_.low_interval_frames;

        uint32_t required_interval = base_interval;

        if (priority == EntityPriority::Low) {
            if (accumulated_overhead_ns_ > budget_ns_ * 3) {
                required_interval = std::min(config_.low_interval_max_frames,
                                             config_.low_interval_frames + consecutive_overruns_);
            }
        }

        if (priority == EntityPriority::Medium && accumulated_overhead_ns_ > budget_ns_ * 5) {
            required_interval = std::max(config_.medium_interval_frames, uint32_t(3));
        }

        if (current_frame - last_tick_frame < required_interval) {
            return false;
        }

        return true;
    }

    /**
     * Record budget overrun for adaptive smoothing.
     */
    void on_budget_exceeded(uint64_t elapsed_ns) {
        if (elapsed_ns > budget_ns_) {
            accumulated_overhead_ns_ += (elapsed_ns - budget_ns_);
        }
    }

    /**
     * Finalize tick and update statistics
     * Should be called at the end of the AI tick
     * 
     * @param frame_id Current frame ID
     */
    void finalize_tick(uint64_t frame_id) {
        const uint64_t elapsed_ns = now_ns() - start_ns_;
        const float usage = static_cast<float>(elapsed_ns) / static_cast<float>(budget_ns_);

        // Update EMA
        smoothed_usage_ = smoothed_usage_ * 0.9f + 0.1f * usage;

        if (usage > 1.0f) {
            ++consecutive_overruns_;
            on_budget_exceeded(elapsed_ns);
        } else {
            if (consecutive_overruns_ > 0) {
                --consecutive_overruns_;
            }
            const uint64_t decay = budget_ns_ / 4;
            accumulated_overhead_ns_ = (accumulated_overhead_ns_ > decay)
                ? (accumulated_overhead_ns_ - decay)
                : 0;
        }
    }

    /**
     * Get smoothed usage percentage (0..1+)
     * @return Smoothed usage ratio
     */
    [[nodiscard]] float get_smoothed_usage() const noexcept {
        return smoothed_usage_;
    }

    /**
     * Get current overrun streak
     * @return Number of consecutive frames over budget
     */
    [[nodiscard]] uint32_t get_overrun_streak() const noexcept {
        return consecutive_overruns_;
    }

    /**
     * Get instantaneous usage percentage this frame
     * @return Current usage ratio
     */
    [[nodiscard]] float get_frame_usage() const noexcept {
        const uint64_t elapsed_ns = now_ns() - start_ns_;
        return static_cast<float>(elapsed_ns) / static_cast<float>(budget_ns_);
    }

    /**
     * Get the current usage percent relative to the budget.
     */
    [[nodiscard]] float usage_percent() const noexcept {
        return get_frame_usage() * 100.0f;
    }

    /**
     * Get budget in nanoseconds
     */
    [[nodiscard]] uint64_t get_budget_ns() const noexcept {
        return budget_ns_;
    }

    /**
     * Current accumulated overhead from recent budget overruns.
     */
    [[nodiscard]] uint64_t get_accumulated_overhead_ns() const noexcept {
        return accumulated_overhead_ns_;
    }

private:
    // Get current time in nanoseconds (platform-independent)
    static uint64_t now_ns() noexcept {
        using Clock = std::chrono::high_resolution_clock;
        auto duration = Clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    uint64_t budget_ns_;            // Time budget in nanoseconds
    uint64_t start_ns_;             // Start time of current tick
    uint64_t accumulated_overhead_ns_ = 0;  // Accumulated overload for adaptive scheduling
    float smoothed_usage_;          // Smoothed usage (0..1+), EMA over 60 frames
    uint32_t consecutive_overruns_; // Streak counter for adaptation
    PriorityConfig config_;
};
