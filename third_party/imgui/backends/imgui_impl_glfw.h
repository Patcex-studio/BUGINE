#pragma once

struct GLFWwindow;

namespace ImGui {

bool ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window, bool install_callbacks);
void ImGui_ImplGlfw_Shutdown();
void ImGui_ImplGlfw_NewFrame();

} // namespace ImGui
