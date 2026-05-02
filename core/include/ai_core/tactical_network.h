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
#include <span>
#include <array>
#include <optional>
#include <memory>
#include <glm/glm.hpp>
#include <string>

using Vector2 = glm::vec2;

// ============================================================================
// Tactical Network (Neural Network Advisor) – Phase 3
// ============================================================================

/**
 * TacticalNetworkConfig
 * 
 * Configuration for tactical network integration and fallback.
 */
struct TacticalNetworkConfig {
    // Confidence thresholds
    float min_confidence_to_use = 0.55f;    // Only use NN if confident > this
    float min_confidence_for_roles = 0.65f; // Higher bar for role reassignment
    
    // Fallback settings
    bool enable_fallback_rules = true;      // Use rule-based system as fallback
    bool enable_neural_network = false;     // Set to true when NN is loaded
    
    // Heatmap resolution
    int heatmap_width = 64;                 // Grid width in cells
    int heatmap_height = 64;                // Grid height in cells
    int terrain_map_width = 64;
    int terrain_map_height = 64;
};

/**
 * TacticalInputView
 * 
 * Input data for tactical network evaluation.
 * Uses std::span for zero-copy efficiency.
 */
struct TacticalInputView {
    // Heatmaps (flattened 2D arrays)
    std::span<const float> local_heatmap;   // [W*H] - enemy threat density
    std::span<const float> cover_map;       // [W*H] - cover quality in each cell
    
    // Unit data
    std::span<const int> unit_roles;        // [N] - current roles (0=assault, 1=support, 2=sniper)
    std::span<const Vector2> unit_positions_relative;  // [N] - relative to formation center
    
    // Tactical context
    float comm_delay = 0.1f;                // Communication delay in seconds
    float formation_coherence = 1.0f;       // 0..1, how tight formation is
    float suppression_level = 0.0f;         // Overall suppression (0..1)
    
    // Metadata for determinism
    uint64_t frame_id = 0;
    
    // Heatmap metadata
    int heatmap_width = 64;
    int heatmap_height = 64;
    int map_width = 64;
    int map_height = 64;
};

/**
 * TacticalOutputView
 * 
 * Output from tactical network.
 */
struct TacticalOutputView {
    // Formation adjustments
    std::vector<float> formation_shifts;  // [N*2] - X,Y shift for each slot
    
    // Role assignments (optional)
    std::vector<int> role_assignments;    // [N] - new role for each unit
    
    // Confidence in this output
    float confidence = 0.5f;            // 0..1, how sure is the NN?
    float recommendation_strength = 0.5f;  // How different from default
};

/**
 * ITacticalNetwork
 * 
 * Abstract interface for tactical network (neural network advisor).
 * Provides recommendations for formation adjustments and role changes.
 */
class ITacticalNetwork {
public:
    virtual ~ITacticalNetwork() = default;
    
    /**
     * Evaluate tactical situation and return recommendations
     */
    virtual TacticalOutputView Evaluate(const TacticalInputView& input) = 0;
    
    /**
     * Check if network is loaded and ready
     */
    virtual bool IsLoaded() const = 0;
    
    /**
     * Get network name/identifier
     */
    virtual std::string GetName() const { return "UnknownTacticalNetwork"; }
};

// ============================================================================
// Fallback Tactical System
// 
// Rule-based tactical decisions for when NN is unavailable or unconfident.
// ============================================================================

class TacticalFallback {
public:
    struct Config {
        float suppress_threshold = 0.5f;
        float threat_avoidance_range = 30.0f;
        float cover_search_range = 20.0f;
    };
    
    explicit TacticalFallback(const Config& config);
    
    /**
     * Compute tactical adjustments using rule-based system
     */
    TacticalOutputView Compute(
        const TacticalInputView& input
    );
    
private:
    Config config_;
    
    /**
     * Find best cover locations in heatmap
     */
    std::vector<Vector2> FindBestCover(
        std::span<const float> cover_map,
        std::span<const float> threat_map,
        int map_width,
        int map_height,
        size_t num_units
    );
    
    /**
     * Compute defensive formation shift
     */
    Vector2 ComputeDefensiveShift(
        const Vector2& unit_pos,
        const std::vector<Vector2>& cover_positions,
        float threat_direction_x,
        float threat_direction_y
    );
};

// ============================================================================
// TacticalNetworkAdapter
// 
// Integration layer for NN with fallback and validation.
// ============================================================================

class TacticalNetworkAdapter {
public:
    explicit TacticalNetworkAdapter(
        std::unique_ptr<ITacticalNetwork> network,
        const TacticalNetworkConfig& config
    );
    
    /**
     * Evaluate tactical situation with fallback
     * If NN is unavailable or unconfident, uses rule-based fallback.
     */
    TacticalOutputView Evaluate(
        const TacticalInputView& input,
        std::span<float> output_shifts,
        std::span<int> output_roles
    );
    
    /**
     * Load a neural network from file
     */
    bool LoadNetwork(const std::string& model_path);
    
    /**
     * Check if NN is available
     */
    bool HasNetwork() const { return network_ != nullptr; }
    
    /**
     * Check if NN is loaded and ready
     */
    bool IsNetworkReady() const {
        return network_ && network_->IsLoaded();
    }
    
    /**
     * Get diagnostics
     */
    struct Diagnostics {
        int evaluations_nn = 0;
        int evaluations_fallback = 0;
        float average_nn_confidence = 0.0f;
        int fallbacks_due_to_low_confidence = 0;
        int fallbacks_due_to_invalid_output = 0;
    };
    
    Diagnostics GetDiagnostics() const;
    
    /**
     * Reset diagnostics
     */
    void ResetDiagnostics();
    
private:
    std::unique_ptr<ITacticalNetwork> network_;
    std::unique_ptr<TacticalFallback> fallback_;
    TacticalNetworkConfig config_;
    Diagnostics diagnostics_;

    enum class FallbackReason {
        NetworkNotLoaded,
        LowConfidence,
        InvalidOutput
    };

    bool ValidateOutput(const TacticalOutputView& output);
    void LogFallback(uint64_t frame_id, FallbackReason reason);
};

// ============================================================================
// NullTacticalNetwork
// ============================================================================

class NullTacticalNetwork : public ITacticalNetwork {
public:
    NullTacticalNetwork() = default;
    ~NullTacticalNetwork() override = default;
    TacticalOutputView Evaluate(const TacticalInputView& input) override;
    bool IsLoaded() const override { return false; }
};

