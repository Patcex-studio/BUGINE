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
#include "widgets/minimap.h"
#include <imgui.h>

namespace ui {

Minimap::Minimap(ImTextureID mapTextureId,
                 const std::vector<Marker>& markers,
                 ImVec2 worldBoundsMin,
                 ImVec2 worldBoundsMax,
                 ImVec2 displaySize,
                 const char* windowTitle,
                 ImGuiWindowFlags windowFlags)
    : window_title_(windowTitle),
      window_flags_(windowFlags),
      map_texture_id_(mapTextureId),
      markers_(markers),
      world_bounds_min_(worldBoundsMin),
      world_bounds_max_(worldBoundsMax),
      display_size_(displaySize) {}

void Minimap::Draw(const WidgetContext& ctx) {
    ImGui::Begin(window_title_.c_str(), nullptr, window_flags_);

    ImGui::BeginChild("MinimapCanvas", display_size_, true);
    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_end(canvas_pos.x + display_size_.x, canvas_pos.y + display_size_.y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 bg_color = IM_COL32(40, 40, 40, 200);
    draw_list->AddRectFilled(canvas_pos, canvas_end, bg_color);

    const float width = world_bounds_max_.x - world_bounds_min_.x;
    const float height = world_bounds_max_.y - world_bounds_min_.y;
    for (const auto& marker : markers_) {
        const float normalized_x = width > 0.0f ? (marker.x - world_bounds_min_.x) / width : 0.0f;
        const float normalized_y = height > 0.0f ? (marker.y - world_bounds_min_.y) / height : 0.0f;
        const ImVec2 marker_pos = ImVec2(canvas_pos.x + normalized_x * display_size_.x,
                                       canvas_pos.y + (1.0f - normalized_y) * display_size_.y);
        const ImU32 marker_color = IM_COL32(
            static_cast<int>(marker.color.x * 255.0f),
            static_cast<int>(marker.color.y * 255.0f),
            static_cast<int>(marker.color.z * 255.0f),
            static_cast<int>(marker.color.w * 255.0f));
        draw_list->AddCircleFilled(marker_pos, marker.radius, marker_color);
    }

    ImGui::EndChild();
    ImGui::End();
}

void Minimap::SetMarkers(const std::vector<Marker>& markers) {
    markers_ = markers;
}

void Minimap::SetTexture(ImTextureID mapTextureId) {
    map_texture_id_ = mapTextureId;
}

void Minimap::SetWindowTitle(const char* title) {
    window_title_ = title ? title : "Minimap";
}

void Minimap::SetWindowFlags(ImGuiWindowFlags flags) {
    window_flags_ = flags;
}

void Minimap::SetWorldBounds(ImVec2 minBounds, ImVec2 maxBounds) {
    world_bounds_min_ = minBounds;
    world_bounds_max_ = maxBounds;
}

void Minimap::SetDisplaySize(ImVec2 size) {
    display_size_ = size;
}

} // namespace ui