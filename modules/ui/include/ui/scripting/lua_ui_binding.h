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

#include <string>
#include <functional>
#include <any>
#include <vector>

#ifdef UI_ENABLE_LUA
#include <sol/sol.hpp>
#endif

struct lua_State;

namespace ui::scripting {

// Тип команды UI из Lua
enum class UICommandType {
    BEGIN_WINDOW,
    END_WINDOW,
    TEXT,
    BUTTON,
    SLIDER_FLOAT,
    // ... расширяем по необходимости
};

struct UICommand {
    UICommandType type;
    std::any payload;   // данные команды
};

class LuaUIBinding {
public:
#ifdef UI_ENABLE_LUA
    // Регистрирует все UI-функции в переданном Lua-стейте
    static void Register(sol::state& lua);
#else
    static void Register();
#endif

    // Буфер команд, заполняемый из Lua
    static void PushCommand(UICommand&& cmd);
    static void ExecuteBufferedCommands();   // вызывается в главном потоке перед рендером UI
};

} // namespace ui::scripting