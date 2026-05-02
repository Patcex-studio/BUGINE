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

#include <array>
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>
#include "ai_core/decision_engine.h"
#include "ai_core/tick_budget_enforcer.h"
#include "ai_core/behavior_tree.h"
#include "ai_core/replay_player.h"

// Forward declarations
struct ImDrawList;
struct ImGuiIO;

/**
 * Cached debug data for a single unit
 */
struct CachedUnitDebugData {
    EntityId entity_id = 0;
    bool is_selected = false;
    bool is_visible = false;

    // Behavior tree state
    uint32_t bt_tree_id = 0;
    uint32_t bt_active_node = 0xFFFFFFFFU;
    std::string bt_active_node_name;

    // Decision info
    DecisionSource decision_source = DecisionSource::SafeDefault;
    float decision_confidence = 0.5f;

    // Unit state (for heatmap)
    float stress = 0.0f;
    float fatigue = 0.0f;
    float health = 1.0f;
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
};

/**
 * Debug cache for rendering
 */
struct DebugCache {
    // Budget history (last 128 frames)
    std::array<float, 128> budget_history = {};
    uint32_t budget_write_idx = 0;

    // Unit state cache
    std::unordered_map<EntityId, CachedUnitDebugData> unit_cache;

    void push_budget(float usage) {
        budget_history[budget_write_idx++ % budget_history.size()] = usage;
    }

    void clear_units() {
        unit_cache.clear();
    }
};

/**
 * Interactive ImGui-based AI debugger
 * 
 * Provides real-time visualization of:
 * - Tick budget usage and overrun statistics
 * - Behavior tree execution state
 * - Decision confidence and sources
 * - Perception heatmaps
 * - Replay controls
 */
class AIDebugger {
public:
    /**
     * Constructor
     */
    AIDebugger();

    /**
     * Destructor
     */
    ~AIDebugger();

    /**
     * Initialize debugger with system references
     * @param tick_enforcer Reference to tick budget enforcer
     * @param bt_engine Reference to behavior tree engine (optional)
     * @param replay_player Reference to replay player (optional)
     */
    void Initialize(TickBudgetEnforcer* tick_enforcer,
                    BehaviorTreeEngine* bt_engine = nullptr,
                    ReplayPlayer* replay_player = nullptr);

    /**
     * Collect debug data from AI systems
     * Should be called in AI tick loop (can allocate freely)
     * 
     * @param frame_id Current frame ID
     * @param units Unit data for snapshots
     * @param unit_selection Array of selected unit IDs
     * @param unit_selection_count Number of selected units
     */
    void CollectData(uint64_t frame_id, const UnitSoA& units,
                     const EntityId* unit_selection = nullptr,
                     size_t unit_selection_count = 0);

    /**
     * Render all debugger UI windows
     * Should be called during ImGui render phase
     * Uses collected data (no new allocations critical)
     */
    void Render();

    /**
     * Handle hotkey input
     * @return true if hotkey was handled
     */
    bool HandleInput(int key_code, bool pressed = true);

    /**
     * Set debugger visibility
     */
    void SetVisible(bool visible) noexcept {
        visible_ = visible;
    }

    /**
     * Get debugger visibility
     */
    [[nodiscard]] bool IsVisible() const noexcept {
        return visible_;
    }

    /**
     * Select a unit for detailed inspection
     * @param entity_id Entity to select, 0 to deselect
     */
    void SelectUnit(EntityId entity_id) noexcept {
        selected_unit_ = entity_id;
    }

    /**
     * Get currently selected unit
     */
    [[nodiscard]] EntityId GetSelectedUnit() const noexcept {
        return selected_unit_;
    }

    /**
     * Export debug summary to file
     * @param filename Output filename
     * @return true if successful
     */
    bool ExportSummary(const std::string& filename) const;

private:
    // Render individual windows
    void RenderBudgetProfiler();
    void RenderBehaviorTreeViewer();
    void RenderDecisionExplorer();
    void RenderPerceptionHeatmap();
    void RenderReplayControls();

    // Helper functions
    void RenderTreeNode(const BehaviorTreeNode& node, uint32_t node_index, int depth);
    void RenderUnitIndicator(const CachedUnitDebugData& unit, float screen_scale);

    // System references
    TickBudgetEnforcer* tick_enforcer_ = nullptr;
    BehaviorTreeEngine* bt_engine_ = nullptr;
    ReplayPlayer* replay_player_ = nullptr;

    // View state
    bool visible_ = false;
    EntityId selected_unit_ = 0;
    bool show_budget_profiler_ = false;
    bool show_bt_viewer_ = false;
    bool show_decision_explorer_ = false;
    bool show_perception_heatmap_ = false;
    bool show_replay_controls_ = false;

    // Data cache
    DebugCache cache_;

    // Window states
    struct WindowState {
        bool collapsed = false;
        float width = 400.0f;
        float height = 300.0f;
    };

    std::array<WindowState, 5> window_states_;

    // Hover state for heatmap
    std::optional<EntityId> hovered_unit_;

    // Throttling for expensive operations
    uint32_t frame_counter_ = 0;
};
