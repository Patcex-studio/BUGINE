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
#include "ui/panels/performance_panel.h"
#include "ui/service_locator.h"
#include <rendering_engine/rendering_engine.h>
#include <imgui.h>

namespace ui::panels {

void PerformancePanel::OnAttach(const ServiceLocator& locator) {
    renderingEngine_ = locator.GetRenderingEngine();
    frameTimeHistory_.resize(300, 0.0f);
}

void PerformancePanel::OnDetach() {
    renderingEngine_ = nullptr;
}

void PerformancePanel::OnUpdate(float deltaTime) {
    if (!renderingEngine_) return;
    // Assume RenderingEngine has get_performance_metrics() returning struct with frame_time_ms
    auto metrics = renderingEngine_->get_performance_metrics();
    frameTimeHistory_[historyIndex_] = metrics.frame_time_ms;
    historyIndex_ = (historyIndex_ + 1) % frameTimeHistory_.size();
}

void PerformancePanel::OnUIRender() {
    if (!ImGui::Begin("Performance", &visible_)) {
        ImGui::End();
        return;
    }

    if (renderingEngine_) {
        auto metrics = renderingEngine_->get_performance_metrics();
        ImGui::Text("FPS: %.1f", metrics.fps);
        ImGui::Text("Draw calls: %u", metrics.draw_calls);
        ImGui::PlotLines("Frame Time (ms)", frameTimeHistory_.data(), static_cast<int>(frameTimeHistory_.size()), static_cast<int>(historyIndex_), nullptr, 0.0f, 50.0f, ImVec2(0, 80));
    } else {
        ImGui::Text("RenderingEngine not available");
    }

    ImGui::End();
}

UILayer PerformancePanel::GetLayer() const {
    return UILayer::GameUI;
}

bool PerformancePanel::IsVisible() const {
    return visible_;
}

void PerformancePanel::SetVisible(bool visible) {
    visible_ = visible;
}

} // namespace ui::panels