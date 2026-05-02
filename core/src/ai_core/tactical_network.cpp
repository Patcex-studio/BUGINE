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
#include "ai_core/tactical_network.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// ============================================================================
// TacticalFallback Implementation
// ============================================================================

TacticalFallback::TacticalFallback(const Config& config)
    : config_(config) {
}

TacticalOutputView TacticalFallback::Compute(
    const TacticalInputView& input) {
    
    TacticalOutputView output;
    output.confidence = 0.5f;  // Rule-based, moderate confidence
    output.recommendation_strength = 0.3f;  // Conservative changes
    
    size_t unit_count = input.unit_roles.size();
    output.formation_shifts.assign(unit_count * 2, 0.0f);
    output.role_assignments.assign(unit_count, 0);
    
    // Find best cover positions
    auto cover_positions = FindBestCover(
        input.cover_map,
        input.local_heatmap,
        input.heatmap_width,
        input.heatmap_height,
        unit_count
    );
    
    // Compute shifts for each unit
    for (size_t i = 0; i < unit_count; ++i) {
        Vector2 unit_pos = input.unit_positions_relative[i];
        Vector2 shift = ComputeDefensiveShift(
            unit_pos,
            cover_positions,
            0.0f,  // No threat direction in fallback
            0.0f
        );
        
        output.formation_shifts[i * 2] = shift.x;
        output.formation_shifts[i * 2 + 1] = shift.y;
        output.role_assignments[i] = input.unit_roles[i];
    }
    
    return output;
}

std::vector<Vector2> TacticalFallback::FindBestCover(
    std::span<const float> cover_map,
    std::span<const float> threat_map,
    int map_width,
    int map_height,
    size_t num_units) {
    
    std::vector<Vector2> cover_positions;
    cover_positions.reserve(num_units);
    
    // Simple: find highest cover positions with low threat
    for (size_t unit_idx = 0; unit_idx < num_units; ++unit_idx) {
        float best_score = -1.0f;
        Vector2 best_pos = Vector2(0, 0);
        
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                int idx = y * map_width + x;
                if (idx >= cover_map.size() || idx >= threat_map.size()) continue;
                
                float cover = cover_map[idx];
                float threat = threat_map[idx];
                
                // Score: cover minus threat penalty
                float score = cover - threat * 2.0f;
                
                if (score > best_score) {
                    best_score = score;
                    best_pos = Vector2(static_cast<float>(x), static_cast<float>(y));
                }
            }
        }
        
        cover_positions.push_back(best_pos);
    }
    
    return cover_positions;
}

Vector2 TacticalFallback::ComputeDefensiveShift(
    const Vector2& unit_pos,
    const std::vector<Vector2>& cover_positions,
    float threat_direction_x,
    float threat_direction_y) {
    
    // Simple: move toward nearest cover
    Vector2 nearest_cover = Vector2(0, 0);
    float min_dist = std::numeric_limits<float>::max();
    
    for (const auto& cover : cover_positions) {
        float dist = glm::distance(unit_pos, cover);
        if (dist < min_dist) {
            min_dist = dist;
            nearest_cover = cover;
        }
    }
    
    // Shift toward cover, but limit magnitude
    Vector2 shift = nearest_cover - unit_pos;
    float shift_magnitude = glm::length(shift);
    if (shift_magnitude > 5.0f) {
        shift = glm::normalize(shift) * 5.0f;
    }
    
    return shift;
}

// ============================================================================
// TacticalNetworkAdapter Implementation
// ============================================================================

TacticalNetworkAdapter::TacticalNetworkAdapter(
    std::unique_ptr<ITacticalNetwork> network,
    const TacticalNetworkConfig& config)
    : network_(std::move(network))
    , config_(config) {
    
    fallback_ = std::make_unique<TacticalFallback>(TacticalFallback::Config{
        .suppress_threshold = 0.5f,
        .threat_avoidance_range = 30.0f,
        .cover_search_range = 20.0f
    });
}

TacticalOutputView TacticalNetworkAdapter::Evaluate(
    const TacticalInputView& input,
    std::span<float> output_shifts,
    std::span<int> output_roles) {
    
    TacticalOutputView output;
    
    if (network_ && config_.enable_neural_network && network_->IsLoaded()) {
        auto nn_output = network_->Evaluate(input);
        
        if (ValidateOutput(nn_output) &&
            nn_output.confidence >= config_.min_confidence_to_use) {
            diagnostics_.evaluations_nn++;
            output = std::move(nn_output);
        } else {
            if (nn_output.confidence < config_.min_confidence_to_use) {
                diagnostics_.fallbacks_due_to_low_confidence++;
            } else {
                diagnostics_.fallbacks_due_to_invalid_output++;
            }
            diagnostics_.evaluations_fallback++;
            output = fallback_->Compute(input);
        }
    } else {
        diagnostics_.evaluations_fallback++;
        output = fallback_->Compute(input);
    }
    
    if (!output_shifts.empty()) {
        size_t copy_count = std::min(output_shifts.size(), output.formation_shifts.size());
        std::copy_n(output.formation_shifts.begin(), copy_count, output_shifts.begin());
        if (copy_count < output_shifts.size()) {
            std::fill(output_shifts.begin() + copy_count, output_shifts.end(), 0.0f);
        }
    }
    if (!output_roles.empty()) {
        size_t copy_count = std::min(output_roles.size(), output.role_assignments.size());
        std::copy_n(output.role_assignments.begin(), copy_count, output_roles.begin());
        if (copy_count < output_roles.size()) {
            std::fill(output_roles.begin() + copy_count, output_roles.end(), 0);
        }
    }
    
    return output;
}

bool TacticalNetworkAdapter::LoadNetwork(const std::string& model_path) {
    network_.reset();
    return false;
}

bool TacticalNetworkAdapter::ValidateOutput(
    const TacticalOutputView& output) {
    
    for (float shift : output.formation_shifts) {
        if (!std::isfinite(shift) || std::abs(shift) > 5.0f) {
            return false;
        }
    }
    
    for (int role : output.role_assignments) {
        if (role < 0 || role > 2) {
            return false;
        }
    }
    
    return std::isfinite(output.confidence) && output.confidence >= 0.0f && output.confidence <= 1.0f;
}

void TacticalNetworkAdapter::LogFallback(uint64_t frame_id, FallbackReason reason) {
    // Placeholder: in real implementation, log to file or console
}

TacticalNetworkAdapter::Diagnostics TacticalNetworkAdapter::GetDiagnostics() const {
    return diagnostics_;
}

void TacticalNetworkAdapter::ResetDiagnostics() {
    diagnostics_ = Diagnostics{};
}

// ============================================================================
// NullTacticalNetwork Implementation
// ============================================================================

TacticalOutputView NullTacticalNetwork::Evaluate(const TacticalInputView& input) {
    TacticalOutputView output;
    output.confidence = 0.0f;
    output.recommendation_strength = 0.0f;
    
    size_t unit_count = input.unit_roles.size();
    output.formation_shifts.assign(unit_count * 2, 0.0f);
    output.role_assignments.assign(unit_count, 0);
    if (!input.unit_roles.empty()) {
        std::copy(input.unit_roles.begin(), input.unit_roles.end(), output.role_assignments.begin());
    }
    
    return output;
}
