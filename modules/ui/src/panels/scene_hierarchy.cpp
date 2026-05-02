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
#include "ui/panels/scene_hierarchy.h"
#include "ui/service_locator.h"
#include <algorithm>
#include <imgui.h>
#include <rendering_engine/model_system.h>
#include <physics_core/physics_core.h>
#include <string>

namespace ui::panels {

void SceneHierarchy::OnAttach(const ServiceLocator& locator) {
    physics_ = locator.GetPhysics();
    models_ = locator.GetModelSystem();
}

void SceneHierarchy::OnDetach() {
    physics_ = nullptr;
    models_ = nullptr;
}

void SceneHierarchy::OnUpdate(float deltaTime) {
    entities_.clear();
    if (!physics_) return;

    const auto& bodies = physics_->get_rigid_bodies();
    for (size_t i = 0; i < std::min(bodies.size(), size_t(1000)); ++i) {
        const auto& body = bodies[i];
        uint64_t id = body.entity_id;
        std::string name = "Entity " + std::to_string(id);
        uint32_t type = body.body_type;
        entities_.push_back({id, name, type});
    }
}

void SceneHierarchy::OnUIRender() {
    if (!ImGui::Begin("Scene Hierarchy", &visible_)) {
        ImGui::End();
        return;
    }

    for (const auto& entry : entities_) {
        if (ImGui::TreeNode((void*)(uintptr_t)entry.id, "%s (ID: %llu)", entry.name.c_str(), entry.id)) {
            // On click, could select, but for now just display
            ImGui::Text("Type: %u", entry.type);
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

UILayer SceneHierarchy::GetLayer() const {
    return UILayer::Editor;
}

bool SceneHierarchy::IsVisible() const {
    return visible_;
}

void SceneHierarchy::SetVisible(bool visible) {
    visible_ = visible;
}

} // namespace ui::panels