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
#include "ui/scripting/ui_event_system.h"
#ifdef UI_ENABLE_LUA
#include <sol/sol.hpp>
#endif

namespace ui::scripting {

#ifdef UI_ENABLE_LUA
sol::state* UIEventSystem::luaState_ = nullptr;
std::unordered_map<std::string, std::vector<int>> UIEventSystem::listeners_;

void UIEventSystem::Initialize(sol::state& lua) {
    luaState_ = &lua;
}

void UIEventSystem::Subscribe(const std::string& event, int luaRef) {
    listeners_[event].push_back(luaRef);
}

void UIEventSystem::Trigger(const std::string& event, const sol::object& data) {
    if (!luaState_) return;
    auto it = listeners_.find(event);
    if (it != listeners_.end()) {
        for (int ref : it->second) {
            sol::function callback = luaState_->registry().get<sol::function>(ref);
            if (callback.valid()) {
                callback(data);
            }
        }
    }
}
#else
void UIEventSystem::Initialize() {
}

void UIEventSystem::Subscribe(const std::string& event, int luaRef) {
    (void)event;
    (void)luaRef;
}

void UIEventSystem::Trigger(const std::string& event, const std::string& data) {
    (void)event;
    (void)data;
}
#endif

} // namespace ui::scripting
