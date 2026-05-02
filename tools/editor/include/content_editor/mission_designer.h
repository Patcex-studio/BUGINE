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
#include <array>
#include <string>
#include <immintrin.h>
#include "physics_core/types.h"

namespace content_editor {

// Forward declarations
struct MissionObjective;
struct UnitPlacement;
struct MissionTrigger;
struct ScriptedEvent;
struct ConditionalBranch;
struct MissionValidationResult;
struct UnitPreview;
struct TriggerPreview;

// Mission types
enum class MissionType : uint32_t {
    CAMPAIGN = 0,
    SKIRMISH = 1,
    MULTIPLAYER = 2,
    TUTORIAL = 3,
    HISTORICAL = 4,
    COUNT
};

// Objective types
enum class ObjectiveType : uint32_t {
    DESTROY_UNITS = 0,
    CAPTURE_POINT = 1,
    DEFEND_POSITION = 2,
    ESCORT_UNIT = 3,
    SURVIVE_TIME = 4,
    COLLECT_ITEMS = 5,
    COUNT
};

// Trigger types
enum class TriggerType : uint32_t {
    TIME_BASED = 0,
    UNIT_DESTROYED = 1,
    POSITION_REACHED = 2,
    CONDITION_MET = 3,
    PLAYER_ACTION = 4,
    COUNT
};

// Mission definition structure
struct alignas(32) MissionDefinition {
    uint32_t mission_id;           // Unique mission identifier
    char mission_name[128];        // Human-readable mission name
    char mission_description[512]; // Mission description
    MissionType mission_type;      // Mission type
    Era era;                       // Historical period
    uint32_t nations_involved;     // Bitmask of involved nations

    // Map and environment
    char map_name[64];            // Map filename
    float start_time_hours;        // Mission start time
    float weather_conditions;      // Weather settings
    float visibility_range_km;     // Visibility range

    // Objective structure
    std::vector<MissionObjective> primary_objectives; // Primary objectives
    std::vector<MissionObjective> secondary_objectives; // Secondary objectives
    std::vector<MissionObjective> bonus_objectives; // Bonus objectives

    // Unit placement
    std::vector<UnitPlacement> player_units; // Player-controlled units
    std::vector<UnitPlacement> enemy_units; // Enemy units
    std::vector<UnitPlacement> civilian_units; // Civilian units
    std::vector<UnitPlacement> reinforcement_units; // Reinforcements

    // Trigger system
    std::vector<MissionTrigger> triggers; // Event triggers
    std::vector<ScriptedEvent> scripted_events; // Scripted events
    std::vector<ConditionalBranch> conditional_branches; // Conditional logic

    // Validation and metadata
    MissionValidationResult validation_result; // Validation status
    uint32_t estimated_playtime_minutes; // Estimated playtime
    uint32_t difficulty_rating;     // Difficulty rating (1-10)
    bool is_balanced;              // Is the mission balanced?
};

// Mission objective
struct alignas(32) MissionObjective {
    uint32_t objective_id;
    char objective_name[128];
    ObjectiveType objective_type;
    __m256 target_position;        // Target location (x,y,z,w)
    uint32_t target_unit_count;    // Number of units to affect
    float time_limit_seconds;      // Time limit for completion
    float completion_percentage;   // Current completion (0.0-1.0)
    bool is_completed;
    bool is_failed;
    uint32_t reward_points;        // Points awarded for completion
};

// Unit placement definition
struct alignas(32) UnitPlacement {
    physics_core::EntityID unit_template_id; // Template for the unit
    __m256 world_position;         // World position (x,y,z,w)
    __m256 world_rotation;         // World rotation (rx,ry,rz,rw)
    uint32_t unit_team;           // PLAYER, ENEMY, NEUTRAL
    float initial_health;          // Starting health percentage
    std::vector<uint32_t> ai_behaviors; // AI behavior tree IDs
    std::vector<uint32_t> script_hooks; // Script hook IDs
    uint32_t spawn_condition;      // When to spawn the unit
    float spawn_delay_seconds;     // Delay before spawning
    bool is_reinforcement;         // Is this a reinforcement?
    bool can_be_destroyed;         // Can this unit be destroyed?
};

// Mission trigger
struct alignas(32) MissionTrigger {
    uint32_t trigger_id;
    char trigger_name[64];
    TriggerType trigger_type;
    __m256 trigger_position;       // Position for spatial triggers
    uint32_t trigger_condition;    // Condition to check
    float trigger_radius;          // Radius for spatial triggers
    float trigger_time;            // Time for time-based triggers
    bool is_active;
    bool has_fired;
    std::vector<uint32_t> linked_events; // Events to trigger
};

// Scripted event
struct ScriptedEvent {
    uint32_t event_id;
    char event_name[64];
    char script_code[1024];       // Lua script code
    uint32_t execution_context;   // When to execute
    bool is_executed;
    float execution_delay;        // Delay before execution
};

// Conditional branch
struct ConditionalBranch {
    uint32_t branch_id;
    char condition_name[64];
    char condition_script[512];   // Lua condition
    uint32_t true_branch_event;   // Event if true
    uint32_t false_branch_event;  // Event if false
    bool is_evaluated;
    bool condition_result;
};

// Mission validation result
struct MissionValidationResult {
    bool is_valid;
    uint32_t error_count;
    uint32_t warning_count;
    float balance_score;          // 0.0 to 1.0
    float difficulty_score;       // 0.0 to 1.0
};

// Unit preview for editor
struct UnitPreview {
    uint32_t unit_id;
    __m256 preview_position;
    __m256 preview_rotation;
    float preview_scale;
    uint32_t team_color;
    bool is_selected;
    bool is_visible;
};

// Trigger preview for editor
struct TriggerPreview {
    uint32_t trigger_id;
    __m256 preview_position;
    float preview_radius;
    uint32_t trigger_type_color;
    bool is_selected;
    bool is_active;
};

// Mission editor state
struct alignas(32) MissionEditorState {
    MissionDefinition current_mission; // Current mission being edited
    std::vector<UnitPreview> unit_previews; // Preview of placed units
    std::vector<TriggerPreview> trigger_previews; // Preview of triggers
    __m256 editor_camera_position; // Editor camera position
    __m256 editor_camera_rotation; // Editor camera rotation
    uint32_t selected_tool;        // Currently selected editor tool
    uint32_t selected_unit;        // Currently selected unit
    bool is_playing_test;         // Currently playing test version
    float test_progress;          // Progress in test playthrough
};

// Mission Designer main class
class MissionDesigner {
public:
    MissionDesigner();
    ~MissionDesigner();

    // Mission management
    bool create_new_mission(const MissionDefinition& template_spec, MissionEditorState& editor_state);
    bool load_mission(uint32_t mission_id, MissionEditorState& editor_state);
    bool save_mission(const MissionEditorState& editor_state);
    bool export_mission(const MissionEditorState& editor_state, const char* export_path);

    // Unit placement management
    bool add_unit_to_mission(MissionEditorState& editor_state, const UnitPlacement& unit);
    bool remove_unit_from_mission(MissionEditorState& editor_state, uint32_t unit_id);
    bool update_unit_position(MissionEditorState& editor_state, uint32_t unit_id, const __m256& position);

    // Objective management
    bool add_objective_to_mission(MissionEditorState& editor_state, const MissionObjective& objective);
    bool remove_objective_from_mission(MissionEditorState& editor_state, uint32_t objective_id);

    // Trigger system management
    bool add_trigger_to_mission(MissionEditorState& editor_state, const MissionTrigger& trigger);
    bool remove_trigger_from_mission(MissionEditorState& editor_state, uint32_t trigger_id);

    // Validation
    bool validate_mission(const MissionEditorState& editor_state, MissionValidationResult& result);

    // Test execution
    bool start_mission_test(MissionEditorState& editor_state);
    bool stop_mission_test(MissionEditorState& editor_state);
    bool get_test_status(const MissionEditorState& editor_state, float& progress, bool& is_complete);

    // Template management
    bool load_mission_templates(std::vector<MissionDefinition>& templates);
    bool create_template_from_mission(const MissionDefinition& mission, const char* template_name);

private:
    class Impl;
    Impl* impl_;
};

} // namespace content_editor