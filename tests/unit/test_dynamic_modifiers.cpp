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
/**
 * Unit Tests for Phase 2: Dynamic Modifiers System
 * 
 * Tests cover:
 * - Weather effects on movement speed, perception, stress
 * - Terrain type impact on logistics
 * - Supply calculations and thresholds
 * - Neural network encoding
 * - Cache efficiency
 * 
 * Note: Uses static assertions and compile-time checks where possible
 * for zero-runtime testing overhead in production builds.
 */

#include "../../../core/include/ai_core/dynamic_modifiers.h"
#include <cassert>
#include <iostream>
#include <cmath>

// ============================================================================
// Test Utilities
// ============================================================================

static constexpr float EPSILON = 0.001f;

/**
 * Assert float equality with epsilon tolerance
 */
void AssertFloatEqual(float a, float b, const char* test_name) {
    if (std::abs(a - b) > EPSILON) {
        std::cerr << "FAIL: " << test_name << " | Expected ~" << b 
                  << ", got " << a << std::endl;
        std::exit(1);
    }
}

/**
 * Assert condition
 */
void AssertTrue(bool condition, const char* test_name) {
    if (!condition) {
        std::cerr << "FAIL: " << test_name << std::endl;
        std::exit(1);
    }
}

void AssertFalse(bool condition, const char* test_name) {
    if (condition) {
        std::cerr << "FAIL: " << test_name << std::endl;
        std::exit(1);
    }
}

#define PASS(name) std::cout << "PASS: " << name << std::endl

// ============================================================================
// Test 1: WorldState::FastHash
// ============================================================================

void TestWorldStateHash() {
    WorldState w1;
    w1.precipitation = 0.5f;
    w1.fog_density = 0.3f;
    w1.time_of_day = 14.0f;
    w1.terrain_type = TerrainType::Mud;
    
    uint64_t h1 = w1.FastHash();
    
    // Same weather should give same hash
    WorldState w2 = w1;
    AssertTrue(w1.FastHash() == w2.FastHash(), "WorldState hash consistency");
    
    // Different weather should (likely) give different hash
    w2.precipitation = 0.6f;
    AssertFalse(w1.FastHash() == w2.FastHash(), "WorldState hash uniqueness");
    
    PASS("WorldState::FastHash");
}

// ============================================================================
// Test 2: SupplyStatus::WeightedRatio
// ============================================================================

void TestSupplyWeighting() {
    SupplyStatus supply;
    supply.ammo_ratio = 0.5f;
    supply.fuel_ratio = 0.8f;
    supply.medical_ratio = 0.9f;
    
    SupplyStatus::Weights w_infantry;
    w_infantry.ammo = 0.8f;
    w_infantry.fuel = 0.1f;
    w_infantry.medical = 0.1f;
    
    float ratio = supply.WeightedRatio(w_infantry);
    // 0.5 * 0.8 + 0.8 * 0.1 + 0.9 * 0.1 = 0.4 + 0.08 + 0.09 = 0.57
    AssertFloatEqual(ratio, 0.57f, "SupplyStatus weighted ratio");
    
    // Depletion time estimation
    supply.ammo_depletion_rate = -0.05f;  // 5% per second
    float time_to_empty = supply.TimeToEmptyAmmo();
    // 0.5 / 0.05 = 10 seconds
    AssertFloatEqual(time_to_empty, 10.0f, "SupplyStatus time to empty");
    
    PASS("SupplyStatus::WeightedRatio");
}

// ============================================================================
// Test 3: Modifier Clamping
// ============================================================================

void TestModifierClamping() {
    Modifier m;
    
    // Extreme values should be clamped
    m.speed_mult = 3.0f;           // Beyond max
    m.accuracy_mult = 0.1f;        // Below min
    m.perception_range_mult = 1.0f;
    m.stress_gain_mult = 1.0f;
    
    m.Clamp(0.2f, 2.0f);
    
    AssertFloatEqual(m.speed_mult, 2.0f, "Modifier clamping: speed max");
    AssertFloatEqual(m.accuracy_mult, 0.2f, "Modifier clamping: accuracy min");
    
    PASS("Modifier::Clamp");
}

// ============================================================================
// Test 4: Modifier Composition
// ============================================================================

void TestModifierComposition() {
    Modifier weather;
    weather.speed_mult = 0.85f;      // 15% rain penalty
    weather.stress_gain_mult = 1.2f;
    
    Modifier terrain;
    terrain.speed_mult = 0.7f;       // 30% mud penalty
    
    // Compose: weather *= terrain
    Modifier composed = weather;
    composed *= terrain;
    
    // 0.85 * 0.7 = 0.595
    AssertFloatEqual(composed.speed_mult, 0.595f, "Modifier composition");
    
    // Identity check
    Modifier identity;
    AssertTrue(identity.IsIdentity(), "Modifier identity check");
    
    composed.speed_mult = 1.0001f;  // Very close to 1.0
    AssertFalse(composed.IsIdentity(), "Modifier non-identity");
    
    PASS("Modifier::operator*=");
}

// ============================================================================
// Test 5: DynamicModifierSystem – Clear Weather
// ============================================================================

void TestClearWeather() {
    DynamicModifierSystem system;
    WeatherCoefficients coeffs;  // All defaults
    
    WorldState clear_sky;
    clear_sky.precipitation = 0.0f;
    clear_sky.fog_density = 0.0f;
    clear_sky.wind_speed = 0.0f;
    clear_sky.time_of_day = 12.0f;  // Noon
    clear_sky.terrain_type = TerrainType::Asphalt;
    
    Modifier mod = system.Compute(clear_sky, coeffs, 0);
    
    // Clear weather should result in near-identity modifier
    AssertTrue(std::abs(mod.speed_mult - 1.0f) < 0.01f, "Clear weather speed");
    AssertTrue(std::abs(mod.accuracy_mult - 1.0f) < 0.01f, "Clear weather accuracy");
    AssertTrue(std::abs(mod.perception_range_mult - 1.0f) < 0.01f, "Clear weather perception");
    
    PASS("DynamicModifierSystem: Clear Weather");
}

// ============================================================================
// Test 6: DynamicModifierSystem – Heavy Rain
// ============================================================================

void TestHeavyRain() {
    DynamicModifierSystem system;
    WeatherCoefficients coeffs;
    
    WorldState rain;
    rain.precipitation = 1.0f;      // Heavy rain
    rain.fog_density = 0.2f;
    rain.terrain_type = TerrainType::Grass;
    
    Modifier mod = system.Compute(rain, coeffs, 0);
    
    // Heavy rain: speed_mult -= 1.0 * 0.15 = 0.85
    AssertFloatEqual(mod.speed_mult, 0.85f, "Heavy rain speed penalty");
    
    // Stress: += 1.0 * 0.2 = 1.2
    AssertFloatEqual(mod.stress_gain_mult, 1.2f, "Heavy rain stress gain");
    
    PASS("DynamicModifierSystem: Heavy Rain");
}

// ============================================================================
// Test 7: DynamicModifierSystem – Night + Fog
// ============================================================================

void TestNightAndFog() {
    DynamicModifierSystem system;
    WeatherCoefficients coeffs;
    
    WorldState night;
    night.time_of_day = 22.0f;      // 10 PM
    night.fog_density = 0.8f;
    
    Modifier mod = system.Compute(night, coeffs, 0);
    
    // Fog reduces perception: 1.0 - 0.8 * 0.5 = 0.6
    // Night reduces it more: 0.6 - 0.3 = 0.3, clamped to min 0.2
    AssertTrue(mod.perception_range_mult < 0.5f, "Night+fog perception severely reduced");
    
    PASS("DynamicModifierSystem: Night and Fog");
}

// ============================================================================
// Test 8: DynamicModifierSystem – Mud Terrain
// ============================================================================

void TestMudTerrain() {
    DynamicModifierSystem system;
    WeatherCoefficients coeffs;
    
    WorldState mud;
    mud.terrain_type = TerrainType::Mud;
    mud.precipitation = 0.0f;
    
    Modifier mod = system.Compute(mud, coeffs, 0);
    
    // Mud: speed_mult -= 0.3
    AssertFloatEqual(mod.speed_mult, 0.7f, "Mud speed penalty");
    
    // Mud also increases stress: += 0.1
    AssertFloatEqual(mod.stress_gain_mult, 1.1f, "Mud stress penalty");
    
    PASS("DynamicModifierSystem: Mud Terrain");
}

// ============================================================================
// Test 9: DynamicModifierSystem – Cache Hit Rate
// ============================================================================

void TestDynamicModifierCache() {
    DynamicModifierSystem system;
    WeatherCoefficients coeffs;
    
    WorldState w;
    w.precipitation = 0.5f;
    w.fog_density = 0.3f;
    
    // First computation (cache miss)
    Modifier m1 = system.Compute(w, coeffs, 0);
    
    // Same weather with slightly later frame (still valid in cache)
    Modifier m2 = system.Compute(w, coeffs, 2);
    
    // Should be identical
    AssertFloatEqual(m1.speed_mult, m2.speed_mult, "Cache hit result 1");
    AssertFloatEqual(m1.stress_gain_mult, m2.stress_gain_mult, "Cache hit result 2");
    
    // Cache should show hits
    float hit_rate = system.GetCacheHitRate();
    AssertTrue(hit_rate >= 50.0f, "Cache hit rate >= 50%");
    
    PASS("DynamicModifierSystem: Cache");
}

// ============================================================================
// Test 10: NeuralInputEncoder::Encode
// ============================================================================

void TestNeuralInputEncoding() {
    WorldState world;
    world.precipitation = 0.5f;
    world.fog_density = 0.2f;
    world.time_of_day = 16.0f;
    world.terrain_type = TerrainType::Grass;
    
    SupplyStatus supply;
    supply.ammo_ratio = 0.6f;
    supply.fuel_ratio = 0.8f;
    supply.medical_ratio = 0.9f;
    
    SupplyStatus::Weights weights{0.6f, 0.3f, 0.1f};
    
    NeuralInputEncoder::SquadState squad;
    squad.friendly_count = 10;
    squad.known_enemy_count = 5;
    
    auto encoded = NeuralInputEncoder::Encode(world, supply, weights, squad);
    
    // Verify size
    AssertTrue(encoded.size() == NeuralInputEncoder::Schema::COUNT, 
               "Encoded vector correct size");
    
    // Verify all values are normalized
    for (float v : encoded) {
        AssertTrue(v >= 0.0f && v <= 1.0f, "Encoded value in range");
    }
    
    // Validate
    bool valid = NeuralInputEncoder::Validate(encoded);
    AssertTrue(valid, "Encoded vector validates");
    
    PASS("NeuralInputEncoder::Encode");
}

// ============================================================================
// Test 11: NeuralInputEncoder Validation
// ============================================================================

void TestNeuralInputValidation() {
    // Valid input
    std::array<float, NeuralInputEncoder::Schema::COUNT> valid;
    std::fill(valid.begin(), valid.end(), 0.5f);
    AssertTrue(NeuralInputEncoder::Validate(valid), "Valid input passes");
    
    // Invalid: NaN
    std::array<float, NeuralInputEncoder::Schema::COUNT> nan_input = valid;
    nan_input[0] = std::numeric_limits<float>::quiet_NaN();
    AssertFalse(NeuralInputEncoder::Validate(nan_input), "NaN input rejected");
    
    // Invalid: out of range
    std::array<float, NeuralInputEncoder::Schema::COUNT> oob_input = valid;
    oob_input[0] = 2.0f;  // Out of bounds
    AssertFalse(NeuralInputEncoder::Validate(oob_input), "Out-of-bounds input rejected");
    
    PASS("NeuralInputEncoder::Validate");
}

// ============================================================================
// Test 12: Compile-time Checks (Static Assertions)
// ============================================================================

void TestCompileTimeChecks() {
    // These would be compile-time errors if violated:
    static_assert(sizeof(WorldState) <= 64, "WorldState is reasonably sized");
    static_assert(sizeof(SupplyStatus) <= 64, "SupplyStatus is reasonable");
    static_assert(sizeof(Modifier) == 16, "Modifier is 4 floats = 16 bytes");
    static_assert(NeuralInputEncoder::Schema::COUNT == 7, "Neural schema has expected size");
    
    PASS("Compile-time checks");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n========== Phase 2: Dynamic Modifiers Tests ==========\n" << std::endl;
    
    try {
        TestWorldStateHash();
        TestSupplyWeighting();
        TestModifierClamping();
        TestModifierComposition();
        TestClearWeather();
        TestHeavyRain();
        TestNightAndFog();
        TestMudTerrain();
        TestDynamicModifierCache();
        TestNeuralInputEncoding();
        TestNeuralInputValidation();
        TestCompileTimeChecks();
        
        std::cout << "\n========== ALL TESTS PASSED ==========\n" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "\nUnknown exception in tests\n" << std::endl;
        return 1;
    }
}
