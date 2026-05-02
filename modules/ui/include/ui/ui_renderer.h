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

#include <vulkan/vulkan.h>

namespace ui {

class UIRenderer {
public:
    UIRenderer() = default;
    ~UIRenderer();

    bool Initialize(VkInstance instance,
                    VkPhysicalDevice physicalDevice,
                    VkDevice device,
                    uint32_t graphicsQueueFamily,
                    VkQueue graphicsQueue,
                    VkRenderPass uiRenderPass,
                    uint32_t subpassIndex);

    bool UploadFonts(VkQueue graphicsQueue);
    void Render(VkCommandBuffer commandBuffer);
    void Shutdown();
    bool IsInitialized() const noexcept;

private:
    bool CreateDescriptorPool();
    bool CreateCommandPool(uint32_t graphicsQueueFamily);

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
};

} // namespace ui
