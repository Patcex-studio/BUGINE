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
#include "widgets/unit_list.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace ui {

static std::string ToLowerCopy(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

UnitList::UnitList(const std::vector<UnitEntry>& units,
                   UnitAction onDoubleClick,
                   UnitAction onContextMenu)
    : units_(units),
      on_double_click_(onDoubleClick),
      on_context_menu_(onContextMenu) {
    UpdateFilteredItems();
}

void UnitList::UpdateFilteredItems() {
    filtered_indices_.clear();
    const std::string lower_filter = ToLowerCopy(filter_);
    for (size_t i = 0; i < units_.size(); ++i) {
        const auto& unit = units_[i];
        if (filter_.empty()) {
            filtered_indices_.push_back(i);
            continue;
        }
        const std::string lower_name = ToLowerCopy(unit.name);
        if (lower_name.find(lower_filter) != std::string::npos) {
            filtered_indices_.push_back(i);
        }
    }
    if (selected_index_ >= static_cast<int>(filtered_indices_.size())) {
        selected_index_ = -1;
    }
}

void UnitList::Draw(const WidgetContext& ctx) {
    ImGui::Begin("Unit List", nullptr);

    if (ImGui::InputText("Filter", &filter_)) {
        UpdateFilteredItems();
    }

    if (ImGui::BeginChild("UnitListChild", ImVec2(0, 0), true)) {
        for (size_t idx = 0; idx < filtered_indices_.size(); ++idx) {
            const auto& unit = units_[filtered_indices_[idx]];
            const bool selected = selected_index_ == static_cast<int>(idx);
            ImGui::PushID(static_cast<int>(idx));
            const std::string label = unit.name + " (" + unit.type + ")";
            if (ImGui::Selectable(label.c_str(), selected)) {
                selected_index_ = static_cast<int>(idx);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && on_double_click_) {
                    on_double_click_(unit.id);
                }
            }
            if (ImGui::BeginPopupContextItem("unit_context_menu")) {
                if (ImGui::MenuItem("Select")) {
                    selected_index_ = static_cast<int>(idx);
                    if (on_context_menu_) on_context_menu_(unit.id);
                }
                if (ImGui::MenuItem("Inspect")) {
                    if (on_context_menu_) on_context_menu_(unit.id);
                }
                ImGui::EndPopup();
            }
            if (selected) {
                ImGui::SameLine();
                ImGui::TextDisabled("HP: %u", unit.health);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ui