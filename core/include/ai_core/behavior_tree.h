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
#include <string>
#include <array>
#include <unordered_map>

// Behavior Tree Node Types
enum class BTNodeType {
    COMPOSITE_SEQUENCE = 0,        // Выполнять дочерние ноды по порядку
    COMPOSITE_SELECTOR = 1,         // Выполнять пока не найдет успешную ноду
    COMPOSITE_PARALLEL = 2,         // Выполнять все дочерние ноды параллельно
    DECORATOR_CONDITION = 3,        // Условие для выполнения дочерней ноды
    DECORATOR_INVERTER = 4,         // Инвертировать результат дочерней ноды
    LEAF_ACTION = 5,                // Конечное действие
    LEAF_CONDITION = 6,             // Проверка условия
    LEAF_CHECK_SUPPLY_LEVEL = 7     // Check supply level
};

namespace BTCondition {
    inline constexpr uint32_t HAS_ENEMY_IN_SIGHT = 1;
    inline constexpr uint32_t IS_SUPPRESSED = 2;
    inline constexpr uint32_t SUPPLY_LOW = 3;
}

namespace BTAction {
    inline constexpr uint32_t ATTACK_TARGET = 1;
    inline constexpr uint32_t MOVE_TO_COVER = 2;
    inline constexpr uint32_t FALLBACK = 3;
}

enum class BTStatus {
    Success,
    Failure,
    Running
};

struct BehaviorTreeNode {
    uint32_t node_id;               // Unique node identifier
    BTNodeType node_type;           // Type of node
    std::vector<uint32_t> children; // Child node indices
    std::string node_name;          // Human-readable name for debugging

    // Node-specific data
    union {
        struct {                    // For action nodes
            uint32_t action_type;
            std::array<float, 8> action_parameters;
        } action_data;

        struct {                    // For condition nodes
            uint32_t condition_type;
            float threshold_value;
            bool invert_result;
        } condition_data;

        struct {                    // For decorator nodes
            uint32_t decorator_type;
            std::array<float, 4> decorator_parameters;
        } decorator_data;
    };
};

struct BehaviorTreeInstance {
    uint32_t tree_id;               // Reference to behavior tree definition
    uint64_t entity_id;             // Associated game entity (assuming EntityID is uint64_t)
    uint32_t current_node;          // Current execution node
    uint32_t running_child;         // For Sequence/Selector: which child is running
    float execution_time;           // Time spent in current node
    std::vector<float> blackboard;   // Shared memory for tree variables
    bool is_running;                // Execution state
};

#include "ai_core/decision_engine.h"

class BehaviorTreeEngine final : public IDecisionEngine {
public:
    BehaviorTreeEngine();
    ~BehaviorTreeEngine();

    // IDecisionEngine interface
    DecisionResult Execute(DecisionRequest& req, std::vector<Command>& out_commands) override;
    void ResetContext(EntityId entity_id) override;
    std::optional<BTNodeSnapshot> SaveSnapshot(EntityId entity_id) const override;
    bool RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snap) override;

    // Add a behavior tree definition
    void add_tree_definition(uint32_t tree_id, const std::vector<BehaviorTreeNode>& nodes);

    // Create a new instance
    uint32_t create_instance(uint32_t tree_id, uint64_t entity_id);

    // Tick the behavior tree instance
    BTStatus tick_instance(uint32_t instance_id, DecisionRequest& req, std::vector<Command>& out_commands);

    // Debug helpers
    const BehaviorTreeInstance* GetInstanceForEntity(EntityId entity_id) const;
    const std::vector<BehaviorTreeNode>* GetTreeDefinition(uint32_t tree_id) const;

private:
    BTStatus tick_node(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_sequence(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_selector(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_parallel(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_condition(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_action(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands);
    BTStatus tick_check_supply_level(const BehaviorTreeNode& node, const DecisionRequest& req);

    std::unordered_map<uint32_t, std::vector<BehaviorTreeNode>> tree_definitions_;
    std::unordered_map<uint32_t, BehaviorTreeInstance> instances_;
    uint32_t next_instance_id_ = 1;
};