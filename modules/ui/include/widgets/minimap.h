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

#include "i_widget.h"
#include <vector>
#include <string>
#include <imgui.h>

namespace ui {

struct Marker {
    float x, y; // Normalized 0-1
    ImVec4 color;
    float radius = 4.0f;
};

class Minimap : public IWidget {
public:
    Minimap(ImTextureID mapTextureId,
            const std::vector<Marker>& markers,
            ImVec2 worldBoundsMin = ImVec2(0.0f, 0.0f),
            ImVec2 worldBoundsMax = ImVec2(1.0f, 1.0f),
            ImVec2 displaySize = ImVec2(256, 256),
            const char* windowTitle = "Minimap",
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None);

    void Draw(const WidgetContext& ctx) override;
    void SetMarkers(const std::vector<Marker>& markers);
    void SetTexture(ImTextureID mapTextureId);
    void SetWindowTitle(const char* title);
    void SetWindowFlags(ImGuiWindowFlags flags);
    void SetWorldBounds(ImVec2 minBounds, ImVec2 maxBounds);
    void SetDisplaySize(ImVec2 size);

private:
    std::string window_title_;
    ImGuiWindowFlags window_flags_;
    ImTextureID map_texture_id_;
    std::vector<Marker> markers_;
    ImVec2 world_bounds_min_;
    ImVec2 world_bounds_max_;
    ImVec2 display_size_;
};

} // namespace ui