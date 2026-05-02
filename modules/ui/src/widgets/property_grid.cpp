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
#include "widgets/property_grid.h"
#include <algorithm>
#include <imgui.h>

namespace ui {

PropertyGrid::PropertyGrid(const std::vector<Property>& properties)
    : properties_(properties) {}

void PropertyGrid::Draw(const WidgetContext& ctx) {
    ImGui::Begin("Property Grid", nullptr);

    for (auto& prop : properties_) {
        ImGui::PushID(prop.name.c_str());
        ImGui::TextUnformatted(prop.name.c_str());
        ImGui::SameLine(200.0f);

        if (std::holds_alternative<int>(prop.value)) {
            int val = std::get<int>(prop.value);
            if (!prop.readOnly && ImGui::InputInt("##value", &val)) {
                val = std::clamp(val, static_cast<int>(prop.min), static_cast<int>(prop.max));
                prop.value = val;
                if (prop.on_change) prop.on_change(prop.value);
            } else if (prop.readOnly) {
                ImGui::TextDisabled("%d", val);
            }
        } else if (std::holds_alternative<float>(prop.value)) {
            float val = std::get<float>(prop.value);
            if (!prop.readOnly && ImGui::InputFloat("##value", &val, 0.0f, 0.0f, "%.3f")) {
                val = std::clamp(val, prop.min, prop.max);
                prop.value = val;
                if (prop.on_change) prop.on_change(prop.value);
            } else if (prop.readOnly) {
                ImGui::TextDisabled("%.3f", val);
            }
        } else if (std::holds_alternative<std::string>(prop.value)) {
            std::string val = std::get<std::string>(prop.value);
            if (!prop.readOnly && ImGui::InputText("##value", &val)) {
                prop.value = val;
                if (prop.on_change) prop.on_change(prop.value);
            } else if (prop.readOnly) {
                ImGui::TextDisabled("%s", val.c_str());
            }
        } else if (std::holds_alternative<bool>(prop.value)) {
            bool val = std::get<bool>(prop.value);
            if (!prop.readOnly && ImGui::Checkbox("##value", &val)) {
                prop.value = val;
                if (prop.on_change) prop.on_change(prop.value);
            } else if (prop.readOnly) {
                ImGui::TextDisabled(val ? "true" : "false");
            }
        }

        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace ui