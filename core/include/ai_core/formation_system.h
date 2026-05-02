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
#include <unordered_map>
#include <optional>
#include <glm/glm.hpp>
#include "physics_core/terrain_system.h"

using Vector3 = glm::vec3;
using Vector2 = glm::vec2;

// ============================================================================
// Formation System – Phase 3: TerrainAware Formation
// ============================================================================

/**
 * FormationConfig
 * 
 * Data-driven configuration for formation building and adaptation.
 * Loaded from AIConfig, applied to all formations globally.
 */
struct FormationConfig {
    float max_slot_shift = 2.0f;            // Max deviation from nominal position (meters)
    float min_slot_distance = 1.0f;         // Min distance between slots (meters)
    float slope_threshold = 30.0f;          // Slope threshold (degrees) before shifting
    float cover_preference_weight = 0.7f;   // How much to prefer cover (0..1)
    
    // Caching and performance
    uint64_t max_cache_age_frames = 4;      // Max frames before terrain query expires
    float terrain_query_resolution = 1.0f;  // Grid resolution for terrain sampling
};

/**
 * TerrainQueryResult
 * 
 * Result of querying terrain/cover at a position.
 * Includes freshness tracking for caching.
 */
struct TerrainQueryResult {
    bool is_passable = true;            // Can units stand here?
    float cover_quality = 0.0f;         // 0..1, quality of cover/concealment
    float slope_deg = 0.0f;             // Slope in degrees
    uint64_t queried_frame = 0;         // Frame when this was queried
    
    /**
     * Check if this result is still valid (fresh)
     */
    bool IsFresh(uint64_t current_frame, uint64_t max_age = 4) const {
        return current_frame - queried_frame <= max_age;
    }
};

/**
 * FormationSlot
 * 
 * Individual position within a formation, including terrain adaptation.
 */
struct FormationSlot {
    Vector2 offset;                     // Target offset from formation center (local)
    Vector3 world_pos;                  // Computed world position
    float cover_quality = 0.0f;         // Quality of cover at this position (0..1)
    bool is_passable = true;            // Is this position valid?
    float slope_deg = 0.0f;             // Current slope at position
    
    // Previous position for interpolation/smoothness
    Vector3 prev_world_pos;
    
    // Source-to-target for validation
    Vector3 original_nominal_pos;       // Where this slot started
};

/**
 * Formation
 * 
 * Complete formation definition with all slots.
 * Contains nominal layout and adaptive state.
 */
enum class FormationType : uint8_t {
    LINE = 0,       // Single line, front-facing
    COLUMN = 1,     // Single file, one behind another
    WEDGE = 2,      // V-shape (point forward)
    BOX = 3,        // 2x2 or similar box
    CIRCLE = 4,     // Defensive circle
    SKIRMISH = 5    // Loose, spread out
};

struct Formation {
    FormationType type = FormationType::LINE;
    std::vector<FormationSlot> slots;    // All positions in formation
    Vector3 center_pos;                 // Formation center in world space
    float facing_yaw = 0.0f;            // Formation heading in degrees
    
    uint64_t last_update_frame = 0;     // When was this formation last computed?
    float update_interval_s = 0.5f;     // How often to recompute (0.5s typical)
};

// ============================================================================
// FormationManager
// 
// Core system for building, querying, and updating terrain-aware formations.
// ============================================================================

class ITerrainProvider {
public:
    virtual ~ITerrainProvider() = default;
    
    /**
     * Query terrain properties at a world position
     */
    virtual TerrainQueryResult QueryTerrain(const Vector3& world_pos, uint64_t frame_id) = 0;
    
    /**
     * Batch query for performance (optional, can use QueryTerrain in loop)
     */
    virtual std::vector<TerrainQueryResult> QueryBatch(
        std::span<const Vector3> positions,
        uint64_t frame_id
    ) {
        std::vector<TerrainQueryResult> results;
        results.reserve(positions.size());
        for (const auto& pos : positions) {
            results.push_back(QueryTerrain(pos, frame_id));
        }
        return results;
    };
};

class PhysicsTerrainProvider : public ITerrainProvider {
public:
    explicit PhysicsTerrainProvider(physics_core::TerrainSystem* terrain_system) : terrain_system_(terrain_system) {}
    
    TerrainQueryResult QueryTerrain(const Vector3& world_pos, uint64_t frame_id) override {
        TerrainQueryResult result;
        result.queried_frame = frame_id;
        
        // Sample height and surface type
        float height;
        if (terrain_system_->sample_height(world_pos.x, world_pos.y, height)) {
            // Calculate slope (simplified, using neighboring points)
            float h1, h2;
            terrain_system_->sample_height(world_pos.x + 1.0f, world_pos.y, h1);
            terrain_system_->sample_height(world_pos.x, world_pos.y + 1.0f, h2);
            float dx = h1 - height;
            float dy = h2 - height;
            result.slope_deg = glm::degrees(std::atan(std::sqrt(dx*dx + dy*dy)));
        } else {
            result.slope_deg = 0.0f;
        }
        
        uint8_t surface_type = terrain_system_->query_surface_type(world_pos.x, world_pos.y);
        // Assume passable if not water (id 1) or impassable (id 2)
        result.is_passable = (surface_type != 1 && surface_type != 2 && result.slope_deg < 35.0f);
        
        // Cover quality: simplified, based on surface type
        result.cover_quality = (surface_type == 3) ? 0.8f : 0.0f;  // Assume id 3 is cover
        
        return result;
    }
    
private:
    physics_core::TerrainSystem* terrain_system_;
};

class FormationManager {
public:
    explicit FormationManager(ITerrainProvider* terrain_provider, 
                            const FormationConfig& config);
    
    ~FormationManager() = default;
    
    /**
     * Create a formation of given type at a position
     */
    Formation CreateFormation(FormationType type, 
                             const Vector3& center_pos,
                             float facing_yaw,
                             size_t unit_count);
    
    /**
     * Update an existing formation's slot positions based on terrain
     * Returns true if significant update occurred
     */
    bool UpdateFormation(Formation& formation, 
                        uint64_t current_frame,
                        float dt_s);
    
    /**
     * Compute adapted positions for all slots
     * Takes terrain into account (passability, cover, slope)
     */
    std::vector<Vector3> ComputeAdaptedSlots(
        std::span<const FormationSlot> nominal_slots,
        uint64_t frame_id
    );

    /**
     * Batch query terrain results for many positions.
     * Uses cached results when available.
     */
    std::vector<TerrainQueryResult> QueryBatch(
        std::span<const Vector3> positions,
        uint64_t frame_id
    ) const;
    
    /**
     * Get formation configuration
     */
    const FormationConfig& GetConfig() const { return config_; }
    
    /**
     * Clear terrain cache (call when world changes)
     */
    void InvalidateCache() { terrain_cache_.clear(); }
    
private:
    /**
     * Hash world position for cache lookup
     */
    static uint64_t HashPosition(const Vector3& pos);
    
    /**
     * Find nearest passable position within max_shift distance
     */
    Vector3 FindNearestPassable(const Vector3& nominal_pos,
                               float max_shift,
                               uint64_t frame_id);
    
    /**
     * Adapt a single slot position (shift, prefer cover, avoid slopes)
     */
    Vector3 AdaptSlotPosition(const Vector3& nominal_pos,
                             Vector3& out_cover_quality,
                             uint64_t frame_id);

    /**
     * Query terrain with result caching.
     */
    TerrainQueryResult QueryTerrainCached(const Vector3& world_pos, uint64_t frame_id) const;
    
    ITerrainProvider* terrain_provider_;
    FormationConfig config_;
    
    // Cache for terrain queries: hash(position) -> TerrainQueryResult
    mutable std::unordered_map<uint64_t, TerrainQueryResult> terrain_cache_;
    
    // Nominal formations (predefined patterns)
    std::vector<FormationSlot> line_pattern_;
    std::vector<FormationSlot> column_pattern_;
    std::vector<FormationSlot> wedge_pattern_;
    std::vector<FormationSlot> box_pattern_;
    std::vector<FormationSlot> circle_pattern_;
    std::vector<FormationSlot> skirmish_pattern_;
    
    /**
     * Initialize nominal formation patterns
     */
    void InitializePatterns();
    
    /**
     * Get pattern for given formation type
     */
    std::vector<FormationSlot> GetNominalPattern(FormationType type, size_t unit_count) const;
};

// ============================================================================
// Formation Utility Functions
// ============================================================================

/**
 * Create formation patterns for common types
 */
namespace FormationPatterns {
    /**
     * Generate a line formation (all in a row)
     * spacing: distance between units in line
     */
    std::vector<FormationSlot> MakeLine(size_t count, float spacing = 2.0f);
    
    /**
     * Generate a column formation (single file)
     * spacing: distance between units front-to-back
     */
    std::vector<FormationSlot> MakeColumn(size_t count, float spacing = 3.0f);
    
    /**
     * Generate a wedge formation (V-shape)
     * spacing: distance between units
     */
    std::vector<FormationSlot> MakeWedge(size_t count, float spacing = 2.0f);
}
