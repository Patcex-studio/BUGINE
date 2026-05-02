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
#include <unordered_map>
#include <vector>

#ifdef UI_ENABLE_LUA
#include <sol/sol.hpp>
#endif

namespace ui::scripting {

class UIEventSystem {
public:
#ifdef UI_ENABLE_LUA
    // Инициализация с Lua-стейтом
    static void Initialize(sol::state& lua);

    // Подписать коллбэк Lua на событие
    static void Subscribe(const std::string& event, int luaRef);

    // Вызвать событие (обычно из движка)
    static void Trigger(const std::string& event, const sol::object& data);

private:
    static sol::state* luaState_;
    static std::unordered_map<std::string, std::vector<int>> listeners_;
#else
    static void Initialize();
    static void Subscribe(const std::string& event, int luaRef);
    static void Trigger(const std::string& event, const std::string& data);
#endif
};

} // namespace ui::scripting