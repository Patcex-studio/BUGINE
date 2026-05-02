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
#include "ai_core/pathfinding.h"

#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <algorithm>

// Implementation of PathfindingSystem
// For now, basic implementation; full A* with SIMD needed

namespace {

inline void unpack_position(const __m256& vector, float out[3]) {
    const float* data = reinterpret_cast<const float*>(&vector);
    out[0] = data[0];
    out[1] = data[1];
    out[2] = data[2];
}

inline float distance_squared(const __m256& a, const __m256& b) {
    float da[3];
    float db[3];
    unpack_position(a, da);
    unpack_position(b, db);
    float dx = da[0] - db[0];
    float dy = da[1] - db[1];
    float dz = da[2] - db[2];
    return dx * dx + dy * dy + dz * dz;
}

inline float distance(const __m256& a, const __m256& b) {
    return std::sqrt(distance_squared(a, b));
}

struct AStarNode {
    uint32_t node_id;
    float f_score;
};

struct AStarNodeCompare {
    bool operator()(const AStarNode& lhs, const AStarNode& rhs) const noexcept {
        return lhs.f_score > rhs.f_score;
    }
};

} // namespace

PathResult PathfindingSystem::find_path_simd(const PathfindingQuery& query) {
    PathResult result;

    if (nav_mesh_.empty()) {
        result.waypoints.push_back(query.start_position);
        result.waypoints.push_back(query.goal_position);
        result.total_cost = distance(query.start_position, query.goal_position);
        result.success = true;
        return result;
    }

    uint32_t start_node_id = 0;
    uint32_t goal_node_id = 0;
    float best_start_distance = std::numeric_limits<float>::infinity();
    float best_goal_distance = std::numeric_limits<float>::infinity();

    for (const auto& [node_id, node] : nav_mesh_) {
        float start_dist = distance_squared(node.world_position, query.start_position);
        if (start_dist < best_start_distance) {
            best_start_distance = start_dist;
            start_node_id = node_id;
        }

        float goal_dist = distance_squared(node.world_position, query.goal_position);
        if (goal_dist < best_goal_distance) {
            best_goal_distance = goal_dist;
            goal_node_id = node_id;
        }
    }

    struct NodeRecord {
        float g_score;
        float f_score;
        uint32_t came_from;
        bool closed;
    };

    auto heuristic = [&](uint32_t node_id) {
        const auto& node = nav_mesh_.at(node_id);
        return distance(node.world_position, query.goal_position);
    };

    std::priority_queue<AStarNode, std::vector<AStarNode>, AStarNodeCompare> open_set;
    std::unordered_map<uint32_t, NodeRecord> node_records;
    node_records[start_node_id] = {0.0f, heuristic(start_node_id), start_node_id, false};
    open_set.push({start_node_id, node_records[start_node_id].f_score});

    std::unordered_map<uint32_t, bool> forbidden;
    for (uint32_t zone : query.forbidden_zones) {
        forbidden[zone] = true;
    }

    bool path_found = false;
    while (!open_set.empty()) {
        const auto current = open_set.top();
        open_set.pop();

        auto& current_record = node_records[current.node_id];
        if (current_record.closed) {
            continue;
        }
        current_record.closed = true;

        if (current.node_id == goal_node_id) {
            path_found = true;
            break;
        }

        const auto& current_node = nav_mesh_.at(current.node_id);
        for (uint32_t neighbor_id : current_node.neighbors) {
            if (forbidden.find(neighbor_id) != forbidden.end()) {
                continue;
            }
            auto neighbor_it = nav_mesh_.find(neighbor_id);
            if (neighbor_it == nav_mesh_.end()) {
                continue;
            }
            const auto& neighbor_node = neighbor_it->second;
            float move_cost = neighbor_node.cost_to_enter + distance(current_node.world_position, neighbor_node.world_position);
            const float tentative_g_score = current_record.g_score + move_cost;
            auto record_it = node_records.find(neighbor_id);
            if (record_it == node_records.end() || tentative_g_score < record_it->second.g_score) {
                node_records[neighbor_id] = {tentative_g_score, tentative_g_score + heuristic(neighbor_id), current.node_id, false};
                open_set.push({neighbor_id, node_records[neighbor_id].f_score});
            }
        }
    }

    if (!path_found) {
        result.waypoints.push_back(query.start_position);
        result.waypoints.push_back(query.goal_position);
        result.total_cost = distance(query.start_position, query.goal_position);
        result.success = true;
        return result;
    }

    std::vector<uint32_t> path_nodes;
    uint32_t current_id = goal_node_id;
    while (true) {
        path_nodes.push_back(current_id);
        const auto& record = node_records[current_id];
        if (current_id == record.came_from) {
            break;
        }
        current_id = record.came_from;
    }
    std::reverse(path_nodes.begin(), path_nodes.end());

    float total_cost = 0.0f;
    __m256 previous_position = query.start_position;
    result.waypoints.push_back(previous_position);
    for (uint32_t node_id : path_nodes) {
        const auto& node = nav_mesh_.at(node_id);
        result.waypoints.push_back(node.world_position);
        total_cost += distance(previous_position, node.world_position) + node.cost_to_enter;
        previous_position = node.world_position;
    }
    result.waypoints.push_back(query.goal_position);
    total_cost += distance(previous_position, query.goal_position);

    result.total_cost = total_cost;
    result.success = true;
    return result;
}