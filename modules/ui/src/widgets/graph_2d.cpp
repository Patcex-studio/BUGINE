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
#include "widgets/graph_2d.h"
#include <imgui.h>

namespace ui {

Graph2D::Graph2D(const std::vector<float>& data,
                 const std::string& label,
                 float scaleMin,
                 float scaleMax,
                 ImVec2 plotSize)
    : data_(data),
      label_(label),
      scale_min_(scaleMin),
      scale_max_(scaleMax),
      plot_size_(plotSize) {}

void Graph2D::Draw(const WidgetContext& ctx) {
    ImGui::Begin("Graph 2D", nullptr);
    ImGui::PlotLines(label_.c_str(), data_.data(), static_cast<int>(data_.size()), 0, nullptr, scale_min_, scale_max_, plot_size_);
    ImGui::End();
}

} // namespace ui