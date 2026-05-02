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

#include "ui/service_locator.h"
#include "ui/ui_types.h"

namespace ui {

class IPanel {
public:
    virtual ~IPanel() = default;

    // Called when adding the panel to UI
    virtual void OnAttach(const ServiceLocator& locator) = 0;

    // Called when removing the panel
    virtual void OnDetach() = 0;

    // Logic, getting data from queues (called every frame before render)
    virtual void OnUpdate(float deltaTime) = 0;

    // ImGui rendering (called every frame, between BeginFrame/EndFrame)
    virtual void OnUIRender() = 0;

    // Panel layer ordering
    virtual UILayer GetLayer() const = 0;

    // Can the panel be closed by the user
    virtual bool IsClosable() const { return true; }

    // Panel visibility
    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool visible) = 0;
};

} // namespace ui