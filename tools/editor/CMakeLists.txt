# Content Editor CMake configuration

# Find required packages
find_package(Lua 5.4 REQUIRED)

# Create content editor library
add_library(content_editor
    src/content_editor.cpp
    src/vehicle_editor.cpp
    src/mission_designer.cpp
    src/scripting_api.cpp
)

target_include_directories(content_editor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(content_editor
    PUBLIC
        physics_core
        assembly_system
        Lua::Lua
)

# Set compiler options
target_compile_options(content_editor PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Create main editor executable
add_executable(content_editor_app
    src/main.cpp
)

target_link_libraries(content_editor_app
    PRIVATE
        content_editor
)

# Install targets
install(TARGETS content_editor content_editor_app
    EXPORT content_editor_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Export package
install(EXPORT content_editor_targets
    FILE content_editor-config.cmake
    NAMESPACE content_editor::
    DESTINATION lib/cmake/content_editor
)