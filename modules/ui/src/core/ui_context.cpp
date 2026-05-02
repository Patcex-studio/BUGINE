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
#include "ui/ui_context.h"
#include "ui/input_manager.h"
#include "ui/ui_renderer.h"
#include "ui/service_locator.h"
#include "ui/panels/i_panel.h"
#include "ui/scripting/lua_ui_binding.h"
#include "ui/scripting/ui_event_system.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <vulkan/vulkan.h>

#ifdef UI_ENABLE_LUA
#include <sol/sol.hpp>
#endif

namespace ui {

// struct GLFWwindow; // Remove this

class UIContext::Impl {
public:
    UIConfig config;
    bool initialized = false;

    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkRenderPass uiRenderPass = VK_NULL_HANDLE;
    uint32_t subpassIndex = 0;

    ServiceLocator serviceLocator_;
    std::vector<std::unique_ptr<IPanel>> panels_;
#ifdef UI_ENABLE_LUA
    sol::state* luaState_ = nullptr;
#else
    void* luaState_ = nullptr;
#endif

    std::unique_ptr<InputManager> inputManager_;
    std::unique_ptr<UIRenderer> renderer_;

    bool Initialize(GLFWwindow* w,
                    VkInstance inst,
                    VkDevice dev,
                    VkPhysicalDevice pd,
                    uint32_t queueFamily,
                    VkQueue queue,
                    VkRenderPass renderPass,
                    uint32_t subpass) {
        window = w;
        instance = inst;
        device = dev;
        physicalDevice = pd;
        graphicsQueueFamily = queueFamily;
        graphicsQueue = queue;
        uiRenderPass = renderPass;
        subpassIndex = subpass;

        ImGui::IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui::IO& io = ImGui::GetIO();
        if (config.enable_docking) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
        if (config.enable_viewports) {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        inputManager_ = std::make_unique<InputManager>();
        if (!inputManager_->Initialize(window)) {
            std::cerr << "[UI] Input manager failed to initialize" << std::endl;
            return false;
        }

        renderer_ = std::make_unique<UIRenderer>();
        if (!renderer_->Initialize(instance, physicalDevice, device, graphicsQueueFamily, graphicsQueue, uiRenderPass, subpassIndex)) {
            std::cerr << "[UI] Renderer failed to initialize" << std::endl;
            return false;
        }

        if (!renderer_->UploadFonts(graphicsQueue)) {
            std::cerr << "[UI] Font upload failed" << std::endl;
            return false;
        }

        initialized = true;
        std::cout << "[UI] Module initialized" << std::endl;

#ifdef UI_ENABLE_LUA
        // Integrate Lua
        auto scripting = serviceLocator_.GetScriptingAPI();
        if (scripting) {
            auto& lua = scripting->GetLuaState();
            ui::scripting::LuaUIBinding::Register(lua);
            ui::scripting::UIEventSystem::Initialize(lua);
            // Load demo script
            try {
                lua.script_file("modules/ui/resources/scripts/ui_demo.lua");
            } catch (const sol::error& e) {
                std::cerr << "[UI] Lua script error: " << e.what() << std::endl;
            }
            luaState_ = &lua;
        }
#endif

        return true;
    }

    void Shutdown() {
        panels_.clear();
        initialized = false;
        std::cout << "[UI] Module shutdown" << std::endl;
    }

    void AddPanel(std::unique_ptr<IPanel> panel) {
        panel->OnAttach(serviceLocator_);
        panels_.push_back(std::move(panel));
    }

    void RemovePanel(IPanel* panel) {
        auto it = std::find_if(panels_.begin(), panels_.end(),
                               [panel](const std::unique_ptr<IPanel>& p) { return p.get() == panel; });
        if (it != panels_.end()) {
            (*it)->OnDetach();
            panels_.erase(it);
        }
    }

    void UpdatePanels(float deltaTime) {
        for (auto& panel : panels_) {
            panel->OnUpdate(deltaTime);
        }
    }

    void RenderPanels() {
        for (uint32_t layer = 0; layer < static_cast<uint32_t>(UILayer::COUNT); ++layer) {
            for (auto& panel : panels_) {
                if (panel->IsVisible() && panel->GetLayer() == static_cast<UILayer>(layer)) {
                    panel->OnUIRender();
                }
            }
        }
#ifdef UI_ENABLE_LUA
        if (luaState_) {
            sol::function renderFunc = (*luaState_)["render_ui_demo"];
            if (renderFunc.valid()) {
                renderFunc();
            }
        }
#endif
    }

    ServiceLocator& GetServiceLocator() {
        return serviceLocator_;
    }
};

UIContext::UIContext(const UIConfig& config)
    : pImpl_(std::make_unique<Impl>())
{
    pImpl_->config = config;
}

UIContext::~UIContext() {
    if (pImpl_ && pImpl_->initialized) {
        Shutdown();
    }
}

bool UIContext::Initialize(GLFWwindow* window,
                           VkInstance instance,
                           VkDevice device,
                           VkPhysicalDevice physicalDevice,
                           uint32_t graphicsQueueFamily,
                           VkQueue graphicsQueue,
                           VkRenderPass uiRenderPass,
                           uint32_t subpassIndex)
{
    return pImpl_->Initialize(window, instance, device, physicalDevice, graphicsQueueFamily, graphicsQueue, uiRenderPass, subpassIndex);
}

void UIContext::Shutdown() {
    if (pImpl_->renderer_) {
        pImpl_->renderer_->Shutdown();
    }
    if (pImpl_->inputManager_) {
        pImpl_->inputManager_->Shutdown();
    }
    ImGui::DestroyContext(ImGui::GetCurrentContext());
    pImpl_->Shutdown();
}

bool UIContext::IsInitialized() const {
    return pImpl_->initialized;
}

void UIContext::BeginFrame() {
    if (!pImpl_->initialized) {
        return;
    }

    if (pImpl_->inputManager_) {
        pImpl_->inputManager_->BeginFrame();
    }
    ImGui::NewFrame();
}

void UIContext::EndFrame() {
    if (!pImpl_->initialized) {
        return;
    }

    ImGui::EndFrame();
    if (pImpl_->inputManager_) {
        pImpl_->inputManager_->EndFrame();
    }
}

void UIContext::RenderUI(VkCommandBuffer commandBuffer) {
    if (!pImpl_->initialized || !pImpl_->renderer_) {
        return;
    }

    RenderPanels();
    pImpl_->renderer_->Render(commandBuffer);
}

void UIContext::AddPanel(std::unique_ptr<IPanel> panel) {
    pImpl_->AddPanel(std::move(panel));
}

void UIContext::RemovePanel(IPanel* panel) {
    pImpl_->RemovePanel(panel);
}

void UIContext::UpdatePanels(float deltaTime) {
    pImpl_->UpdatePanels(deltaTime);
}

void UIContext::RenderPanels() {
    pImpl_->RenderPanels();
}

ServiceLocator& UIContext::GetServiceLocator() {
    return pImpl_->GetServiceLocator();
}

} // namespace ui
