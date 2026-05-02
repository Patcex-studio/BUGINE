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
#include "ai_core/ai_debugger.h"
#include <fstream>
#include <iostream>

#ifdef AI_DEBUGGER_USE_IMGUI
#include <imgui.h>
#endif

// Note: ImGui integration is enabled if AI_DEBUGGER_USE_IMGUI is defined.

AIDebugger::AIDebugger() : frame_counter_(0) {
    window_states_[0].width = 500.0f;   // Budget profiler
    window_states_[1].width = 400.0f;   // BT viewer
    window_states_[2].width = 400.0f;   // Decision explorer
    window_states_[3].width = 600.0f;   // Perception heatmap
    window_states_[4].width = 300.0f;   // Replay controls
}

AIDebugger::~AIDebugger() = default;

void AIDebugger::Initialize(TickBudgetEnforcer* tick_enforcer,
                            BehaviorTreeEngine* bt_engine,
                            ReplayPlayer* replay_player) {
    tick_enforcer_ = tick_enforcer;
    bt_engine_ = bt_engine;
    replay_player_ = replay_player;

    std::cout << "AIDebugger: Initialized with tick_enforcer, bt_engine, replay_player\n";
}

void AIDebugger::CollectData(uint64_t frame_id, const UnitSoA& units,
                            const EntityId* unit_selection,
                            size_t unit_selection_count) {
    if (!visible_ && selected_unit_ == 0) {
        return;  // Skip expensive collection if not visible
    }

    // Update budget history
    if (tick_enforcer_) {
        cache_.push_budget(tick_enforcer_->get_frame_usage());
    }

    // Clear and rebuild unit cache
    cache_.clear_units();

    // Add selected units to cache
    if (unit_selection && unit_selection_count > 0) {
        for (size_t i = 0; i < unit_selection_count; ++i) {
            CachedUnitDebugData unit_data;
            unit_data.entity_id = unit_selection[i];
            unit_data.is_selected = true;

            // TODO: Populate unit_data from units SoA
            // This requires expanding UnitSoA to include debug info

            cache_.unit_cache[unit_selection[i]] = unit_data;
        }
    }

    // Add selected unit to cache
    if (selected_unit_ != 0 && cache_.unit_cache.find(selected_unit_) == cache_.unit_cache.end()) {
        CachedUnitDebugData unit_data;
        unit_data.entity_id = selected_unit_;
        unit_data.is_selected = true;
        cache_.unit_cache[selected_unit_] = unit_data;
    }

    ++frame_counter_;
}

void AIDebugger::Render() {
    if (!visible_) {
        return;
    }

#ifdef AI_DEBUGGER_USE_IMGUI
    if (!ImGui::Begin("AI Debugger", &visible_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Budget Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderBudgetProfiler();
    }
    if (ImGui::CollapsingHeader("Behavior Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderBehaviorTreeViewer();
    }
    if (ImGui::CollapsingHeader("Decision Explorer")) {
        RenderDecisionExplorer();
    }
    if (ImGui::CollapsingHeader("Perception Heatmap")) {
        RenderPerceptionHeatmap();
    }
    if (ImGui::CollapsingHeader("Replay Controls")) {
        RenderReplayControls();
    }

    ImGui::End();
#else
    std::cout << "AIDebugger::Render() called; ImGui support not enabled.\n";
#endif
}

bool AIDebugger::HandleInput(int key_code, bool pressed) {
    if (!pressed) return false;

    // F3 toggle visibility
    if (key_code == 0xF3) {  // VK_F3
        visible_ = !visible_;
        std::cout << "AIDebugger: Toggled visibility to " << visible_ << "\n";
        return true;
    }

    // T toggle budget profiler
    if (key_code == 0x54) {  // 'T'
        show_budget_profiler_ = !show_budget_profiler_;
        return true;
    }

    // B toggle behavior tree viewer
    if (key_code == 0x42) {  // 'B'
        show_bt_viewer_ = !show_bt_viewer_;
        return true;
    }

    // D toggle decision explorer
    if (key_code == 0x44) {  // 'D'
        show_decision_explorer_ = !show_decision_explorer_;
        return true;
    }

    // H toggle perception heatmap
    if (key_code == 0x48) {  // 'H'
        show_perception_heatmap_ = !show_perception_heatmap_;
        return true;
    }

    // R toggle replay controls
    if (key_code == 0x52) {  // 'R'
        show_replay_controls_ = !show_replay_controls_;
        return true;
    }

    return false;
}

bool AIDebugger::ExportSummary(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "AIDebugger: Failed to open file for export: " << filename << "\n";
        return false;
    }

    // Write header
    file << "AI Debugger Summary Export\n";
    file << "====================================\n\n";

    // Write budget statistics
    if (tick_enforcer_) {
        file << "TICK BUDGET STATISTICS\n";
        file << "----------------------\n";
        file << "Smoothed Usage: " << (tick_enforcer_->get_smoothed_usage() * 100.0f) << "%\n";
        file << "Overrun Streak: " << tick_enforcer_->get_overrun_streak() << "\n";
        file << "\nBudget History (last 128 frames):\n";

        for (size_t i = 0; i < cache_.budget_history.size(); ++i) {
            const float usage = cache_.budget_history[i];
            if (usage > 0.0f) {
                file << "[" << i << "] " << (usage * 100.0f) << "%\n";
            }
        }
    }

    // Write unit cache info
    if (!cache_.unit_cache.empty()) {
        file << "\n\nCACHED UNITS\n";
        file << "------------\n";
        for (const auto& [entity_id, unit_data] : cache_.unit_cache) {
            file << "Entity " << entity_id << ":\n";
            file << "  Stress: " << unit_data.stress << "\n";
            file << "  Fatigue: " << unit_data.fatigue << "\n";
            file << "  Health: " << unit_data.health << "\n";
            file << "  BT Node: " << unit_data.bt_active_node_name << "\n";
        }
    }

    file << "\n\nEND OF EXPORT\n";
    file.close();

    std::cout << "AIDebugger: Exported summary to " << filename << "\n";
    return true;
}

// ============================================================================
// Private Methods (Stubs for ImGui integration)
// ============================================================================

void AIDebugger::RenderBudgetProfiler() {
#ifdef AI_DEBUGGER_USE_IMGUI
    const size_t history_size = cache_.budget_history.size();
    ImGui::PlotLines("Frame Budget", cache_.budget_history.data(), static_cast<int>(history_size), static_cast<int>(cache_.budget_write_idx % history_size), nullptr, 0.0f, 1.0f, ImVec2(window_states_[0].width - 30.0f, 120.0f));
    if (cache_.budget_history[0] != 0.0f) {
        const float current_usage = cache_.budget_history[(cache_.budget_write_idx + history_size - 1) % history_size];
        ImGui::Text("Current usage: %.1f%%", current_usage * 100.0f);
    }
    if (tick_enforcer_) {
        ImGui::Text("Smoothed usage: %.1f%%", tick_enforcer_->get_smoothed_usage() * 100.0f);
        ImGui::Text("Overrun streak: %u", tick_enforcer_->get_overrun_streak());
    }
#endif
}

void AIDebugger::RenderBehaviorTreeViewer() {
#ifdef AI_DEBUGGER_USE_IMGUI
    if (!bt_engine_) {
        ImGui::Text("Behavior tree engine not attached.");
        return;
    }

    if (selected_unit_ == 0) {
        ImGui::Text("Select a unit to view its behavior tree.");
        return;
    }

    const BehaviorTreeInstance* instance = bt_engine_->GetInstanceForEntity(selected_unit_);
    if (!instance) {
        ImGui::Text("No behavior tree instance for selected unit.");
        return;
    }

    ImGui::Text("Entity: %llu", static_cast<unsigned long long>(instance->entity_id));
    ImGui::Text("Tree ID: %u", instance->tree_id);
    ImGui::Text("Active node: %u", instance->current_node);

    const std::vector<BehaviorTreeNode>* tree = bt_engine_->GetTreeDefinition(instance->tree_id);
    if (!tree || tree->empty()) {
        ImGui::Text("Behavior tree definition unavailable.");
        return;
    }

    ImGui::Separator();
    for (uint32_t node_index = 0; node_index < tree->size(); ++node_index) {
        RenderTreeNode((*tree)[node_index], node_index, 0);
    }
#endif
}

void AIDebugger::RenderDecisionExplorer() {
#ifdef AI_DEBUGGER_USE_IMGUI
    for (const auto& [entity_id, unit] : cache_.unit_cache) {
        if (ImGui::TreeNode((void*)(uintptr_t)entity_id, "Entity %llu", static_cast<unsigned long long>(entity_id))) {
            ImGui::Text("BT Node: %s", unit.bt_active_node_name.c_str());
            ImGui::Text("Decision source: %u", static_cast<uint32_t>(unit.decision_source));
            ImGui::Text("Confidence: %.2f", unit.decision_confidence);
            ImGui::Text("Health: %.2f", unit.health);
            ImGui::Text("Stress: %.2f", unit.stress);
            ImGui::Text("Fatigue: %.2f", unit.fatigue);
            ImGui::TreePop();
        }
    }
#endif
}

void AIDebugger::RenderPerceptionHeatmap() {
#ifdef AI_DEBUGGER_USE_IMGUI
    ImGui::Text("Cached units: %zu", cache_.unit_cache.size());
    for (const auto& [entity_id, unit] : cache_.unit_cache) {
        ImGui::BulletText("%llu: pos=(%.1f, %.1f, %.1f) visible=%s", static_cast<unsigned long long>(entity_id), unit.position[0], unit.position[1], unit.position[2], unit.is_visible ? "yes" : "no");
    }
#endif
}

void AIDebugger::RenderReplayControls() {
#ifdef AI_DEBUGGER_USE_IMGUI
    if (!replay_player_) {
        ImGui::Text("Replay player not attached.");
        return;
    }

    ImGui::Text("Replay status: %s", replay_player_->IsLoaded() ? "Loaded" : "No replay");
    if (ImGui::Button("Play")) {
        replay_player_->Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        replay_player_->Pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        replay_player_->StepFrame();
    }

    if (const ReplayFrame* current_frame = replay_player_->GetCurrentFrame()) {
        ImGui::Text("Current frame: %u", static_cast<unsigned int>(current_frame->world_state.frame_id));
    }
#endif
}

void AIDebugger::RenderTreeNode(const BehaviorTreeNode& node, uint32_t node_index, int depth) {
#ifdef AI_DEBUGGER_USE_IMGUI
    const ImGuiTreeNodeFlags flags = (node_index == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    const bool open = ImGui::TreeNodeEx((void*)(uintptr_t)node_index, flags, "%s", node.node_name.c_str());
    if (open) {
        ImGui::Text("Type: %u", static_cast<uint32_t>(node.node_type));
        ImGui::Text("Child count: %zu", node.children.size());
        if (node.children.empty()) {
            ImGui::Text("Leaf node");
        }
        ImGui::TreePop();
    }
#endif
}

void AIDebugger::RenderUnitIndicator(const CachedUnitDebugData& unit, float screen_scale) {
#ifdef AI_DEBUGGER_USE_IMGUI
    ImGui::Text("Unit %llu indicator (scale %.2f)", static_cast<unsigned long long>(unit.entity_id), screen_scale);
#endif
}
