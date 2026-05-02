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
#include "ui/ui_renderer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <array>

namespace ui {

UIRenderer::~UIRenderer() {
    Shutdown();
}

bool UIRenderer::CreateDescriptorPool() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    std::array<VkDescriptorPoolSize, 1> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[0].descriptorCount = 1000;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    return vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptorPool_) == VK_SUCCESS;
}

bool UIRenderer::CreateCommandPool(uint32_t graphicsQueueFamily) {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = graphicsQueueFamily;

    return vkCreateCommandPool(device_, &pool_info, nullptr, &commandPool_) == VK_SUCCESS;
}

bool UIRenderer::Initialize(VkInstance instance,
                            VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            uint32_t graphicsQueueFamily,
                            VkQueue graphicsQueue,
                            VkRenderPass uiRenderPass,
                            uint32_t subpassIndex) {
    if (!instance || !physicalDevice || !device || uiRenderPass == VK_NULL_HANDLE) {
        return false;
    }

    instance_ = instance;
    physicalDevice_ = physicalDevice;
    device_ = device;

    if (!CreateDescriptorPool()) {
        return false;
    }

    if (!CreateCommandPool(graphicsQueueFamily)) {
        return false;
    }

    ImGui::ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physicalDevice_;
    init_info.Device = device_;
    init_info.QueueFamily = graphicsQueueFamily;
    init_info.Queue = graphicsQueue;
    init_info.DescriptorPool = descriptorPool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.CheckVkResultFn = [](VkResult result) {
        if (result != VK_SUCCESS) {
            // Logging backend can be added later.
        }
    };

    if (!ImGui::ImGui_ImplVulkan_Init(&init_info, uiRenderPass, subpassIndex)) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool UIRenderer::UploadFonts(VkQueue graphicsQueue) {
    if (!initialized_ || commandPool_ == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE) {
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = commandPool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device_, &alloc_info, &commandBuffer) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &begin_info) != VK_SUCCESS) {
        return false;
    }

    ImGui::ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        return false;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        return false;
    }
    vkQueueWaitIdle(graphicsQueue);

    ImGui::ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
    return true;
}

void UIRenderer::Render(VkCommandBuffer commandBuffer) {
    if (!initialized_ || commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    ImGui::Render();
    ImGui::ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void UIRenderer::Shutdown() {
    if (initialized_) {
        ImGui::ImGui_ImplVulkan_Shutdown();
        initialized_ = false;
    }

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

bool UIRenderer::IsInitialized() const noexcept {
    return initialized_;
}

} // namespace ui
