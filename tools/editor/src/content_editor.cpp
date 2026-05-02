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
#include "content_editor/vehicle_editor.h"
#include "content_editor/mission_designer.h"
#include "content_editor/scripting_api.h"

namespace content_editor {

class ContentEditor::Impl {
public:
    VehicleEditor vehicle_editor_;
    MissionDesigner mission_designer_;
    ScriptingAPI scripting_api_;
};

ContentEditor::ContentEditor() : impl_(new Impl()) {}

ContentEditor::~ContentEditor() {
    delete impl_;
}

bool ContentEditor::initialize() {
    // Initialize subsystems
    if (!impl_->scripting_api_.initialize()) {
        return false;
    }

    // Additional initialization can be added here
    return true;
}

void ContentEditor::shutdown() {
    impl_->scripting_api_.shutdown();
}

VehicleEditor& ContentEditor::get_vehicle_editor() {
    return impl_->vehicle_editor_;
}

MissionDesigner& ContentEditor::get_mission_designer() {
    return impl_->mission_designer_;
}

ScriptingAPI& ContentEditor::get_scripting_api() {
    return impl_->scripting_api_;
}

} // namespace content_editor