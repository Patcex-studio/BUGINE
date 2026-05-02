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
#include "ai_core/behavior_tree.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_set>

#include <iostream> // For debugging, can be removed later

// Implementation of BehaviorTreeEngine

BehaviorTreeEngine::BehaviorTreeEngine() = default;
BehaviorTreeEngine::~BehaviorTreeEngine() = default;

// IDecisionEngine interface
DecisionResult BehaviorTreeEngine::Execute(DecisionRequest& req, std::vector<Command>& out_commands) {
    out_commands.clear();

    uint32_t instance_id = 0;
    for (const auto& [id, inst] : instances_) {
        if (inst.entity_id == req.entity_id) {
            instance_id = id;
            break;
        }
    }
    if (instance_id == 0) {
        instance_id = create_instance(0, req.entity_id); // Assume tree_id 0 for default behavior
    }

    const BTStatus status = tick_instance(instance_id, req, out_commands);
    if (status == BTStatus::Running) {
        return DecisionResult::Running;
    }
    if (out_commands.empty()) {
        out_commands.push_back({CommandType::IDLE, 0.5f});
    }
    return DecisionResult::Success;
}

void BehaviorTreeEngine::ResetContext(EntityId entity_id) {
    for (auto& [id, inst] : instances_) {
        if (inst.entity_id == entity_id) {
            inst.current_node = 0;
            inst.running_child = 0;
            inst.is_running = true;
            break;
        }
    }
}

std::optional<BTNodeSnapshot> BehaviorTreeEngine::SaveSnapshot(EntityId entity_id) const {
    for (const auto& [id, inst] : instances_) {
        if (inst.entity_id == entity_id) {
            BTNodeSnapshot snap;
            snap.tree_id = inst.tree_id;
            snap.node_index = inst.current_node;
            snap.blackboard_size = static_cast<uint8_t>(std::min<size_t>(snap.blackboard.size(), inst.blackboard.size() * sizeof(float)));
            std::memcpy(snap.blackboard.data(), inst.blackboard.data(), snap.blackboard_size);
            return snap;
        }
    }
    return std::nullopt;
}

bool BehaviorTreeEngine::RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snap) {
    for (auto& [id, inst] : instances_) {
        if (inst.entity_id == entity_id) {
            inst.tree_id = snap.tree_id;
            inst.current_node = snap.node_index;
            const size_t float_count = snap.blackboard.size() / sizeof(float);
            inst.blackboard.assign(float_count, 0.0f);
            if (!snap.blackboard.empty()) {
                std::memcpy(inst.blackboard.data(), snap.blackboard.data(), float_count * sizeof(float));
            }
            return true;
        }
    }
    return false;
}

// Add a behavior tree definition
void BehaviorTreeEngine::add_tree_definition(uint32_t tree_id, const std::vector<BehaviorTreeNode>& nodes) {
    tree_definitions_[tree_id] = nodes;
}

// Create a new instance
uint32_t BehaviorTreeEngine::create_instance(uint32_t tree_id, uint64_t entity_id) {
    uint32_t instance_id = next_instance_id_++;
    BehaviorTreeInstance instance{
        tree_id,
        entity_id,
        0, // start at root node
        0, // running_child
        0.0f,
        std::vector<float>(16, 0.0f), // blackboard with 16 floats
        true
    };
    instances_[instance_id] = instance;
    return instance_id;
}

// Tick the behavior tree instance
BTStatus BehaviorTreeEngine::tick_instance(uint32_t instance_id, DecisionRequest& req, std::vector<Command>& out_commands) {
    const auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return BTStatus::Failure;
    }

    auto& instance = it->second;
    if (!instance.is_running) {
        return BTStatus::Failure;
    }

    instance.execution_time += req.delta_time;
    const auto* tree = GetTreeDefinition(instance.tree_id);
    if (!tree || tree->empty()) {
        instance.is_running = false;
        return BTStatus::Failure;
    }

    const auto& root = (*tree)[0];
    const BTStatus status = tick_node(root, instance, req, out_commands);
    if (status != BTStatus::Running) {
        instance.execution_time = 0.0f;
    }

    instance.is_running = (status == BTStatus::Running);
    return status;
}

namespace {

static float DistanceSquared(const Vector3& a, const Vector3& b) noexcept {
    const Vector3 d = a - b;
    return d.x * d.x + d.y * d.y + d.z * d.z;
}

static bool IsVisibleThreat(const PerceptionState<16>& perception) noexcept {
    for (uint8_t i = 0; i < perception.threat_count; ++i) {
        if (perception.threats[i].confidence > 0.0f) {
            return true;
        }
    }
    return false;
}

static const ThreatRecord* ChooseHighestConfidenceThreat(const PerceptionState<16>& perception) noexcept {
    const ThreatRecord* best = nullptr;
    for (uint8_t i = 0; i < perception.threat_count; ++i) {
        const auto& threat = perception.threats[i];
        if (!best || threat.confidence > best->confidence) {
            best = &threat;
        }
    }
    return best;
}

static std::optional<size_t> FindUnitIndex(const UnitSoA* units, EntityId entity_id) noexcept {
    if (!units) {
        return std::nullopt;
    }
    if (!units->entity_ids.empty()) {
        for (size_t i = 0; i < units->entity_ids.size(); ++i) {
            if (units->entity_ids[i] == entity_id) {
                return i;
            }
        }
        return std::nullopt;
    }

    const size_t count = units->positions_x.size();
    if (entity_id < units->entity_id || entity_id >= units->entity_id + static_cast<EntityId>(count)) {
        return std::nullopt;
    }
    return static_cast<size_t>(entity_id - units->entity_id);
}

static const PerceptionState<16>* GetPerceptionState(const UnitSoA* units, EntityId entity_id) noexcept {
    const auto index = FindUnitIndex(units, entity_id);
    return index ? &units->perception_states[*index] : nullptr;
}

} // namespace

BTStatus BehaviorTreeEngine::tick_node(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands) {
    switch (node.node_type) {
        case BTNodeType::COMPOSITE_SEQUENCE:
            return tick_sequence(node, inst, req, out_commands);
        case BTNodeType::COMPOSITE_SELECTOR:
            return tick_selector(node, inst, req, out_commands);
        case BTNodeType::COMPOSITE_PARALLEL:
            return tick_parallel(node, inst, req, out_commands);
        case BTNodeType::DECORATOR_CONDITION:
        case BTNodeType::LEAF_CONDITION:
            return tick_condition(node, inst, req, out_commands);
        case BTNodeType::LEAF_ACTION:
            return tick_action(node, inst, req, out_commands);
        case BTNodeType::LEAF_CHECK_SUPPLY_LEVEL:
            return tick_check_supply_level(node, req);
        case BTNodeType::DECORATOR_INVERTER:
            if (node.children.empty()) {
                return BTStatus::Failure;
            }
            return tick_node(tree_definitions_.at(inst.tree_id)[node.children[0]], inst, req, out_commands) == BTStatus::Success
                ? BTStatus::Failure
                : BTStatus::Success;
        default:
            return BTStatus::Failure;
    }
}

BTStatus BehaviorTreeEngine::tick_sequence(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands) {
    for (size_t i = inst.running_child; i < node.children.size(); ++i) {
        const auto& child = tree_definitions_.at(inst.tree_id)[node.children[i]];
        BTStatus status = tick_node(child, inst, req, out_commands);
        if (status == BTStatus::Running) {
            inst.running_child = static_cast<uint32_t>(i);
            return BTStatus::Running;
        }
        if (status == BTStatus::Failure) {
            inst.running_child = 0;
            return BTStatus::Failure;
        }
    }
    inst.running_child = 0;
    return BTStatus::Success;
}

BTStatus BehaviorTreeEngine::tick_selector(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands) {
    for (size_t i = inst.running_child; i < node.children.size(); ++i) {
        const auto& child = tree_definitions_.at(inst.tree_id)[node.children[i]];
        BTStatus status = tick_node(child, inst, req, out_commands);
        if (status == BTStatus::Running) {
            inst.running_child = static_cast<uint32_t>(i);
            return BTStatus::Running;
        }
        if (status == BTStatus::Success) {
            inst.running_child = 0;
            return BTStatus::Success;
        }
    }
    inst.running_child = 0;
    return BTStatus::Failure;
}

BTStatus BehaviorTreeEngine::tick_parallel(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands) {
    bool all_success = true;
    bool any_running = false;
    for (uint32_t child_idx : node.children) {
        const auto& child = tree_definitions_.at(inst.tree_id)[child_idx];
        BTStatus status = tick_node(child, inst, req, out_commands);
        if (status == BTStatus::Failure) all_success = false;
        if (status == BTStatus::Running) any_running = true;
    }
    if (any_running) return BTStatus::Running;
    return all_success ? BTStatus::Success : BTStatus::Failure;
}

BTStatus BehaviorTreeEngine::tick_condition(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>&) {
    switch (node.condition_data.condition_type) {
        case BTCondition::HAS_ENEMY_IN_SIGHT: {
            const auto* perception = GetPerceptionState(req.units, req.entity_id);
            return (perception && IsVisibleThreat(*perception)) ? BTStatus::Success : BTStatus::Failure;
        }
        case BTCondition::IS_SUPPRESSED: {
            if (req.units && !req.units->suppression_levels.empty()) {
                const auto index = FindUnitIndex(req.units, req.entity_id);
                if (index) {
                    return req.units->suppression_levels[*index] > node.condition_data.threshold_value ? BTStatus::Success : BTStatus::Failure;
                }
            }
            return BTStatus::Failure;
        }
        case BTCondition::SUPPLY_LOW: {
            SupplyStatus::Weights weights;
            const float ratio = req.supply.WeightedRatio(weights);
            return ratio < node.condition_data.threshold_value ? BTStatus::Success : BTStatus::Failure;
        }
        default:
            return node.condition_data.threshold_value > 0.0f ? BTStatus::Success : BTStatus::Failure;
    }
}

BTStatus BehaviorTreeEngine::tick_action(const BehaviorTreeNode& node, BehaviorTreeInstance& inst, const DecisionRequest& req, std::vector<Command>& out_commands) {
    switch (node.action_data.action_type) {
        case BTAction::ATTACK_TARGET: {
            const auto* perception = GetPerceptionState(req.units, req.entity_id);
            if (!perception) {
                return BTStatus::Failure;
            }
            const ThreatRecord* target = ChooseHighestConfidenceThreat(*perception);
            if (!target || target->confidence <= 0.0f) {
                return BTStatus::Success;
            }
            const Vector3 target_pos = target->last_known_pos;
            out_commands.push_back({CommandType::ATTACK, 1.0f, 0, {target_pos.x, target_pos.y, target_pos.z, 0.0f}});
            return BTStatus::Running;
        }
        case BTAction::MOVE_TO_COVER: {
            const auto* perception = GetPerceptionState(req.units, req.entity_id);
            const auto index = FindUnitIndex(req.units, req.entity_id);
            if (!perception || !index) {
                return BTStatus::Failure;
            }
            const Vector3 current_pos{req.units->positions_x[*index], req.units->positions_y[*index], req.units->positions_z[*index]};
            Vector3 cover_pos = current_pos + Vector3{5.0f, 0.0f, 0.0f};
            if (perception->threat_count > 0) {
                const auto* threat = ChooseHighestConfidenceThreat(*perception);
                if (threat) {
                    const Vector3 away = current_pos - threat->last_known_pos;
                    const float length_sq = away.x * away.x + away.y * away.y + away.z * away.z;
                    const float norm = std::sqrt(std::max(0.0001f, length_sq));
                    cover_pos = (norm > 0.001f) ? current_pos + away * (5.0f / norm) : current_pos + Vector3{5.0f, 0.0f, 0.0f};
                }
            }
            out_commands.push_back({CommandType::SEEK_COVER, 0.9f, 0, {cover_pos.x, cover_pos.y, cover_pos.z, 0.0f}});
            const float distance_sq = DistanceSquared(current_pos, cover_pos);
            return distance_sq < 1.0f ? BTStatus::Success : BTStatus::Running;
        }
        case BTAction::FALLBACK: {
            out_commands.push_back({CommandType::IDLE, 0.2f, 0, {0.0f, 0.0f, 0.0f, 0.0f}});
            return inst.execution_time < 0.1f ? BTStatus::Running : BTStatus::Success;
        }
        default:
            return BTStatus::Success;
    }
}

BTStatus BehaviorTreeEngine::tick_check_supply_level(const BehaviorTreeNode& node, const DecisionRequest& req) {
    const float threshold = node.condition_data.threshold_value;
    const float overall = req.supply.WeightedRatio(SupplyStatus::Weights{});
    return overall < threshold ? BTStatus::Success : BTStatus::Failure;
}

const BehaviorTreeInstance* BehaviorTreeEngine::GetInstanceForEntity(EntityId entity_id) const {
    for (const auto& [id, inst] : instances_) {
        if (inst.entity_id == entity_id) {
            return &inst;
        }
    }
    return nullptr;
}

const std::vector<BehaviorTreeNode>* BehaviorTreeEngine::GetTreeDefinition(uint32_t tree_id) const {
    auto it = tree_definitions_.find(tree_id);
    return it != tree_definitions_.end() ? &it->second : nullptr;
}