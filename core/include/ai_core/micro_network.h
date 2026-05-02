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

#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <numeric>
#include <span>
#include <algorithm>
#include <glm/glm.hpp>
#include "ai_config.h"

using Vector2 = glm::vec2;

// ============================================================================
// Direct Control & Micro-level AI Types
// ============================================================================

enum class Stance : uint8_t {
    Standing = 0,
    Crouching = 1,
    Prone = 2,
    COUNT = 3
};

enum class InputFlags : uint8_t {
    None = 0,
    AimManual = 1 << 0,
    Fire = 1 << 1,
    Reload = 1 << 2,
    Interact = 1 << 3
};

inline InputFlags operator|(InputFlags lhs, InputFlags rhs) noexcept {
    return static_cast<InputFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline InputFlags operator&(InputFlags lhs, InputFlags rhs) noexcept {
    return static_cast<InputFlags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

inline InputFlags& operator|=(InputFlags& lhs, InputFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

inline bool HasFlag(InputFlags value, InputFlags flag) noexcept {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

struct InputState {
    Vector2 move_dir{0.0f, 0.0f};
    uint8_t flags = static_cast<uint8_t>(InputFlags::None);
    Stance stance = Stance::Standing;

    uint32_t GetHash() const noexcept {
        const uint32_t x = std::bit_cast<uint32_t>(move_dir.x);
        const uint32_t y = std::bit_cast<uint32_t>(move_dir.y);
        const uint32_t f = static_cast<uint32_t>(flags);
        const uint32_t s = static_cast<uint32_t>(stance);
        uint32_t hash = 2166136261u;
        auto mix = [&](uint32_t value) noexcept {
            hash ^= value;
            hash *= 16777619u;
        };
        mix(x);
        mix(y);
        mix(f);
        mix(s);
        return hash;
    }
};

struct InputSmoother {
    struct SmoothCache {
        Vector2 prev_move{0.0f, 0.0f};
        Vector2 curr_move{0.0f, 0.0f};
        uint64_t last_update_frame = 0;
    };

    explicit InputSmoother(size_t pool_size = 128u)
        : cache_(pool_size == 0 ? 1u : pool_size) {
    }

    void SmoothBatch(std::span<const InputState> inputs,
                     std::span<Vector2> out_smoothed,
                     float dt,
                     uint64_t frame_id,
                     float smoothing_time) {
        if (inputs.size() != out_smoothed.size()) {
            return;
        }

        const float alpha = (smoothing_time <= 0.0f)
            ? 1.0f
            : std::min(1.0f, dt / smoothing_time);

        for (size_t i = 0; i < inputs.size(); ++i) {
            auto& cache = cache_[i % cache_.size()];
            if (cache.last_update_frame != frame_id) {
                cache.prev_move = cache.curr_move;
                cache.curr_move = inputs[i].move_dir;
                cache.last_update_frame = frame_id;
            }

            out_smoothed[i] = glm::mix(cache.prev_move, cache.curr_move, alpha);
        }
    }

private:
    std::vector<SmoothCache> cache_;
};

struct MicroInput {
    static constexpr size_t COUNT = 8;

    float hp = 1.0f;
    float ammo = 1.0f;
    float fatigue = 0.0f;
    float stress = 0.0f;
    float nearest_threat_dist = 0.0f;
    float cover_dist = 0.0f;
    uint8_t stance = 0;
    uint8_t role = 0;
};

struct MicroOutput {
    std::array<float, 5> action_probabilities{{0.2f, 0.2f, 0.2f, 0.2f, 0.2f}};
    float confidence = 1.0f;
};

class IMicroNetwork {
public:
    virtual ~IMicroNetwork() = default;
    virtual MicroOutput Evaluate(const MicroInput& input) = 0;
};

struct MicroFallback {
    std::array<float, 5> Compute(const MicroInput&) const noexcept {
        return std::array<float, 5>{{0.2f, 0.2f, 0.2f, 0.2f, 0.2f}};
    }
};

class MicroNetworkAdapter {
public:
    explicit MicroNetworkAdapter(std::unique_ptr<IMicroNetwork> network = nullptr)
        : network_(std::move(network)) {
    }

    std::array<float, 5> Evaluate(const MicroInput& raw_input) const {
        static_assert(MicroInput::COUNT == 8, "MicroInput schema mismatch");

        std::array<float, 5> fallback = fallback_.Compute(raw_input);
        if (!network_) {
            return fallback;
        }

        auto output = network_->Evaluate(raw_input);
        if (!ValidateProbabilities(output.action_probabilities)) {
            return fallback;
        }

        if (output.confidence < 0.6f) {
            return fallback;
        }

        return output.action_probabilities;
    }

private:
    static bool ValidateProbabilities(const std::array<float, 5>& probabilities) noexcept {
        const float sum = std::accumulate(probabilities.begin(), probabilities.end(), 0.0f);
        if (std::abs(sum - 1.0f) > 0.01f) {
            return false;
        }
        return std::all_of(probabilities.begin(), probabilities.end(), [](float p) {
            return p >= 0.0f && p <= 1.0f;
        });
    }

    std::unique_ptr<IMicroNetwork> network_;
    MicroFallback fallback_;
};

struct StanceTransition {
    Stance from = Stance::Standing;
    Stance to = Stance::Standing;
    float duration = 0.0f;
};

enum class StanceTransitionState : uint8_t {
    Idle = 0,
    Transitioning = 1,
    Interrupted = 2
};

struct StanceController {
    Stance current = Stance::Standing;
    Stance target = Stance::Standing;
    StanceTransitionState state = StanceTransitionState::Idle;
    float progress = 0.0f;
    float base_duration = 0.0f;

    static bool CanTransition(Stance from,
                              Stance to,
                              bool terrain_stable,
                              float suppression_level) noexcept {
        if (from == to) {
            return false;
        }
        if (to == Stance::Prone && !terrain_stable) {
            return false;
        }
        if (from == Stance::Prone && to == Stance::Standing && suppression_level > 0.6f) {
            return false;
        }
        return true;
    }

    void StartTransition(Stance next, float duration_seconds) noexcept {
        if (current == next) {
            state = StanceTransitionState::Idle;
            progress = 0.0f;
            base_duration = 0.0f;
            target = current;
            return;
        }
        target = next;
        state = StanceTransitionState::Transitioning;
        progress = 0.0f;
        base_duration = duration_seconds;
    }

    void Update(float dt, float fatigue, float fatigue_penalty) noexcept {
        if (state != StanceTransitionState::Transitioning) {
            return;
        }

        const float actual_duration = std::max(0.01f, base_duration * (1.0f + fatigue * fatigue_penalty));
        progress += dt / actual_duration;

        if (progress >= 1.0f) {
            current = target;
            state = StanceTransitionState::Idle;
            progress = 0.0f;
            base_duration = 0.0f;
        }
    }

    float GetSpeedMult(const AIConfig& cfg) const noexcept {
        if (state != StanceTransitionState::Transitioning) {
            return GetStanceSpeedMult(current, cfg);
        }
        const float t = std::clamp(progress, 0.0f, 1.0f);
        return glm::mix(GetStanceSpeedMult(current, cfg), GetStanceSpeedMult(target, cfg), t);
    }

    float GetAccuracyMult(const AIConfig& cfg) const noexcept {
        if (state != StanceTransitionState::Transitioning) {
            return GetStanceAccuracyMult(current, cfg);
        }
        const float t = std::clamp(progress, 0.0f, 1.0f);
        return glm::mix(GetStanceAccuracyMult(current, cfg), GetStanceAccuracyMult(target, cfg), t);
    }

private:
    static float GetStanceSpeedMult(Stance stance, const AIConfig& cfg) noexcept {
        switch (stance) {
            case Stance::Standing: return cfg.stance_speed_standing;
            case Stance::Crouching: return cfg.stance_speed_crouching;
            case Stance::Prone: return cfg.stance_speed_prone;
            default: return 1.0f;
        }
    }

    static float GetStanceAccuracyMult(Stance stance, const AIConfig& cfg) noexcept {
        switch (stance) {
            case Stance::Standing: return cfg.stance_accuracy_standing;
            case Stance::Crouching: return cfg.stance_accuracy_crouching;
            case Stance::Prone: return cfg.stance_accuracy_prone;
            default: return 1.0f;
        }
    }
};
