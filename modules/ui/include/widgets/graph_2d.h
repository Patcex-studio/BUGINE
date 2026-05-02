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

class Graph2D : public IWidget {
public:
    Graph2D(const std::vector<float>& data,
            const std::string& label = "Graph",
            float scaleMin = 0.0f,
            float scaleMax = 1.0f,
            ImVec2 plotSize = ImVec2(0, 80));

    void Draw(const WidgetContext& ctx) override;

private:
    std::vector<float> data_;
    std::string label_;
    float scale_min_;
    float scale_max_;
    ImVec2 plot_size_;
};

} // namespace ui