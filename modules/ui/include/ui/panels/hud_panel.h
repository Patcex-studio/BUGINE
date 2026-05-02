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

#include "ui/panels/i_panel.h"
#include "widgets/minimap.h"
#include <glm/glm.hpp>
#include <vector>

namespace rendering_engine { class RenderingEngine; class CameraController; struct Camera; }
namespace physics_core { class PhysicsCore; }

namespace ui::panels {

class HUDPanel : public IPanel {
public:
    HUDPanel();

    void OnAttach(const ServiceLocator& locator) override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnUIRender() override;
    UILayer GetLayer() const override;
    bool IsVisible() const override;
    void SetVisible(bool visible) override;

private:
    bool ProjectWorldToScreen(const rendering_engine::Camera& camera,
                              const glm::vec3& worldPos,
                              ImVec2& outScreen) const;

    void DrawCrosshair() const;
    void DrawHealthBars(const rendering_engine::Camera& camera) const;
    void DrawMinimap();

    rendering_engine::RenderingEngine* rendering_engine_ = nullptr;
    rendering_engine::CameraController* camera_controller_ = nullptr;
    physics_core::PhysicsCore* physics_ = nullptr;
    bool visible_ = true;

    std::vector<ui::Marker> minimap_markers_;
    ui::Minimap minimap_widget_;
    ImTextureID map_texture_id_ = nullptr;
};

} // namespace ui::panels
