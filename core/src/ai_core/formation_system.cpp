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
#include "ai_core/formation_system.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <functional>

// ============================================================================
// FormationManager Implementation
// ============================================================================

FormationManager::FormationManager(ITerrainProvider* terrain_provider,
                                   const FormationConfig& config)
    : terrain_provider_(terrain_provider)
    , config_(config) {
    InitializePatterns();
}

Formation FormationManager::CreateFormation(FormationType type,
                                           const Vector3& center_pos,
                                           float facing_yaw,
                                           size_t unit_count) {
    Formation formation;
    formation.type = type;
    formation.center_pos = center_pos;
    formation.facing_yaw = facing_yaw;
    formation.slots = GetNominalPattern(type, unit_count);
    
    // Set original nominal positions for reference
    for (auto& slot : formation.slots) {
        slot.original_nominal_pos = center_pos + Vector3(slot.offset.x, 0.0f, slot.offset.y);
        slot.world_pos = slot.original_nominal_pos;
        slot.prev_world_pos = slot.world_pos;
    }
    
    return formation;
}

bool FormationManager::UpdateFormation(Formation& formation,
                                       uint64_t current_frame,
                                       float dt_s) {
    // Check if update is needed
    float time_since_update = (current_frame - formation.last_update_frame) * 
                             (dt_s / 1.0f);  // Approximate
    
    if (time_since_update < formation.update_interval_s && formation.last_update_frame != 0) {
        return false;  // No update needed yet
    }
    
    // Recompute positions
    auto new_positions = ComputeAdaptedSlots(formation.slots, current_frame);
    
    bool significant_change = false;
    
    // Update slots with interpolation
    for (size_t i = 0; i < formation.slots.size() && i < new_positions.size(); ++i) {
        formation.slots[i].prev_world_pos = formation.slots[i].world_pos;
        formation.slots[i].world_pos = new_positions[i];
        
        // Check for significant change
        float dist = glm::distance(formation.slots[i].prev_world_pos, new_positions[i]);
        if (dist > 0.5f) {
            significant_change = true;
        }
    }
    
    formation.last_update_frame = current_frame;
    return significant_change;
}

std::vector<Vector3> FormationManager::ComputeAdaptedSlots(
    std::span<const FormationSlot> nominal_slots,
    uint64_t frame_id) {
    
    std::vector<Vector3> adapted;
    adapted.reserve(nominal_slots.size());
    
    for (const auto& slot : nominal_slots) {
        Vector3 cover_quality_placeholder;
        Vector3 adapted_pos = AdaptSlotPosition(slot.original_nominal_pos,
                                               cover_quality_placeholder,
                                               frame_id);
        adapted.push_back(adapted_pos);
    }
    
    return adapted;
}

Vector3 FormationManager::FindNearestPassable(const Vector3& nominal_pos,
                                              float max_shift,
                                              uint64_t frame_id) {
    // Start with nominal position
    auto nominal_query = QueryTerrainCached(nominal_pos, frame_id);
    if (nominal_query.is_passable && nominal_query.slope_deg < config_.slope_threshold) {
        return nominal_pos;
    }
    
    // Spiral search outward from nominal position
    const float step = 0.5f;
    const int max_attempts = 20;
    
    for (int dist = 1; dist <= max_attempts; ++dist) {
        float shift_distance = dist * step;
        if (shift_distance > max_shift) break;
        
        // Try 8 directions around nominal
        for (int dir = 0; dir < 8; ++dir) {
            float angle = (dir / 8.0f) * 2.0f * 3.14159f;
            Vector3 candidate = nominal_pos + Vector3(
                std::cos(angle) * shift_distance,
                0.0f,
                std::sin(angle) * shift_distance
            );
            
            auto query = terrain_provider_->QueryTerrain(candidate, frame_id);
            if (query.is_passable && query.slope_deg < config_.slope_threshold) {
                return candidate;
            }
        }
    }
    
    // Fallback: return nominal even if not ideal
    return nominal_pos;
}

Vector3 FormationManager::AdaptSlotPosition(const Vector3& nominal_pos,
                                           Vector3& out_cover_quality,
                                           uint64_t frame_id) {
    // Query terrain at nominal position
    auto query = QueryTerrainCached(nominal_pos, frame_id);
    out_cover_quality = Vector3(query.cover_quality, 0, 0);
    
    // If passable and slope OK, prefer nominal
    if (query.is_passable && query.slope_deg < config_.slope_threshold) {
        return nominal_pos;
    }
    
    // Find nearest passable position
    Vector3 passable = FindNearestPassable(nominal_pos, config_.max_slot_shift, frame_id);
    
    // If we found cover nearby, prefer it
    auto passable_query = terrain_provider_->QueryTerrain(passable, frame_id);
    if (passable_query.cover_quality > query.cover_quality + 0.3f &&
        glm::distance(passable, nominal_pos) < config_.max_slot_shift) {
        out_cover_quality.x = passable_query.cover_quality;
        return passable;
    }
    
    return passable;
}

void FormationManager::InitializePatterns() {
    line_pattern_ = FormationPatterns::MakeLine(1, 2.0f);
    column_pattern_ = FormationPatterns::MakeColumn(1, 3.0f);
    wedge_pattern_ = FormationPatterns::MakeWedge(1, 2.0f);
}

std::vector<FormationSlot> FormationManager::GetNominalPattern(
    FormationType type,
    size_t unit_count) const {
    
    switch (type) {
        case FormationType::LINE:
            return FormationPatterns::MakeLine(unit_count, 2.0f);
        case FormationType::COLUMN:
            return FormationPatterns::MakeColumn(unit_count, 3.0f);
        case FormationType::WEDGE:
            return FormationPatterns::MakeWedge(unit_count, 2.0f);
        default:
            return FormationPatterns::MakeLine(unit_count, 2.0f);
    }
}

uint64_t FormationManager::HashPosition(const Vector3& pos) {
    // FNV-1a hash of quantized position
    // Quantize to 0.5m grid to improve cache hits
    uint32_t qx = static_cast<uint32_t>(pos.x / 0.5f) & 0xFFFF;
    uint32_t qz = static_cast<uint32_t>(pos.z / 0.5f) & 0xFFFF;
    
    uint64_t h = 14695981039346656037ULL;
    h ^= qx; h *= 1099511628211ULL;
    h ^= qz; h *= 1099511628211ULL;
    return h;
}

// ============================================================================
// Formation Patterns
// ============================================================================

namespace FormationPatterns {

std::vector<FormationSlot> MakeLine(size_t count, float spacing) {
    std::vector<FormationSlot> slots;
    slots.reserve(count);
    
    // Center the line
    float line_width = (count - 1) * spacing / 2.0f;
    
    for (size_t i = 0; i < count; ++i) {
        FormationSlot slot;
        slot.offset.x = (static_cast<float>(i) * spacing) - line_width;
        slot.offset.y = 0.0f;
        slot.cover_quality = 0.0f;
        slot.is_passable = true;
        slot.slope_deg = 0.0f;
        slots.push_back(slot);
    }
    
    return slots;
}

std::vector<FormationSlot> MakeColumn(size_t count, float spacing) {
    std::vector<FormationSlot> slots;
    slots.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        FormationSlot slot;
        slot.offset.x = 0.0f;
        slot.offset.y = static_cast<float>(i) * spacing;
        slot.cover_quality = 0.0f;
        slot.is_passable = true;
        slot.slope_deg = 0.0f;
        slots.push_back(slot);
    }
    
    return slots;
}

std::vector<FormationSlot> MakeWedge(size_t count, float spacing) {
    std::vector<FormationSlot> slots;
    slots.reserve(count);
    
    // V-shaped wedge
    size_t half = (count + 1) / 2;
    
    for (size_t i = 0; i < count; ++i) {
        FormationSlot slot;
        
        // Row index
        size_t row = (count - 1 - i) / 2;
        bool right_side = (i % 2 == 0);
        
        slot.offset.x = right_side ? spacing * (row + 1) : -spacing * (row + 1);
        slot.offset.y = row * spacing * 1.5f;
        slot.cover_quality = 0.0f;
        slot.is_passable = true;
        slot.slope_deg = 0.0f;
        slots.push_back(slot);
    }
    
    return slots;
}

}  // namespace FormationPatterns

std::vector<TerrainQueryResult> FormationManager::QueryBatch(
    std::span<const Vector3> positions,
    uint64_t frame_id) const {
    std::vector<TerrainQueryResult> results;
    results.reserve(positions.size());
    for (const auto& pos : positions) {
        results.push_back(QueryTerrainCached(pos, frame_id));
    }
    return results;
}

TerrainQueryResult FormationManager::QueryTerrainCached(
    const Vector3& world_pos,
    uint64_t frame_id) const {
    const uint64_t key = HashPosition(world_pos);
    auto it = terrain_cache_.find(key);
    if (it != terrain_cache_.end() &&
        it->second.IsFresh(frame_id, config_.max_cache_age_frames)) {
        return it->second;
    }

    TerrainQueryResult result = terrain_provider_->QueryTerrain(world_pos, frame_id);
    result.queried_frame = frame_id;
    terrain_cache_[key] = result;
    return result;
}
