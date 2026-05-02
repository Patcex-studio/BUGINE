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
#include <string>
#include <vector>

namespace physics_core { class PhysicsCore; }
namespace rendering_engine { class ModelSystem; }

namespace ui::panels {

class SceneHierarchy : public IPanel {
public:
    void OnAttach(const ServiceLocator& locator) override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnUIRender() override;
    UILayer GetLayer() const override;
    bool IsVisible() const override;
    void SetVisible(bool visible) override;

private:
    physics_core::PhysicsCore* physics_ = nullptr;
    rendering_engine::ModelSystem* models_ = nullptr;

    struct Entry {
        uint64_t id;
        std::string name;
        uint32_t type;
    };
    std::vector<Entry> entities_;
    bool visible_ = true;
};

} // namespace ui::panels