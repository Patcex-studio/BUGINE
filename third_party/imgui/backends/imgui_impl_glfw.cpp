#include "imgui_impl_glfw.h"
#include "imgui.h"

namespace ImGui {

bool ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window, bool install_callbacks) {
    (void)window;
    (void)install_callbacks;
    return true;
}

void ImGui_ImplGlfw_Shutdown() {
}

void ImGui_ImplGlfw_NewFrame() {
}

} // namespace ImGui
