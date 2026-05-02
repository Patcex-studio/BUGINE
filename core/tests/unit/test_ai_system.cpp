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
#include <gtest/gtest.h>
#include "ai_core/behavior_tree.h"
#include "ai_core/pathfinding.h"
#include "ai_core/tactical_decision.h"

// Test Behavior Tree
TEST(BehaviorTreeTest, CreateInstance) {
    BehaviorTreeSystem bt_system;
    std::vector<BehaviorTreeNode> nodes = {
        {0, BTNodeType::LEAF_ACTION, {}, "Action1"}
    };
    bt_system.add_tree_definition(1, nodes);
    uint32_t instance_id = bt_system.create_instance(1, 123);
    EXPECT_NE(instance_id, 0u);
}

// Test Pathfinding
TEST(PathfindingTest, SubmitQuery) {
    PathfindingSystem pf_system;
    PathfindingQuery query{123, _mm256_set1_ps(0.0f), _mm256_set1_ps(10.0f), 0, 100.0f, {}, {}};
    pf_system.submit_query(query);
    pf_system.process_queries();
    const auto* result = pf_system.get_path(123);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
}

// Test Tactical Decision
TEST(TacticalDecisionTest, EvaluateSituation) {
    TacticalDecisionSystem td_system;
    TacticalSituation situation{123, {124}, {125}, _mm256_set1_ps(0.0f), 500.0f, 0.8f, 1};
    TacticalDecision decision = td_system.evaluate_situation(situation);
    EXPECT_EQ(decision.decision_type, 2u); // RETREAT due to high threat
}