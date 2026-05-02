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
#include "widgets/console.h"
#include <imgui.h>

namespace ui {

Console::Console(std::function<void(const std::string&)> onCommand)
    : on_command_(onCommand) {
    input_.reserve(256);
}

void Console::AddLog(const std::string& message, ImU32 color) {
    if (logs_.size() >= kMaxLogEntries) {
        logs_.erase(logs_.begin());
    }
    logs_.push_back({message, color});
}

void Console::Draw(const WidgetContext& ctx) {
    ImGui::Begin("Console", nullptr);

    ImGui::BeginChild("Log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    for (const auto& log : logs_) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", log.message.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    if (ImGui::InputText("Command", &input_, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!input_.empty()) {
            AddLog(std::string("> ") + input_, IM_COL32(200, 200, 200, 255));
            if (on_command_) on_command_(input_);
            input_.clear();
        }
    }

    ImGui::End();
}

} // namespace ui