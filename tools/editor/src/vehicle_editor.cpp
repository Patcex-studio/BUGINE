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
#include "content_editor/vehicle_editor.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

namespace content_editor {

class VehicleEditor::Impl {
public:
    std::vector<VehicleBlueprint> loaded_blueprints_;
    std::vector<VehicleBlueprint> vehicle_templates_;
    std::mt19937 random_engine_;

    Impl() : random_engine_(std::random_device{}()) {}

    uint32_t generate_unique_id() {
        return static_cast<uint32_t>(random_engine_());
    }

    bool validate_blueprint_internal(const VehicleBlueprint& blueprint, VehicleValidationResult& result) {
        result.error_count = 0;
        result.warning_count = 0;
        result.validation_score = 1.0f;
        result.is_valid = true;

        // Check required slots are filled
        for (const auto& slot : blueprint.component_slots) {
            if (slot.is_required) {
                bool slot_filled = false;
                for (const auto& compat : blueprint.compatible_components) {
                    if ((compat.compatibility_flags & (1 << static_cast<uint32_t>(slot.slot_type))) != 0) {
                        slot_filled = true;
                        break;
                    }
                }
                if (!slot_filled) {
                    result.error_count++;
                    result.is_valid = false;
                    result.validation_score -= 0.1f;
                }
            }
        }

        // Check performance parameters are reasonable
        if (blueprint.performance.max_speed_kmh <= 0.0f || blueprint.performance.max_speed_kmh > 200.0f) {
            result.warning_count++;
            result.validation_score -= 0.05f;
        }

        if (blueprint.performance.crew_size <= 0.0f || blueprint.performance.crew_size > 20.0f) {
            result.error_count++;
            result.is_valid = false;
            result.validation_score -= 0.1f;
        }

        // Check component compatibility
        for (const auto& component : blueprint.compatible_components) {
            bool compatible_slot_exists = false;
            for (const auto& slot : blueprint.component_slots) {
                if ((component.compatibility_flags & (1 << static_cast<uint32_t>(slot.slot_type))) != 0) {
                    compatible_slot_exists = true;
                    break;
                }
            }
            if (!compatible_slot_exists) {
                result.warning_count++;
                result.validation_score -= 0.02f;
            }
        }

        result.validation_score = std::max(0.0f, result.validation_score);
        return result.is_valid;
    }

    void initialize_template_blueprints() {
        // Initialize with some basic templates
        VehicleBlueprint tank_template = {};
        tank_template.blueprint_id = generate_unique_id();
        strcpy(tank_template.blueprint_name, "Basic Tank Template");
        tank_template.vehicle_class = VehicleClass::TANK;
        tank_template.nation = Nation::USSR;
        tank_template.era = Era::WW2;

        // Add basic slots
        ComponentSlot engine_slot = {};
        engine_slot.slot_id = 1;
        strcpy(engine_slot.slot_name, "Engine Compartment");
        engine_slot.slot_type = ComponentSlotType::ENGINE;
        engine_slot.is_required = true;
        engine_slot.is_unique = true;
        tank_template.component_slots.push_back(engine_slot);

        ComponentSlot turret_slot = {};
        turret_slot.slot_id = 2;
        strcpy(turret_slot.slot_name, "Turret Mount");
        turret_slot.slot_type = ComponentSlotType::TURRET;
        turret_slot.is_required = true;
        turret_slot.is_unique = true;
        tank_template.component_slots.push_back(turret_slot);

        vehicle_templates_.push_back(tank_template);
    }
};

VehicleEditor::VehicleEditor() : impl_(new Impl()) {
    impl_->initialize_template_blueprints();
}

VehicleEditor::~VehicleEditor() {
    delete impl_;
}

bool VehicleEditor::create_new_blueprint(const VehicleBlueprint& template_spec, VehicleEditorState& editor_state) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Copy template to current blueprint
    editor_state.current_blueprint = template_spec;
    editor_state.current_blueprint.blueprint_id = impl_->generate_unique_id();

    // Initialize editor state
    editor_state.component_previews.clear();
    editor_state.modification_history.clear();
    editor_state.selected_slot = 0;
    editor_state.hovered_component = 0;
    editor_state.is_validating = false;
    editor_state.validation_progress = 0.0f;

    // Set default camera position
    float camera_pos[8] = {10.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    editor_state.editor_camera_position = _mm256_loadu_ps(camera_pos);

    float camera_rot[8] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    editor_state.editor_camera_rotation = _mm256_loadu_ps(camera_rot);

    // Validate initial blueprint
    validate_blueprint(editor_state, editor_state.current_blueprint.validation_result);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Created new blueprint '" << editor_state.current_blueprint.blueprint_name
              << "' in " << duration.count() << " ms" << std::endl;

    return duration.count() < 100; // Check performance target
}

bool VehicleEditor::load_blueprint(uint32_t blueprint_id, VehicleEditorState& editor_state) {
    // Find blueprint in loaded blueprints
    auto it = std::find_if(impl_->loaded_blueprints_.begin(), impl_->loaded_blueprints_.end(),
                          [blueprint_id](const VehicleBlueprint& bp) { return bp.blueprint_id == blueprint_id; });

    if (it != impl_->loaded_blueprints_.end()) {
        editor_state.current_blueprint = *it;
        return true;
    }

    // Try to load from file (placeholder implementation)
    std::string filename = "blueprint_" + std::to_string(blueprint_id) + ".bin";
    std::ifstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(&editor_state.current_blueprint), sizeof(VehicleBlueprint));
        impl_->loaded_blueprints_.push_back(editor_state.current_blueprint);
        return true;
    }

    return false;
}

bool VehicleEditor::save_blueprint(const VehicleEditorState& editor_state) {
    std::string filename = "blueprint_" + std::to_string(editor_state.current_blueprint.blueprint_id) + ".bin";
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&editor_state.current_blueprint), sizeof(VehicleBlueprint));
        return true;
    }
    return false;
}

bool VehicleEditor::export_blueprint(const VehicleEditorState& editor_state, const char* export_path) {
    // Placeholder export implementation
    // In real implementation, this would convert to the modular vehicle system format
    std::ofstream file(export_path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&editor_state.current_blueprint), sizeof(VehicleBlueprint));
        return true;
    }
    return false;
}

bool VehicleEditor::add_component_to_slot(VehicleEditorState& editor_state, uint32_t slot_id, uint32_t component_id) {
    // Find the slot
    auto slot_it = std::find_if(editor_state.current_blueprint.component_slots.begin(),
                               editor_state.current_blueprint.component_slots.end(),
                               [slot_id](const ComponentSlot& slot) { return slot.slot_id == slot_id; });

    if (slot_it == editor_state.current_blueprint.component_slots.end()) {
        return false;
    }

    // Find the component
    auto comp_it = std::find_if(editor_state.current_blueprint.compatible_components.begin(),
                               editor_state.current_blueprint.compatible_components.end(),
                               [component_id](const CompatibleComponent& comp) { return comp.component_id == component_id; });

    if (comp_it == editor_state.current_blueprint.compatible_components.end()) {
        return false;
    }

    // Check compatibility
    if ((comp_it->compatibility_flags & (1 << static_cast<uint32_t>(slot_it->slot_type))) == 0) {
        return false;
    }

    // Add to modification history
    ModificationHistory history = {};
    history.action_id = impl_->generate_unique_id();
    strcpy(history.action_description, "Added component to slot");
    history.timestamp = static_cast<uint32_t>(std::time(nullptr));
    editor_state.modification_history.push_back(history);

    // Update validation
    validate_blueprint(editor_state, editor_state.current_blueprint.validation_result);

    return true;
}

bool VehicleEditor::remove_component_from_slot(VehicleEditorState& editor_state, uint32_t slot_id) {
    // Implementation for removing component
    // Add to modification history
    ModificationHistory history = {};
    history.action_id = impl_->generate_unique_id();
    strcpy(history.action_description, "Removed component from slot");
    history.timestamp = static_cast<uint32_t>(std::time(nullptr));
    editor_state.modification_history.push_back(history);

    // Update validation
    validate_blueprint(editor_state, editor_state.current_blueprint.validation_result);

    return true;
}

bool VehicleEditor::validate_blueprint(const VehicleEditorState& editor_state, VehicleValidationResult& result) {
    auto start_time = std::chrono::high_resolution_clock::now();

    bool valid = impl_->validate_blueprint_internal(editor_state.current_blueprint, result);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Validated blueprint in " << duration.count() << " ms" << std::endl;

    return duration.count() < 5; // Check performance target
}

bool VehicleEditor::undo_last_action(VehicleEditorState& editor_state) {
    if (editor_state.modification_history.empty()) {
        return false;
    }

    // Remove last action
    editor_state.modification_history.pop_back();

    // Re-validate
    validate_blueprint(editor_state, editor_state.current_blueprint.validation_result);

    return true;
}

bool VehicleEditor::redo_last_action(VehicleEditorState& editor_state) {
    // Placeholder - would need to store undone actions
    return false;
}

bool VehicleEditor::update_editor_camera(VehicleEditorState& editor_state, const __m256& position, const __m256& rotation) {
    editor_state.editor_camera_position = position;
    editor_state.editor_camera_rotation = rotation;
    return true;
}

bool VehicleEditor::load_vehicle_templates(std::vector<VehicleBlueprint>& templates) {
    templates = impl_->vehicle_templates_;
    return true;
}

bool VehicleEditor::create_template_from_blueprint(const VehicleBlueprint& blueprint, const char* template_name) {
    VehicleBlueprint new_template = blueprint;
    strcpy(new_template.blueprint_name, template_name);
    new_template.blueprint_id = impl_->generate_unique_id();
    impl_->vehicle_templates_.push_back(new_template);
    return true;
}

} // namespace content_editor