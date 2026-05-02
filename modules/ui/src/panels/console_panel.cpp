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
#include "ui/panels/console_panel.h"
#include "ui/service_locator.h"
#include "ui/command_queue.h"
#include <imgui.h>
#include <functional>

namespace scripting_api {
class ScriptingAPI {
public:
    virtual ~ScriptingAPI() = default;
    virtual std::string execute_script(const std::string& script_code) = 0;
};
}

namespace ui::panels {

ConsolePanel::ConsolePanel()
    : consoleWidget_(std::bind(&ConsolePanel::ExecuteCommand, this, std::placeholders::_1)) {
}

void ConsolePanel::OnAttach(const ServiceLocator& locator) {
    scripting_ = locator.GetScriptingAPI();
}

void ConsolePanel::OnDetach() {
    scripting_ = nullptr;
}

void ConsolePanel::OnUpdate(float deltaTime) {
    // Nothing to update
}

void ConsolePanel::OnUIRender() {
    if (!ImGui::Begin("Console", &visible_)) {
        ImGui::End();
        return;
    }

    WidgetContext ctx;
    consoleWidget_.Draw(ctx);

    ImGui::End();
}

UILayer ConsolePanel::GetLayer() const {
    return UILayer::GameUI;
}

void ConsolePanel::ExecuteCommand(const std::string& cmd) {
    if (cmd.rfind("spawn ", 0) == 0) {
        // Extract parameters, e.g., "spawn tank"
        std::string entityType = cmd.substr(6); // "tank"
        Command command{CommandType::SPAWN_ENTITY, entityType};
        CommandQueue::Instance().Push(std::move(command));
        consoleWidget_.AddLog("Spawned: " + entityType);
    } else if (scripting_) {
        // Pass to scripting API
        auto result = scripting_->execute_script(cmd);
        consoleWidget_.AddLog(result);
    } else {
        consoleWidget_.AddLog("Unknown command: " + cmd);
    }
}

bool ConsolePanel::IsVisible() const {
    return visible_;
}

void ConsolePanel::SetVisible(bool visible) {
    visible_ = visible;
}

} // namespace ui::panels