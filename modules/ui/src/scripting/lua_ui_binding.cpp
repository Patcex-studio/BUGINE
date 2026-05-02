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
#include "ui/scripting/lua_ui_binding.h"
#include "ui/scripting/ui_event_system.h"
#include "ui/command_queue.h"
#include "imgui.h"  // assuming ImGui is included

#ifdef UI_ENABLE_LUA
#include <sol/sol.hpp>
#endif

namespace ui::scripting {

#ifdef UI_ENABLE_LUA
void LuaUIBinding::Register(sol::state& lua) {
    auto ui_table = lua.create_table();

    ui_table["BeginWindow"] = [](const std::string& title) {
        ImGui::Begin(title.c_str());
    };

    ui_table["EndWindow"] = []() {
        ImGui::End();
    };

    ui_table["Text"] = [](const std::string& text) {
        ImGui::Text("%s", text.c_str());
    };

    ui_table["Button"] = [](const std::string& label) -> bool {
        return ImGui::Button(label.c_str());
    };

    ui_table["SliderFloat"] = [](const std::string& label, float min, float max, float value) -> std::tuple<bool, float> {
        bool changed = ImGui::SliderFloat(label.c_str(), &value, min, max);
        return {changed, value};
    };

    ui_table["On"] = [&lua](const std::string& event, sol::function callback) {
        int ref = lua.registry().create(callback);
        UIEventSystem::Subscribe(event, ref);
    };

    lua["ui"] = ui_table;

    auto core_table = lua.create_table();
    core_table["command"] = [](const std::string& cmd, sol::table args) {
        Command command;
        if (cmd == "spawn") {
            command.type = CommandType::SPAWN_ENTITY;
            std::string type = args["type"];
            float x = args.get_or("x", 0.0f);
            float y = args.get_or("y", 0.0f);
            float z = args.get_or("z", 0.0f);
            command.payload = std::make_tuple(type, x, y, z);
        }
        CommandQueue::Instance().Push(std::move(command));
    };
    lua["core"] = core_table;
}
#else
void LuaUIBinding::Register() {
    // Lua scripting unavailable in this build.
}
#endif

void LuaUIBinding::PushCommand(UICommand&& cmd) {
    (void)cmd;
}

void LuaUIBinding::ExecuteBufferedCommands() {
}

} // namespace ui::scripting
