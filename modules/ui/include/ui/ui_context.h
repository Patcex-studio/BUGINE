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

#include <memory>
#include <vulkan/vulkan.h>

#include "ui_types.h"
#include "ui_config.h"

struct GLFWwindow;

namespace ui {

class IPanel;
class ServiceLocator;

class UIContext {
public:
    UIContext(const UIConfig& config = {});
    ~UIContext();

    UIContext(const UIContext&) = delete;
    UIContext& operator=(const UIContext&) = delete;

    bool Initialize(GLFWwindow* window,
                    VkInstance instance,
                    VkDevice device,
                    VkPhysicalDevice physicalDevice,
                    uint32_t graphicsQueueFamily,
                    VkQueue graphicsQueue,
                    VkRenderPass uiRenderPass,
                    uint32_t subpassIndex);

    void Shutdown();

    bool IsInitialized() const;

    // Frame management
    void BeginFrame();
    void EndFrame();
    void RenderUI(VkCommandBuffer commandBuffer);

    // Panel management
    void AddPanel(std::unique_ptr<IPanel> panel);
    void RemovePanel(IPanel* panel);

    // Update panels (call before render)
    void UpdatePanels(float deltaTime);

    // Render panels (call after ImGui::NewFrame)
    void RenderPanels();

    // Access to service locator
    ServiceLocator& GetServiceLocator();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace ui
