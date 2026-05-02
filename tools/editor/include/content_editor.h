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

// Content Editor System Main Header
// Provides unified access to Vehicle Editor, Mission Designer, and Scripting API

#include "content_editor/vehicle_editor.h"
#include "content_editor/mission_designer.h"
#include "content_editor/scripting_api.h"

namespace content_editor {

// Library version
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

// Main editor interface
class ContentEditor {
public:
    ContentEditor();
    ~ContentEditor();

    // Initialize the editor system
    bool initialize();

    // Shutdown the editor system
    void shutdown();

    // Get subsystems
    VehicleEditor& get_vehicle_editor();
    MissionDesigner& get_mission_designer();
    ScriptingAPI& get_scripting_api();

private:
    class Impl;
    Impl* impl_;
};

} // namespace content_editor