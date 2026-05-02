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
struct ComponentSlot;
struct AttachmentPoint;
struct CompatibleComponent;
struct UpgradeSlot;
struct VehicleValidationResult;
struct ValidationError;
struct ComponentPreview;
struct ModificationHistory;

// Vehicle classes
enum class VehicleClass : uint32_t {
    TANK = 0,
    APC = 1,
    ARTILLERY = 2,
    AIRCRAFT = 3,
    INFANTRY = 4,
    COUNT
};

// Nations
enum class Nation : uint32_t {
    USSR = 0,
    GERMANY = 1,
    USA = 2,
    UK = 3,
    FRANCE = 4,
    JAPAN = 5,
    COUNT
};

// Historical eras
enum class Era : uint32_t {
    WW1 = 0,
    WW2 = 1,
    COLD_WAR = 2,
    MODERN = 3,
    FUTURE = 4,
    COUNT
};

// Component slot types
enum class ComponentSlotType : uint32_t {
    ENGINE = 0,
    WEAPON = 1,
    TURRET = 2,
    TRACK = 3,
    HULL = 4,
    AMMO = 5,
    FUEL = 6,
    CREW = 7,
    COUNT
};

// Core vehicle editor data structure
struct alignas(32) VehicleBlueprint {
    uint32_t blueprint_id;          // Unique blueprint identifier
    char blueprint_name[128];       // Human-readable name
    VehicleClass vehicle_class;     // Vehicle class
    Nation nation;                  // Nation
    Era era;                        // Historical period

    // Component structure
    std::vector<ComponentSlot> component_slots; // Pre-defined component slots
    std::vector<AttachmentPoint> attachment_points; // Physical connection points
    std::vector<CompatibleComponent> compatible_components; // Allowed components
    std::vector<UpgradeSlot> upgrade_slots; // Upgrade installation slots

    // Performance parameters
    struct alignas(32) PerformanceSpecs {
        float max_speed_kmh;        // Maximum speed
        float acceleration_ms2;     // Acceleration rate
        float max_armor_mm;         // Maximum armor value
        float main_gun_caliber_mm;  // Main weapon caliber
        float crew_size;            // Required crew
        float operational_range_km; // Operational range
    } performance;

    // Validation state
    VehicleValidationResult validation_result; // Current validation status
    std::vector<ValidationError> validation_errors; // Any validation errors
    bool is_valid_for_gameplay;    // Can be used in gameplay
    bool is_historically_accurate; // Matches historical specifications
};

// Component slot definition
struct alignas(32) ComponentSlot {
    uint32_t slot_id;              // Unique slot identifier
    char slot_name[64];            // Human-readable slot name
    ComponentSlotType slot_type;   // Component type
    uint32_t compatibility_mask;   // Compatible component types
    __m256 local_position;         // Position relative to vehicle center (x,y,z,w)
    __m256 local_rotation;         // Rotation relative to vehicle center (rx,ry,rz,rw)
    bool is_required;              // Must be filled for valid vehicle
    bool is_unique;                // Only one of this type allowed
    float installation_time_hours; // Time to install component
    float installation_cost;       // Cost to install component
};

// Attachment point for physical connections
struct alignas(32) AttachmentPoint {
    uint32_t point_id;
    char point_name[64];
    __m256 position;
    __m256 rotation;
    uint32_t compatible_slot_types;
    bool is_occupied;
    uint32_t attached_component_id;
};

// Compatible component definition
struct CompatibleComponent {
    uint32_t component_id;
    char component_name[128];
    ComponentSlotType component_type;
    uint32_t compatibility_flags;
    float weight_kg;
    float cost;
    bool is_historical;
};

// Upgrade slot for modifications
struct UpgradeSlot {
    uint32_t upgrade_id;
    char upgrade_name[64];
    uint32_t required_slot_type;
    float upgrade_cost;
    float installation_time_hours;
    bool is_installed;
};

// Validation result
struct VehicleValidationResult {
    bool is_valid;
    uint32_t error_count;
    uint32_t warning_count;
    float validation_score; // 0.0 to 1.0
};

// Validation error
struct ValidationError {
    uint32_t error_code;
    char error_message[256];
    uint32_t affected_component_id;
    uint32_t severity; // 0=warning, 1=error, 2=critical
};

// Component preview for editor
struct ComponentPreview {
    uint32_t component_id;
    __m256 preview_position;
    __m256 preview_rotation;
    float opacity;
    bool is_selected;
};

// Modification history for undo/redo
struct ModificationHistory {
    uint32_t action_id;
    char action_description[128];
    uint32_t timestamp;
    std::vector<uint8_t> before_state;
    std::vector<uint8_t> after_state;
};

// Real-time editing state
struct alignas(32) VehicleEditorState {
    VehicleBlueprint current_blueprint; // Current blueprint being edited
    std::vector<ComponentPreview> component_previews; // Preview of installed components
    std::vector<ModificationHistory> modification_history; // Undo/redo history
    __m256 editor_camera_position; // Editor camera position
    __m256 editor_camera_rotation; // Editor camera rotation
    uint32_t selected_slot;       // Currently selected slot
    uint32_t hovered_component;   // Component currently under mouse
    bool is_validating;          // Currently validating blueprint
    float validation_progress;    // Validation progress percentage
};

// Vehicle Editor main class
class VehicleEditor {
public:
    VehicleEditor();
    ~VehicleEditor();

    // Blueprint management
    bool create_new_blueprint(const VehicleBlueprint& template_spec, VehicleEditorState& editor_state);
    bool load_blueprint(uint32_t blueprint_id, VehicleEditorState& editor_state);
    bool save_blueprint(const VehicleEditorState& editor_state);
    bool export_blueprint(const VehicleEditorState& editor_state, const char* export_path);

    // Component management
    bool add_component_to_slot(VehicleEditorState& editor_state, uint32_t slot_id, uint32_t component_id);
    bool remove_component_from_slot(VehicleEditorState& editor_state, uint32_t slot_id);
    bool validate_blueprint(const VehicleEditorState& editor_state, VehicleValidationResult& result);

    // Editor state management
    bool undo_last_action(VehicleEditorState& editor_state);
    bool redo_last_action(VehicleEditorState& editor_state);
    bool update_editor_camera(VehicleEditorState& editor_state, const __m256& position, const __m256& rotation);

    // Template management
    bool load_vehicle_templates(std::vector<VehicleBlueprint>& templates);
    bool create_template_from_blueprint(const VehicleBlueprint& blueprint, const char* template_name);

private:
    class Impl;
    Impl* impl_;
};

} // namespace content_editor