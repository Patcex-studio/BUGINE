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
#include "content_editor/mission_designer.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

namespace content_editor {

class MissionDesigner::Impl {
public:
    std::vector<MissionDefinition> loaded_missions_;
    std::vector<MissionDefinition> mission_templates_;
    std::mt19937 random_engine_;

    Impl() : random_engine_(std::random_device{}()) {}

    uint32_t generate_unique_id() {
        return static_cast<uint32_t>(random_engine_());
    }

    bool validate_mission_internal(const MissionDefinition& mission, MissionValidationResult& result) {
        result.error_count = 0;
        result.warning_count = 0;
        result.balance_score = 1.0f;
        result.difficulty_score = 1.0f;
        result.is_valid = true;

        // Check objectives
        if (mission.primary_objectives.empty()) {
            result.error_count++;
            result.is_valid = false;
            result.balance_score -= 0.2f;
        }

        // Check unit balance
        size_t player_units = mission.player_units.size();
        size_t enemy_units = mission.enemy_units.size();

        if (enemy_units == 0) {
            result.error_count++;
            result.is_valid = false;
        }

        // Simple balance check
        if (player_units > enemy_units * 2) {
            result.warning_count++;
            result.balance_score -= 0.1f;
        }

        // Check map exists (placeholder)
        if (strlen(mission.map_name) == 0) {
            result.error_count++;
            result.is_valid = false;
        }

        // Check time limits
        if (mission.estimated_playtime_minutes <= 0 || mission.estimated_playtime_minutes > 480) {
            result.warning_count++;
            result.difficulty_score -= 0.05f;
        }

        result.balance_score = std::max(0.0f, result.balance_score);
        result.difficulty_score = std::max(0.0f, result.difficulty_score);

        return result.is_valid;
    }

    void initialize_template_missions() {
        // Initialize with basic mission template
        MissionDefinition skirmish_template = {};
        skirmish_template.mission_id = generate_unique_id();
        strcpy(skirmish_template.mission_name, "Basic Skirmish Template");
        strcpy(skirmish_template.mission_description, "A basic tank skirmish mission");
        skirmish_template.mission_type = MissionType::SKIRMISH;
        skirmish_template.era = Era::WW2;
        skirmish_template.nations_involved = (1 << static_cast<uint32_t>(Nation::USSR)) | (1 << static_cast<uint32_t>(Nation::GERMANY));

        strcpy(skirmish_template.map_name, "default_map");
        skirmish_template.start_time_hours = 12.0f;
        skirmish_template.weather_conditions = 0.5f; // Moderate weather
        skirmish_template.visibility_range_km = 2.0f;

        skirmish_template.estimated_playtime_minutes = 30;
        skirmish_template.difficulty_rating = 3;
        skirmish_template.is_balanced = true;

        mission_templates_.push_back(skirmish_template);
    }
};

MissionDesigner::MissionDesigner() : impl_(new Impl()) {
    impl_->initialize_template_missions();
}

MissionDesigner::~MissionDesigner() {
    delete impl_;
}

bool MissionDesigner::create_new_mission(const MissionDefinition& template_spec, MissionEditorState& editor_state) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Copy template to current mission
    editor_state.current_mission = template_spec;
    editor_state.current_mission.mission_id = impl_->generate_unique_id();

    // Initialize editor state
    editor_state.unit_previews.clear();
    editor_state.trigger_previews.clear();
    editor_state.selected_tool = 0;
    editor_state.selected_unit = 0;
    editor_state.is_playing_test = false;
    editor_state.test_progress = 0.0f;

    // Set default camera position
    float camera_pos[8] = {100.0f, 100.0f, 50.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    editor_state.editor_camera_position = _mm256_loadu_ps(camera_pos);

    float camera_rot[8] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    editor_state.editor_camera_rotation = _mm256_loadu_ps(camera_rot);

    // Validate initial mission
    validate_mission(editor_state, editor_state.current_mission.validation_result);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Created new mission '" << editor_state.current_mission.mission_name
              << "' in " << duration.count() << " ms" << std::endl;

    return duration.count() < 200; // Check performance target
}

bool MissionDesigner::load_mission(uint32_t mission_id, MissionEditorState& editor_state) {
    // Find mission in loaded missions
    auto it = std::find_if(impl_->loaded_missions_.begin(), impl_->loaded_missions_.end(),
                          [mission_id](const MissionDefinition& mission) { return mission.mission_id == mission_id; });

    if (it != impl_->loaded_missions_.end()) {
        editor_state.current_mission = *it;
        return true;
    }

    // Try to load from file (placeholder)
    std::string filename = "mission_" + std::to_string(mission_id) + ".bin";
    std::ifstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(&editor_state.current_mission), sizeof(MissionDefinition));
        impl_->loaded_missions_.push_back(editor_state.current_mission);
        return true;
    }

    return false;
}

bool MissionDesigner::save_mission(const MissionEditorState& editor_state) {
    std::string filename = "mission_" + std::to_string(editor_state.current_mission.mission_id) + ".bin";
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&editor_state.current_mission), sizeof(MissionDefinition));
        return true;
    }
    return false;
}

bool MissionDesigner::export_mission(const MissionEditorState& editor_state, const char* export_path) {
    // Placeholder export implementation
    std::ofstream file(export_path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&editor_state.current_mission), sizeof(MissionDefinition));
        return true;
    }
    return false;
}

bool MissionDesigner::add_unit_to_mission(MissionEditorState& editor_state, const UnitPlacement& unit) {
    editor_state.current_mission.player_units.push_back(unit);

    // Add preview
    UnitPreview preview = {};
    preview.unit_id = unit.unit_template_id;
    preview.preview_position = unit.world_position;
    preview.preview_rotation = unit.world_rotation;
    preview.preview_scale = 1.0f;
    preview.team_color = unit.unit_team;
    preview.is_selected = false;
    preview.is_visible = true;
    editor_state.unit_previews.push_back(preview);

    // Re-validate
    validate_mission(editor_state, editor_state.current_mission.validation_result);

    return true;
}

bool MissionDesigner::remove_unit_from_mission(MissionEditorState& editor_state, uint32_t unit_id) {
    // Remove from units
    auto unit_it = std::remove_if(editor_state.current_mission.player_units.begin(),
                                 editor_state.current_mission.player_units.end(),
                                 [unit_id](const UnitPlacement& unit) { return unit.unit_template_id == unit_id; });
    editor_state.current_mission.player_units.erase(unit_it, editor_state.current_mission.player_units.end());

    // Remove from previews
    auto preview_it = std::remove_if(editor_state.unit_previews.begin(),
                                    editor_state.unit_previews.end(),
                                    [unit_id](const UnitPreview& preview) { return preview.unit_id == unit_id; });
    editor_state.unit_previews.erase(preview_it, editor_state.unit_previews.end());

    // Re-validate
    validate_mission(editor_state, editor_state.current_mission.validation_result);

    return true;
}

bool MissionDesigner::update_unit_position(MissionEditorState& editor_state, uint32_t unit_id, const __m256& position) {
    // Update unit position
    for (auto& unit : editor_state.current_mission.player_units) {
        if (unit.unit_template_id == unit_id) {
            unit.world_position = position;
            break;
        }
    }

    // Update preview position
    for (auto& preview : editor_state.unit_previews) {
        if (preview.unit_id == unit_id) {
            preview.preview_position = position;
            break;
        }
    }

    return true;
}

bool MissionDesigner::add_objective_to_mission(MissionEditorState& editor_state, const MissionObjective& objective) {
    editor_state.current_mission.primary_objectives.push_back(objective);

    // Re-validate
    validate_mission(editor_state, editor_state.current_mission.validation_result);

    return true;
}

bool MissionDesigner::remove_objective_from_mission(MissionEditorState& editor_state, uint32_t objective_id) {
    auto it = std::remove_if(editor_state.current_mission.primary_objectives.begin(),
                            editor_state.current_mission.primary_objectives.end(),
                            [objective_id](const MissionObjective& obj) { return obj.objective_id == objective_id; });
    editor_state.current_mission.primary_objectives.erase(it, editor_state.current_mission.primary_objectives.end());

    // Re-validate
    validate_mission(editor_state, editor_state.current_mission.validation_result);

    return true;
}

bool MissionDesigner::add_trigger_to_mission(MissionEditorState& editor_state, const MissionTrigger& trigger) {
    editor_state.current_mission.triggers.push_back(trigger);

    // Add preview
    TriggerPreview preview = {};
    preview.trigger_id = trigger.trigger_id;
    preview.preview_position = trigger.trigger_position;
    preview.preview_radius = trigger.trigger_radius;
    preview.trigger_type_color = static_cast<uint32_t>(trigger.trigger_type);
    preview.is_selected = false;
    preview.is_active = trigger.is_active;
    editor_state.trigger_previews.push_back(preview);

    return true;
}

bool MissionDesigner::remove_trigger_from_mission(MissionEditorState& editor_state, uint32_t trigger_id) {
    // Remove from triggers
    auto trigger_it = std::remove_if(editor_state.current_mission.triggers.begin(),
                                    editor_state.current_mission.triggers.end(),
                                    [trigger_id](const MissionTrigger& trigger) { return trigger.trigger_id == trigger_id; });
    editor_state.current_mission.triggers.erase(trigger_it, editor_state.current_mission.triggers.end());

    // Remove from previews
    auto preview_it = std::remove_if(editor_state.trigger_previews.begin(),
                                    editor_state.trigger_previews.end(),
                                    [trigger_id](const TriggerPreview& preview) { return preview.trigger_id == trigger_id; });
    editor_state.trigger_previews.erase(preview_it, editor_state.trigger_previews.end());

    return true;
}

bool MissionDesigner::validate_mission(const MissionEditorState& editor_state, MissionValidationResult& result) {
    auto start_time = std::chrono::high_resolution_clock::now();

    bool valid = impl_->validate_mission_internal(editor_state.current_mission, result);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Validated mission in " << duration.count() << " ms" << std::endl;

    return duration.count() < 100; // Check performance target
}

bool MissionDesigner::start_mission_test(MissionEditorState& editor_state) {
    if (editor_state.is_playing_test) {
        return false;
    }

    editor_state.is_playing_test = true;
    editor_state.test_progress = 0.0f;

    std::cout << "Started mission test for '" << editor_state.current_mission.mission_name << "'" << std::endl;

    return true;
}

bool MissionDesigner::stop_mission_test(MissionEditorState& editor_state) {
    if (!editor_state.is_playing_test) {
        return false;
    }

    editor_state.is_playing_test = false;
    editor_state.test_progress = 0.0f;

    std::cout << "Stopped mission test" << std::endl;

    return true;
}

bool MissionDesigner::get_test_status(const MissionEditorState& editor_state, float& progress, bool& is_complete) {
    progress = editor_state.test_progress;
    is_complete = (progress >= 1.0f);

    if (editor_state.is_playing_test && !is_complete) {
        // Simulate progress (in real implementation, this would be actual test progress)
        const float delta_progress = 0.01f; // 1% per call
        editor_state.test_progress = std::min(1.0f, editor_state.test_progress + delta_progress);
    }

    return true;
}

bool MissionDesigner::load_mission_templates(std::vector<MissionDefinition>& templates) {
    templates = impl_->mission_templates_;
    return true;
}

bool MissionDesigner::create_template_from_mission(const MissionDefinition& mission, const char* template_name) {
    MissionDefinition new_template = mission;
    strcpy(new_template.mission_name, template_name);
    new_template.mission_id = impl_->generate_unique_id();
    impl_->mission_templates_.push_back(new_template);
    return true;
}

} // namespace content_editor