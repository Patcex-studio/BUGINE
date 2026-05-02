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
#include "widgets/theme_manager.h"
#include <imgui.h>

namespace ui {

Theme ThemeManager::current_theme_;

void ThemeManager::Load(const std::string& themeName) {
    if (themeName == "dark") {
        current_theme_.colors["WindowBg"] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        current_theme_.colors["Text"] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        // Add more colors
    }
    // For simplicity, only dark theme
}

void ThemeManager::Apply() {
    ImGui::ImGuiStyle& style = ImGui::GetStyle();
    for (const auto& [name, color] : current_theme_.colors) {
        if (name == "WindowBg") style.Colors[ImGuiCol_WindowBg] = color;
        else if (name == "Text") style.Colors[ImGuiCol_Text] = color;
        else if (name == "Header") style.Colors[ImGuiCol_Header] = color;
        else if (name == "HeaderHovered") style.Colors[ImGuiCol_HeaderHovered] = color;
        else if (name == "HeaderActive") style.Colors[ImGuiCol_HeaderActive] = color;
        else if (name == "FrameBg") style.Colors[ImGuiCol_FrameBg] = color;
    }
}

} // namespace ui