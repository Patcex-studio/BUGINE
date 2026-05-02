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
#include <string>
#include <array>

// ============================================================================
// AI Configuration & Constants
// ============================================================================

/**
 * Difficulty levels for AI behavior profiles
 */
enum class DifficultyLevel : uint8_t {
    Beginner,
    Rookie,
    Veteran,
    Elite
};

struct FatigueStressConfig {
    float aim_shake_coeff = 0.5f;          // Degrees per fatigue unit
    float stress_aim_shake_coeff = 0.3f;   // Degrees per stress unit
    float reload_penalty = 0.3f;           // Reload speed reduction per fatigue unit
    float min_reload_mult = 0.5f;          // Minimum reload speed multiplier
    float stress_decay_rate = 0.1f;        // Stress reduction per second when safe
    float panic_stress_threshold = 0.8f;   // Stress threshold for panic checks
    float panic_morale_threshold = 0.3f;   // Morale threshold for panic checks
};

struct StanceConfig {
    float fatigue_penalty = 0.4f;
    float speed_standing = 1.0f;
    float speed_crouching = 0.6f;
    float speed_prone = 0.1f;
    float accuracy_standing = 1.0f;
    float accuracy_crouching = 1.2f;
    float accuracy_prone = 1.5f;
    float visibility_standing = 1.0f;
    float visibility_crouching = 0.75f;
    float visibility_prone = 0.5f;
};

/**
 * AIConfig
 * 
 * Configuration for AI behavior, reaction times, and parameters
 * Loaded from config files (TOML/JSON) at startup
 * Applied to units based on difficulty level and unit type
 */
struct AIConfig {
    // ========== Reaction Delay ==========
    // base_reaction_delay * (1.0 + stress * 0.5 - fatigue * 0.3)
    
    float base_reaction_delay_ms = 200.0f;   // Base delay in milliseconds
                                             // Range: 100ms (elite) to 500ms (beginner)
    
    float stress_sensitivity = 0.5f;         // Multiplier for stress impact
    float fatigue_sensitivity = 0.3f;        // Multiplier for fatigue impact
    
    // ========== Control Parameters ==========
    float adaptation_duration_s = 0.3f;      // Smooth return period (AI→player)
    float command_cache_duration_ms = 100.0f; // How long to cache decisions
    
    // ========== Perception ==========
    float perception_delay_ms = 150.0f;      // Delay for updating visible targets
    float perception_stress_mult = 0.4f;     // How stress affects perception delay
    
    // ========== Movement ==========
    float move_speed_multiplier = 1.0f;      // Apply to base unit speed
    float turn_rate_deg_per_s = 45.0f;       // Max rotation speed
    
    // ========== Combat ==========
    float accuracy_base = 0.75f;             // Base hit chance
    float accuracy_stress_mult = -0.15f;     // Stress reduces accuracy

    // ========== Neural Network Integration ==========
    float nn_min_confidence = 0.6f;          // Minimum NN confidence to use NN decisions (0.0..1.0)
    bool nn_enabled = true;                  // Global NN enable flag
    DifficultyLevel difficulty = DifficultyLevel::Rookie;  // Current difficulty profile

    // ========== Fatigue / Stress ==========
    FatigueStressConfig fatigue_stress;

    // ========== Stance Modifiers ==========
    StanceConfig stance;

    enum class Difficulty : uint8_t {
        NORMAL = static_cast<uint8_t>(DifficultyLevel::Rookie),
        HARD = static_cast<uint8_t>(DifficultyLevel::Elite)
    };

    static AIConfig ForDifficulty(DifficultyLevel level) {
        AIConfig cfg;
        
        switch (level) {
            case DifficultyLevel::Beginner:
                cfg.base_reaction_delay_ms = 500.0f;
                cfg.move_speed_multiplier = 0.7f;
                cfg.accuracy_base = 0.5f;
                cfg.nn_min_confidence = 0.8f;  // High threshold for beginners
                cfg.difficulty = DifficultyLevel::Beginner;
                break;
                
            case DifficultyLevel::Rookie:
                cfg.base_reaction_delay_ms = 350.0f;
                cfg.move_speed_multiplier = 0.85f;
                cfg.accuracy_base = 0.6f;
                cfg.nn_min_confidence = 0.7f;
                cfg.difficulty = DifficultyLevel::Rookie;
                break;
                
            case DifficultyLevel::Veteran:
                cfg.base_reaction_delay_ms = 200.0f;
                cfg.move_speed_multiplier = 1.0f;
                cfg.accuracy_base = 0.75f;
                cfg.nn_min_confidence = 0.55f;
                cfg.difficulty = DifficultyLevel::Veteran;
                break;
                
            case DifficultyLevel::Elite:
                cfg.base_reaction_delay_ms = 100.0f;
                cfg.move_speed_multiplier = 1.3f;
                cfg.accuracy_base = 0.95f;
                cfg.nn_min_confidence = 0.5f;
                cfg.difficulty = DifficultyLevel::Elite;
                break;
        }
        
        return cfg;
    }

    static AIConfig ForDifficulty(Difficulty level) {
        return ForDifficulty(level == Difficulty::HARD ? DifficultyLevel::Elite : DifficultyLevel::Rookie);
    }
};

/**
 * Unit AI Parameters
 * Per-unit modifiers that override config defaults
 */
struct UnitAIParams {
    float stress = 0.0f;           // Current stress level (0..1)
    float fatigue = 0.0f;          // Current fatigue (0..1)
    float morale = 1.0f;           // Combat morale multiplier
    uint32_t ai_profile_id = 0;    // Which AI config to use
};

/**
 * Calculated effective delay values
 * Result of applying stress/fatigue to base delay
 */
struct EffectiveDelayParams {
    float reaction_delay_ms = 0.0f;
    float perception_delay_ms = 0.0f;
    
    /**
     * Calculate effective delays based on AI config and unit parameters
     */
    static EffectiveDelayParams Calculate(const AIConfig& config,
                                          const UnitAIParams& unit_params) {
        EffectiveDelayParams result;
        
        // Reaction delay: base_delay * (1.0 + stress * 0.5 - fatigue * 0.3)
        result.reaction_delay_ms = config.base_reaction_delay_ms *
            (1.0f + unit_params.stress * config.stress_sensitivity -
                    unit_params.fatigue * config.fatigue_sensitivity);
        
        // Clamp to reasonable ranges
        result.reaction_delay_ms = std::max(50.0f, 
            std::min(2000.0f, result.reaction_delay_ms));
        
        // Perception delay similarly affected
        result.perception_delay_ms = config.perception_delay_ms *
            (1.0f + unit_params.stress * config.perception_stress_mult);
        
        result.perception_delay_ms = std::max(50.0f,
            std::min(1000.0f, result.perception_delay_ms));
        
        return result;
    }
};

inline float ComputeAimShake(float fatigue, float stress, const AIConfig& cfg) {
    return fatigue * cfg.fatigue_stress.aim_shake_coeff +
           stress * cfg.fatigue_stress.stress_aim_shake_coeff;
}

inline float ComputeReloadSpeedMult(float fatigue, const AIConfig& cfg) {
    return std::max(cfg.fatigue_stress.min_reload_mult,
                    1.0f - fatigue * cfg.fatigue_stress.reload_penalty);
}

inline bool ShouldPanic(float stress, float morale, const AIConfig& cfg) {
    return stress > cfg.fatigue_stress.panic_stress_threshold &&
           morale < cfg.fatigue_stress.panic_morale_threshold;
}

// ============================================================================
// Frame Budget / Tick Budget
// ============================================================================

/**
 * TickBudget
 * Tracks frame time to allow graceful degradation when overwhelmed
 * 
 * Used to:
 * - Pause AI decisions mid-execution
 * - Continue from saved state next frame
 * - Maintain consistent frame time
 */
struct TickBudget {
    uint64_t frame_start_us = 0;        // Microseconds when frame started
    uint64_t budget_us = 16667;         // Frame budget (60 FPS = 16.667ms)
    uint64_t used_us = 0;               // How much time used so far
    
    float GetRemainingMs() const {
        return static_cast<float>(budget_us - used_us) / 1000.0f;
    }
    
    bool HasTimeLeft(uint64_t needed_us) const {
        return (used_us + needed_us) <= budget_us;
    }
    
    void AccountTime(uint64_t elapsed_us) {
        used_us += elapsed_us;
    }
    
    float GetUsedPercent() const {
        return (used_us * 100.0f) / budget_us;
    }
};

// ============================================================================
// Determinism & Replay Support
// ============================================================================

/**
 * For deterministic replays, frame numbers must be consistent
 * This allows debugging of non-deterministic behavior
 */
struct DeterminismContext {
    uint64_t current_frame = 0;
    uint64_t random_seed = 12345;  // Reset per frame for determinism
    
    // Increment before each frame
    void NewFrame() {
        current_frame++;
        // Optionally reseed randomness
    }
};

// ============================================================================
// Supply & Logistics Configuration (Phase 2)
// ============================================================================

/**
 * SupplyWeights
 * Configurable weights for aggregating ammo/fuel/medical into single ratio
 * Different unit types may prioritize different resources
 */
struct SupplyWeights {
    float ammo = 0.6f;      // Infantry values ammo heavily
    float fuel = 0.3f;      // Logistics/fuel importance
    float medical = 0.1f;   // Medical supplies support
    
    /**
     * Preset configurations for common unit types
     */
    static SupplyWeights Infantry() {
        return {0.8f, 0.1f, 0.1f};  // Ammo-critical
    }
    
    static SupplyWeights Vehicle() {
        return {0.4f, 0.5f, 0.1f};  // Fuel-critical
    }
    
    static SupplyWeights Support() {
        return {0.3f, 0.2f, 0.5f};  // Medical-heavy
    }
};

/**
 * LogisticsConfig
 * Configuration for supply thresholds and phase transitions
 */
struct LogisticsConfig {
    // Phase transition thresholds
    float retreat_threshold = 0.4f;      // Below this: enter PHASE_RETREAT
    float regroup_threshold = 0.6f;      // Above this: return to PHASE_COMBAT
    
    // Hysteresis smoothing (prevents "dithering" between phases)
    float phase_transition_rate = 0.5f;  // How fast to lerp phase ratio (0..1 per second)
    
    // Predictive planning
    bool enable_predictive_resupply = true;  // Plan resupply before running empty
    float resupply_lookahead_seconds = 60.0f; // How far ahead to plan
    
    // Supply weights for this squad (loaded per difficulty/unit type)
    SupplyWeights weights = SupplyWeights::Infantry();
};

/**
 * Extended AIConfig for Phase 2+
 * Includes weather, logistics, and dynamic world state support
 */
struct AIConfigPhase2 {
    // === Phase 1 config (included) ===
    AIConfig phase1;
    
    // === Phase 2: Weather & Environment ===
    WeatherCoefficients weather_coeffs;
    LogisticsConfig logistics;
    
    // === Future phases ===
    // (Terrain awareness, formation system, etc.)
    
    /**
     * Helper: create full config from difficulty
     */
    static AIConfigPhase2 ForDifficulty(DifficultyLevel level) {
        AIConfigPhase2 cfg;
        cfg.phase1 = AIConfig::ForDifficulty(level);
        
        // Adjust logistics per difficulty
        switch (level) {
            case DifficultyLevel::Beginner:
                cfg.logistics.retreat_threshold = 0.5f;  // Retreat earlier
                break;
            case DifficultyLevel::Rookie:
                cfg.logistics.retreat_threshold = 0.45f;
                break;
            case DifficultyLevel::Veteran:
                cfg.logistics.retreat_threshold = 0.35f;
                break;
            case DifficultyLevel::Elite:
                cfg.logistics.retreat_threshold = 0.2f;  // Fight to the end
                break;
            default:
                break;
        }
        
        return cfg;
    }
};
