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
#include "content_editor.h"
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "Content Editor System Test" << std::endl;
    std::cout << "==========================" << std::endl;

    // Initialize editor system
    content_editor::ContentEditor editor;
    if (!editor.initialize()) {
        std::cerr << "Failed to initialize content editor" << std::endl;
        return 1;
    }

    // Test vehicle editor
    auto& vehicle_editor = editor.get_vehicle_editor();
    content_editor::VehicleEditorState vehicle_state = {};

    // Load templates
    std::vector<content_editor::VehicleBlueprint> templates;
    if (vehicle_editor.load_vehicle_templates(templates)) {
        std::cout << "Loaded " << templates.size() << " vehicle templates" << std::endl;

        if (!templates.empty()) {
            // Create new blueprint from template
            auto start_time = std::chrono::high_resolution_clock::now();
            bool success = vehicle_editor.create_new_blueprint(templates[0], vehicle_state);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            if (success) {
                std::cout << "Created vehicle blueprint in " << duration.count() << " ms" << std::endl;
                std::cout << "Blueprint: " << vehicle_state.current_blueprint.blueprint_name << std::endl;
                std::cout << "Validation score: " << vehicle_state.current_blueprint.validation_result.validation_score << std::endl;
            } else {
                std::cout << "Failed to create vehicle blueprint" << std::endl;
            }
        }
    }

    // Test mission designer
    auto& mission_designer = editor.get_mission_designer();
    content_editor::MissionEditorState mission_state = {};

    // Load mission templates
    std::vector<content_editor::MissionDefinition> mission_templates;
    if (mission_designer.load_mission_templates(mission_templates)) {
        std::cout << "Loaded " << mission_templates.size() << " mission templates" << std::endl;

        if (!mission_templates.empty()) {
            // Create new mission from template
            auto start_time = std::chrono::high_resolution_clock::now();
            bool success = mission_designer.create_new_mission(mission_templates[0], mission_state);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            if (success) {
                std::cout << "Created mission in " << duration.count() << " ms" << std::endl;
                std::cout << "Mission: " << mission_state.current_mission.mission_name << std::endl;
                std::cout << "Balance score: " << mission_state.current_mission.validation_result.balance_score << std::endl;
            } else {
                std::cout << "Failed to create mission" << std::endl;
            }
        }
    }

    // Test scripting API
    auto& scripting_api = editor.get_scripting_api();

    // Test script execution
    std::string test_script = R"(
        log_message("Hello from Lua script!")
        local rand_num = get_random_number(1.0, 10.0)
        log_message("Random number: " .. rand_num)
        return "Script executed successfully"
    )";

    content_editor::ScriptExecutionContext context = {};
    context.script_id = 1;
    context.mission_id = 1;
    context.script_name = "test_script";

    content_editor::ScriptExecutionResult script_result;
    bool script_success = scripting_api.execute_script(test_script, context, script_result);

    if (script_success && script_result.success) {
        std::cout << "Script executed successfully in " << script_result.execution_time_ms << " ms" << std::endl;
        std::cout << "Return value: " << script_result.return_value << std::endl;
    } else {
        std::cout << "Script execution failed: " << script_result.error_message << std::endl;
    }

    // Test script validation
    std::string invalid_script = "invalid lua syntax {{{";
    std::string error_msg;
    bool valid = scripting_api.validate_script(invalid_script, error_msg);
    if (!valid) {
        std::cout << "Script validation correctly detected error: " << error_msg << std::endl;
    }

    // Shutdown
    editor.shutdown();

    std::cout << "Content Editor test completed" << std::endl;
    return 0;
}