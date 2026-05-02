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
#include "ui/panels/hud_panel.h"
#include "ui/service_locator.h"
#include <algorithm>
#include <imgui.h>
#include <cfloat>
#include <physics_core/physics_core.h>
#include <rendering_engine/cameras/camera_controller.h>

namespace ui::panels {

HUDPanel::HUDPanel()
    : minimap_widget_(nullptr,
                      minimap_markers_,
                      ImVec2(0.0f, 0.0f),
                      ImVec2(1.0f, 1.0f),
                      ImVec2(200.0f, 200.0f),
                      "HUD Minimap",
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove) {
    minimap_markers_.reserve(128);
}

void HUDPanel::OnAttach(const ServiceLocator& locator) {
    physics_ = locator.GetPhysics();
    rendering_engine_ = locator.GetRenderingEngine();
    camera_controller_ = locator.GetCameraController();
}

void HUDPanel::OnDetach() {
    physics_ = nullptr;
    rendering_engine_ = nullptr;
    camera_controller_ = nullptr;
}

void HUDPanel::OnUpdate(float deltaTime) {
    minimap_markers_.clear();

    if (!physics_) {
        minimap_widget_.SetMarkers(minimap_markers_);
        return;
    }

    float min_x = FLT_MAX;
    float max_x = -FLT_MAX;
    float min_z = FLT_MAX;
    float max_z = -FLT_MAX;

    const auto& bodies = physics_->get_rigid_bodies();
    for (const auto& body : bodies) {
        if (body.health <= 0.0f) {
            continue;
        }

        const float world_x = body.position.x;
        const float world_z = body.position.z;
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
    }

    if (min_x >= max_x || min_z >= max_z) {
        min_x = -50.0f;
        max_x = 50.0f;
        min_z = -50.0f;
        max_z = 50.0f;
    }

    const float width = max_x - min_x;
    const float height = max_z - min_z;
    minimap_markers_.reserve(bodies.size());
    for (const auto& body : bodies) {
        if (body.health <= 0.0f) {
            continue;
        }

        const float normalized_x = width > 0.0f ? (body.position.x - min_x) / width : 0.5f;
        const float normalized_y = height > 0.0f ? (body.position.z - min_z) / height : 0.5f;

        ui::Marker marker;
        marker.x = normalized_x;
        marker.y = normalized_y;
        marker.radius = 4.0f;
        marker.color = ImVec4(1.0f - body.health, body.health, 0.0f, 1.0f);
        minimap_markers_.push_back(marker);
    }

    minimap_widget_.SetMarkers(minimap_markers_);
    minimap_widget_.SetWorldBounds(ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
}

void HUDPanel::OnUIRender() {
    if (!visible_) {
        return;
    }

    DrawCrosshair();

    if (camera_controller_) {
        DrawHealthBars(camera_controller_->GetCamera());
    }

    DrawMinimap();
}

UILayer HUDPanel::GetLayer() const {
    return UILayer::HUD;
}

bool HUDPanel::IsVisible() const {
    return visible_;
}

void HUDPanel::SetVisible(bool visible) {
    visible_ = visible;
}

bool HUDPanel::ProjectWorldToScreen(const rendering_engine::Camera& camera,
                                    const glm::vec3& worldPos,
                                    ImVec2& outScreen) const {
    const glm::vec4 clipPos = camera.view_projection_matrix * glm::vec4(worldPos, 1.0f);
    if (clipPos.w <= 0.0f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f || ndc.z < 0.0f || ndc.z > 1.0f) {
        return false;
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    outScreen.x = (ndc.x * 0.5f + 0.5f) * displaySize.x;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * displaySize.y;
    return true;
}

void HUDPanel::DrawCrosshair() const {
    const ImDrawList* dummy = nullptr;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    const float gap = 10.0f;
    const float length = 15.0f;
    const ImU32 color = IM_COL32(0, 255, 128, 220);

    drawList->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), color, 2.0f);
    drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), color, 2.0f);
    drawList->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), color, 2.0f);
    drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), color, 2.0f);
}

void HUDPanel::DrawHealthBars(const rendering_engine::Camera& camera) const {
    if (!physics_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const auto& bodies = physics_->get_rigid_bodies();
    for (const auto& body : bodies) {
        if (body.health <= 0.0f) {
            continue;
        }

        ImVec2 screenPos;
        const glm::vec3 worldPos(body.position.x, body.position.y + 1.5f, body.position.z);
        if (!ProjectWorldToScreen(camera, worldPos, screenPos)) {
            continue;
        }

        const float barWidth = 60.0f;
        const float barHeight = 6.0f;
        const float healthRatio = glm::clamp(body.health, 0.0f, 1.0f);
        const ImVec2 barMin(screenPos.x - barWidth * 0.5f, screenPos.y - 35.0f);
        const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);
        const ImVec2 fillMax(barMin.x + barWidth * healthRatio, barMax.y);

        drawList->AddRectFilled(barMin, barMax, IM_COL32(40, 40, 40, 220));
        drawList->AddRectFilled(barMin, fillMax, IM_COL32(0, 192, 64, 230));
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 160));
    }
}

void HUDPanel::DrawMinimap() {
    const ImVec2 size(210.0f, 210.0f);
    const ImVec2 position(10.0f, 10.0f);

    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.20f);
    minimap_widget_.SetDisplaySize(ImVec2(200.0f, 200.0f));
    minimap_widget_.Draw(WidgetContext{});
}

} // namespace ui::panels
