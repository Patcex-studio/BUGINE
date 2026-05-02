#pragma once

#include <vulkan/vulkan.h>

namespace ImGui {

struct ImGui_ImplVulkan_InitInfo {
    VkInstance Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    uint32_t QueueFamily = 0;
    VkQueue Queue = VK_NULL_HANDLE;
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    uint32_t MinImageCount = 0;
    uint32_t ImageCount = 0;
    void (*CheckVkResultFn)(VkResult err) = nullptr;
};

bool ImGui_ImplVulkan_Init(ImGui_ImplVulkan_InitInfo* info, VkRenderPass render_pass, uint32_t subpass);
void ImGui_ImplVulkan_Shutdown();
void ImGui_ImplVulkan_RenderDrawData(void* draw_data, VkCommandBuffer command_buffer);
void ImGui_ImplVulkan_CreateFontsTexture(VkCommandBuffer command_buffer);
void ImGui_ImplVulkan_DestroyFontUploadObjects();

} // namespace ImGui
