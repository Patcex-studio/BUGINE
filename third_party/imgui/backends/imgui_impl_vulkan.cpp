#include "imgui_impl_vulkan.h"
#include "imgui.h"

namespace ImGui {

bool ImGui_ImplVulkan_Init(ImGui_ImplVulkan_InitInfo* info, VkRenderPass render_pass, uint32_t subpass) {
    (void)info;
    (void)render_pass;
    (void)subpass;
    return true;
}

void ImGui_ImplVulkan_Shutdown() {
}

void ImGui_ImplVulkan_RenderDrawData(void* draw_data, VkCommandBuffer command_buffer) {
    (void)draw_data;
    (void)command_buffer;
}

void ImGui_ImplVulkan_CreateFontsTexture(VkCommandBuffer command_buffer) {
    (void)command_buffer;
}

void ImGui_ImplVulkan_DestroyFontUploadObjects() {
}

} // namespace ImGui
