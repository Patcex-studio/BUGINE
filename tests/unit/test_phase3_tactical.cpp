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
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ai_core/formation_system.h"
#include "ai_core/suppression_system.h"
#include "ai_core/communication_system.h"
#include "ai_core/tactical_network.h"

// Mock terrain provider for testing
class MockTerrainProvider : public ITerrainProvider {
public:
    TerrainQueryResult QueryTerrain(const Vector3& world_pos, uint64_t frame_id) override {
        TerrainQueryResult result;
        result.is_passable = true;
        result.cover_quality = 0.5f;
        result.slope_deg = 10.0f;
        result.queried_frame = frame_id;
        return result;
    }
};

// ============================================================================
// Formation System Tests
// ============================================================================

TEST_CASE("FormationManager - Basic Formation Creation", "[formation_system]") {
    FormationConfig config;
    MockTerrainProvider terrain;
    FormationManager manager(&terrain, config);
    
    Vector3 center(0, 0, 0);
    Formation formation = manager.CreateFormation(FormationType::LINE, center, 0.0f, 4);
    
    REQUIRE(formation.slots.size() == 4);
    REQUIRE(formation.type == FormationType::LINE);
    REQUIRE(formation.center_pos == center);
}

TEST_CASE("FormationManager - Terrain Adaptation", "[formation_system]") {
    FormationConfig config;
    MockTerrainProvider terrain;
    FormationManager manager(&terrain, config);
    
    Formation formation = manager.CreateFormation(FormationType::LINE, Vector3(0, 0, 0), 0.0f, 2);
    
    // Update formation
    bool changed = manager.UpdateFormation(formation, 1, 0.016f);
    
    // Should have adapted positions
    REQUIRE(formation.slots.size() == 2);
    REQUIRE(formation.last_update_frame == 1);
}

TEST_CASE("FormationPatterns - Line Formation", "[formation_system]") {
    auto slots = FormationPatterns::MakeLine(3, 2.0f);
    
    REQUIRE(slots.size() == 3);
    REQUIRE(slots[0].offset.x == Catch::Approx(-2.0f));
    REQUIRE(slots[1].offset.x == Catch::Approx(0.0f));
    REQUIRE(slots[2].offset.x == Catch::Approx(2.0f));
}

TEST_CASE("FormationPatterns - Column Formation", "[formation_system]") {
    auto slots = FormationPatterns::MakeColumn(3, 3.0f);
    
    REQUIRE(slots.size() == 3);
    REQUIRE(slots[0].offset.y == Catch::Approx(0.0f));
    REQUIRE(slots[1].offset.y == Catch::Approx(3.0f));
    REQUIRE(slots[2].offset.y == Catch::Approx(6.0f));
}

// ============================================================================
// Suppression System Tests
// ============================================================================

TEST_CASE("SuppressionSystem - Zone Creation", "[suppression_system]") {
    SuppressionConfig config;
    SuppressionSystem system(config);
    
    Vector3 center(0, 0, 0);
    system.CreateSuppressionZone(center, 10.0f, 1.0f, 123, 0.0f);
    
    REQUIRE(system.GetActiveZoneCount() == 1);
}

TEST_CASE("SuppressionSystem - Unit Suppression Update", "[suppression_system]") {
    SuppressionConfig config;
    SuppressionSystem system(config);
    
    // Create zone
    system.CreateSuppressionZone(Vector3(0, 0, 0), 10.0f, 1.0f, 123, 0.0f);
    
    // Unit at center
    UnitTacticalState unit;
    Vector3 position(0, 0, 0);
    float cover = 0.0f;
    
    system.UpdateUnitSuppression(unit, position, cover, 0.1f, 0.1f);
    
    REQUIRE(unit.suppression_level > 0.0f);
    REQUIRE(unit.accuracy_mult < 1.0f);
}

TEST_CASE("SuppressionSystem - Accuracy Calculation", "[suppression_system]") {
    SuppressionConfig config;
    UnitTacticalState unit;
    unit.suppression_level = 0.8f;
    
    float accuracy = SuppressionSystem::GetEffectiveAccuracy(unit, config);
    
    REQUIRE(accuracy >= config.min_accuracy_mult);
    REQUIRE(accuracy < 1.0f);
}

TEST_CASE("SuppressionSystem - Behavioral Flags", "[suppression_system]") {
    SuppressionConfig config;
    UnitTacticalState unit;
    unit.suppression_level = 0.9f;
    
    REQUIRE(SuppressionSystem::ShouldBreakFormation(unit, config));
    REQUIRE(SuppressionSystem::RefusesMove(unit, config));
}

// ============================================================================
// Communication System Tests
// ============================================================================

TEST_CASE("CommunicationSystem - Delay Calculation", "[communication_system]") {
    CommunicationConfig config;
    CommunicationSystem system(config);
    
    FormationOrder order;
    order.priority = OrderPriority::TACTICAL;
    
    auto delay = system.ComputeEffectiveDelay(order, 50.0f, 0.5f);
    
    REQUIRE(delay.count() > config.min_delay_ms * 1000);
    REQUIRE(delay.count() < config.max_delay_ms * 1000);
}

TEST_CASE("CommunicationSystem - Emergency Priority", "[communication_system]") {
    CommunicationConfig config;
    CommunicationSystem system(config);
    
    FormationOrder emergency;
    emergency.priority = OrderPriority::EMERGENCY;
    
    FormationOrder tactical;
    tactical.priority = OrderPriority::TACTICAL;
    
    auto emergency_delay = system.ComputeEffectiveDelay(emergency, 50.0f, 0.5f);
    auto tactical_delay = system.ComputeEffectiveDelay(tactical, 50.0f, 0.5f);
    
    REQUIRE(emergency_delay < tactical_delay);
}

TEST_CASE("CommunicationSystem - Order Issuing", "[communication_system]") {
    CommunicationConfig config;
    CommunicationSystem system(config);
    
    std::vector<SubordinateState> subordinates = {
        {1}, {2}, {3}
    };
    
    std::vector<float> params = {1.0f, 2.0f, 3.0f};
    auto current_time = std::chrono::microseconds(1000000);
    
    system.IssueOrder(100, 1, params, OrderPriority::TACTICAL, 
                     10.0f, 0.0f, current_time, subordinates);
    
    REQUIRE(subordinates[0].pending_order.has_value());
    REQUIRE(subordinates[0].pending_order->recipient_id == 1);
}

TEST_CASE("CommunicationSystem - Order Execution", "[communication_system]") {
    CommunicationConfig config;
    CommunicationSystem system(config);
    
    SubordinateState sub{1};
    FormationOrder order;
    order.issue_time = std::chrono::microseconds(0);
    order.effective_delay = std::chrono::microseconds(100000); // 100ms
    
    sub.AcceptOrder(order);
    
    // Check before delay
    auto early_time = std::chrono::microseconds(50000); // 50ms
    REQUIRE(!sub.IsOrderReady(early_time));
    
    // Check after delay
    auto late_time = std::chrono::microseconds(150000); // 150ms
    REQUIRE(sub.IsOrderReady(late_time));
}

// ============================================================================
// Tactical Network Tests
// ============================================================================

TEST_CASE("TacticalFallback - Basic Evaluation", "[tactical_network]") {
    TacticalFallback::Config config;
    TacticalFallback fallback(config);
    
    // Create input
    std::vector<float> heatmap = {0.1f, 0.2f, 0.3f, 0.4f};
    std::vector<float> cover_map = {0.8f, 0.6f, 0.4f, 0.2f};
    std::vector<int> roles = {0, 1};
    std::vector<Vector2> positions = {Vector2(0, 0), Vector2(1, 1)};
    
    TacticalInputView input;
    input.local_heatmap = heatmap;
    input.cover_map = cover_map;
    input.unit_roles = roles;
    input.unit_positions_relative = positions;
    input.heatmap_width = 2;
    input.heatmap_height = 2;
    
    auto output = fallback.Compute(input);
    
    REQUIRE(output.confidence >= 0.0f);
    REQUIRE(output.confidence <= 1.0f);
}

TEST_CASE("NullTacticalNetwork - Zero Output", "[tactical_network]") {
    NullTacticalNetwork network;
    
    TacticalInputView input;
    input.local_heatmap = std::vector<float>{0.1f, 0.2f};
    input.cover_map = std::vector<float>{0.5f, 0.5f};
    input.unit_roles = std::vector<int>{0, 1};
    input.unit_positions_relative = std::vector<Vector2>{Vector2(0, 0), Vector2(1, 1)};
    input.heatmap_width = 1;
    input.heatmap_height = 2;
    
    auto output = network.Evaluate(input);
    
    REQUIRE(output.confidence == 0.0f);
    REQUIRE(!network.IsLoaded());
}

TEST_CASE("TacticalNetworkAdapter - Fallback Behavior", "[tactical_network]") {
    TacticalNetworkConfig config;
    config.enable_neural_network = false; // Force fallback
    
    TacticalNetworkAdapter adapter(nullptr, config);
    
    TacticalInputView input;
    input.local_heatmap = std::vector<float>{0.1f, 0.2f};
    input.cover_map = std::vector<float>{0.5f, 0.5f};
    input.unit_roles = std::vector<int>{0, 1};
    input.unit_positions_relative = std::vector<Vector2>{Vector2(0, 0), Vector2(1, 1)};
    input.heatmap_width = 1;
    input.heatmap_height = 2;
    
    std::vector<float> shifts(4, 0.0f);
    std::vector<int> roles(2, 0);
    
    auto output = adapter.Evaluate(input, shifts, roles);
    
    REQUIRE(output.confidence >= 0.0f);
    REQUIRE(output.confidence <= 1.0f);
    
    auto diag = adapter.GetDiagnostics();
    REQUIRE(diag.evaluations_fallback > 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Phase 3 Integration - Formation + Suppression", "[integration]") {
    // Create formation
    FormationConfig form_config;
    MockTerrainProvider terrain;
    FormationManager formation_mgr(&terrain, form_config);
    
    Formation formation = formation_mgr.CreateFormation(FormationType::LINE, 
                                                        Vector3(0, 0, 0), 0.0f, 3);
    
    // Create suppression
    SuppressionConfig supp_config;
    SuppressionSystem suppression_sys(supp_config);
    
    suppression_sys.CreateSuppressionZone(Vector3(0, 0, 0), 5.0f, 0.8f, 123, 0.0f);
    
    // Update units
    std::vector<UnitTacticalState> units(3);
    std::vector<Vector3> positions = {
        Vector3(-2, 0, 0),
        Vector3(0, 0, 0),
        Vector3(2, 0, 0)
    };
    std::vector<float> cover = {0.0f, 0.5f, 0.0f};
    
    suppression_sys.UpdateSuppressionBatch(units, positions, cover, 0.1f, 0.1f);
    
    // Check that units under fire are suppressed
    REQUIRE(units[0].suppression_level > 0.0f); // No cover
    REQUIRE(units[1].suppression_level < units[0].suppression_level); // Has cover
    REQUIRE(units[2].suppression_level > 0.0f); // No cover
}

TEST_CASE("Phase 3 Integration - Communication + Formation", "[integration]") {
    CommunicationConfig comm_config;
    CommunicationSystem comm_sys(comm_config);
    
    // Create subordinates
    std::vector<SubordinateState> subordinates = {
        {1}, {2}, {3}
    };
    
    auto current_time = std::chrono::microseconds(0);
    
    // Issue formation order
    std::vector<float> params = {10.0f, 0.0f, 0.0f}; // Move to (10,0,0)
    comm_sys.IssueOrder(100, 0, params, OrderPriority::TACTICAL, 
                       20.0f, 0.2f, current_time, subordinates);
    
    // Check orders were issued
    for (const auto& sub : subordinates) {
        REQUIRE(sub.pending_order.has_value());
        REQUIRE(sub.pending_order->parameters[0] == 10.0f);
    }
    
    // Simulate time passing
    auto later_time = std::chrono::microseconds(200000); // 200ms
    
    // Check which orders are ready
    auto completed = comm_sys.UpdateSubordinates(subordinates, later_time);
    
    REQUIRE(completed.size() > 0); // Some orders should be ready
}