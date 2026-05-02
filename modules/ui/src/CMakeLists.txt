# Build the ui library

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
    find_package(glfw3 QUIET)
endif()

add_library(ui STATIC
    core/ui_context.cpp
    core/service_locator.cpp
    core/command_queue.cpp
    core/input_manager.cpp
    core/ui_renderer.cpp
    panels/performance_panel.cpp
    panels/console_panel.cpp
    panels/scene_hierarchy.cpp
    panels/hud_panel.cpp
    widgets/unit_list.cpp
    widgets/property_grid.cpp
    widgets/graph_2d.cpp
    widgets/console.cpp
    widgets/minimap.cpp
    widgets/theme_manager.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_stdlib.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_vulkan.cpp
)

find_path(SOL_INCLUDE_DIR sol/sol.hpp PATHS ${PROJECT_SOURCE_DIR}/third_party/sol2/include /usr/local/include /usr/include)
if(SOL_INCLUDE_DIR)
    message(STATUS "Found sol2 at ${SOL_INCLUDE_DIR}")
    target_compile_definitions(ui PRIVATE UI_ENABLE_LUA)
    target_include_directories(ui PRIVATE ${SOL_INCLUDE_DIR})
    target_sources(ui PRIVATE scripting/lua_ui_binding.cpp scripting/ui_event_system.cpp)
else()
    message(STATUS "sol2 not found; UI Lua scripting disabled")
endif()

target_include_directories(ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/core/include
        ${PROJECT_SOURCE_DIR}/tools/editor/include
        ${PROJECT_SOURCE_DIR}/third_party/imgui
        ${PROJECT_SOURCE_DIR}/third_party/imgui/backends
)

target_link_libraries(ui
    PRIVATE
        Vulkan::Vulkan
)

if(glfw3_FOUND)
    target_link_libraries(ui PRIVATE glfw)
endif()

target_compile_definitions(ui PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_ENABLE_FREETYPE)

target_compile_features(ui PUBLIC cxx_std_20)
