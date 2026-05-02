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

#include <imgui.h>
#include <string>
#include <unordered_map>

namespace ui {

struct Theme {
    std::unordered_map<std::string, ImVec4> colors;
    // Add fonts, sizes if needed
};

class ThemeManager {
public:
    static void Load(const std::string& themeName);
    static void Apply();

private:
    static Theme current_theme_;
};

} // namespace ui