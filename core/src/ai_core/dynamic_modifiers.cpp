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
#include "ai_core/dynamic_modifiers.h"
#include <cmath>
#include <algorithm>
#include <numeric>

// ============================================================================
// DynamicModifierSystem Implementation
// ============================================================================

Modifier DynamicModifierSystem::Compute(const WorldState& world,
                                        const WeatherCoefficients& coeffs,
                                        uint64_t frame_id)
{
    ++total_queries_;
    
    // ========== Cache Lookup ==========
    uint64_t hash = world.FastHash();
    
    auto it = cache_.find(hash);
    if (it != cache_.end() && it->second.valid_until_frame >= frame_id) {
        ++cache_hits_;
        return it->second.modifier;
    }
    
    // ========== Compute New Modifier ==========
    Modifier m{1.0f, 1.0f, 1.0f, 1.0f};
    
    // --- Precipitation Effects ---
    m.speed_mult -= world.precipitation * coeffs.rain_speed_penalty;
    m.stress_gain_mult += world.precipitation * coeffs.rain_stress_gain;
    
    // --- Fog Effects ---
    float visibility_factor = 1.0f - (world.fog_density * coeffs.fog_perception_penalty);
    
    // --- Night Effects ---
    if (world.time_of_day < 6.0f || world.time_of_day > 20.0f) {
        visibility_factor -= coeffs.night_perception_penalty;
    }
    
    m.perception_range_mult = std::max(0.2f, visibility_factor);
    
    // --- Terrain Type Effects ---
    switch (world.terrain_type) {
        case TerrainType::Mud:
            m.speed_mult -= coeffs.mud_speed_penalty;
            m.stress_gain_mult += coeffs.mud_stress_gain;
            break;
        case TerrainType::Sand:
            m.speed_mult -= coeffs.sand_speed_penalty;
            break;
        case TerrainType::Snow:
            m.speed_mult -= coeffs.snow_speed_penalty;
            break;
        case TerrainType::Rock:
            m.speed_mult -= coeffs.rock_speed_penalty;
            break;
        case TerrainType::Water:
            if (coeffs.water_impassable > 0.5f) {
                m.speed_mult = 0.0f;  // Impassable
            }
            break;
        case TerrainType::Asphalt:
        case TerrainType::Grass:
        default:
            break;
    }
    
    // --- Wind Effects ---
    m.accuracy_mult -= world.wind_speed * coeffs.wind_accuracy_penalty;
    
    // --- Temperature Effects ---
    if (world.temperature < coeffs.cold_stress_threshold) {
        float cold_delta = coeffs.cold_stress_threshold - world.temperature;
        m.stress_gain_mult += cold_delta * coeffs.cold_stress_penalty;
    }
    
    // --- Safety Clamping ---
    m.Clamp();
    
    // ========== Cache Storage ==========
    CacheEntry entry;
    entry.state_hash = hash;
    entry.modifier = m;
    entry.valid_until_frame = frame_id + 4;  // Valid for ~67ms at 60 FPS
    
    // Evict if cache is too large
    if (cache_.size() >= MaxCacheSize) {
        EvictOldest();
    }
    
    cache_[hash] = entry;
    return m;
}

void DynamicModifierSystem::EvictOldest()
{
    if (cache_.empty()) return;
    
    // Find entry with smallest valid_until_frame (oldest)
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.valid_until_frame < oldest->second.valid_until_frame) {
            oldest = it;
        }
    }
    
    cache_.erase(oldest);
}

// ============================================================================
// NeuralInputEncoder Implementation
// ============================================================================

constexpr std::array<const char*, NeuralInputEncoder::Schema::COUNT>
NeuralInputEncoder::Schema::Names;

std::array<float, NeuralInputEncoder::Schema::COUNT>
NeuralInputEncoder::Encode(const WorldState& world,
                           const SupplyStatus& supply,
                           const SupplyStatus::Weights& supply_weights,
                           const SquadState& squad)
{
    std::array<float, Schema::COUNT> input{};
    
    // --- Weather inputs (0..1) ---
    input[Schema::Rain] = std::clamp(world.precipitation, 0.0f, 1.0f);
    input[Schema::Fog] = std::clamp(world.fog_density, 0.0f, 1.0f);
    
    // --- Time of day (0..1, normalized from 0..24 hours) ---
    input[Schema::TimeOfDay] = std::clamp(world.time_of_day / 24.0f, 0.0f, 1.0f);
    
    // --- Terrain type (0..1, normalized by COUNT) ---
    input[Schema::Terrain] = 
        static_cast<float>(world.terrain_type) / static_cast<float>(TerrainType::COUNT);
    
    // --- Supply ratio (0..1) ---
    input[Schema::SupplyRatio] = std::clamp(
        supply.WeightedRatio(supply_weights), 0.0f, 1.0f);
    
    // --- Unit counts (normalized: assume max 20 friendly, 30 enemy) ---
    input[Schema::FriendlyCount] = 
        std::clamp(squad.friendly_count / 20.0f, 0.0f, 1.0f);
    input[Schema::EnemyCount] = 
        std::clamp(squad.known_enemy_count / 30.0f, 0.0f, 1.0f);
    
    return input;
}

bool NeuralInputEncoder::Validate(std::span<const float> input)
{
    // Check size
    if (input.size() != Schema::COUNT) {
        return false;
    }
    
    // Check each value is within [-1, 1] and not NaN/Inf
    return std::ranges::all_of(input, [](float v) {
        return v >= -1.0f && v <= 1.0f &&  // Range check
               !std::isnan(v) && !std::isinf(v);  // Validity check
    });
}
