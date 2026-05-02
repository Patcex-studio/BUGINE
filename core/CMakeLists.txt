cmake_minimum_required(VERSION 3.20)
project(PhysicsCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -march=native -O3 -fPIC")

# Physics Core Library
add_library(physics_core STATIC
    src/physics_core/world_space_manager.cpp
    src/physics_core/local_coordinate_frame.cpp
    src/physics_core/physics_body.cpp
    src/physics_core/verlet_integrator.cpp
    src/physics_core/rk4_integrator.cpp
    src/physics_core/simd_processor.cpp
    src/physics_core/rigid_body_system.cpp
    src/physics_core/fluid_system.cpp
    src/physics_core/gas_system.cpp
    src/physics_core/collision_system.cpp
    src/physics_core/constraint_system.cpp
    src/physics_core/soft_body_system.cpp
    src/physics_core/sph_system.cpp
    src/physics_core/flight_dynamics.cpp
    src/physics_core/damage_system.cpp
    src/physics_core/ballistics_system.cpp
    src/physics_core/armor_materials.cpp
    src/physics_core/projectile_properties.cpp
    src/physics_core/terrain_system.cpp
    src/physics_core/async_bvh_builder.cpp
    src/physics_core/sdf_collision_shape.cpp
    src/physics_core/environment_system.cpp
    src/physics_core/destruction_system.cpp
    src/physics_core/hybrid_precision.cpp
    src/physics_core/physics_container.cpp
    src/physics_core/physics_thread_pool.cpp
    src/physics_core/physics_core.cpp
    src/physics_core/repair_system.cpp
    src/physics_core/sph_boundary_system.cpp
    src/physics_core/thermal_system.cpp
    src/physics_core/acoustics_system.cpp
)

target_include_directories(physics_core PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/physics_core
)
target_compile_features(physics_core PRIVATE cxx_std_20)
target_compile_options(physics_core PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)

target_include_directories(physics_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(ecs_core STATIC
    src/ecs/ecs.cpp
)

target_include_directories(ecs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ecs_core PRIVATE cxx_std_20)

target_compile_options(ecs_core PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)

# AI Core Library
add_library(ai_core STATIC
    src/ai_core/behavior_tree.cpp
    src/ai_core/pathfinding.cpp
    src/ai_core/tactical_decision.cpp
    src/ai_core/decision_engine_stub.cpp
    src/ai_core/decision_engine.cpp
    src/ai_core/dynamic_modifiers.cpp
    src/ai_core/formation_system.cpp
    src/ai_core/suppression_system.cpp
    src/ai_core/communication_system.cpp
    src/ai_core/tactical_network.cpp
    src/ai_core/spatial_grid.cpp
    src/ai_core/perception_system.cpp
    src/ai_core/replay_types.cpp
    src/ai_core/replay_recorder.cpp
    src/ai_core/replay_player.cpp
    src/ai_core/ai_debugger.cpp
)

target_include_directories(ai_core PUBLIC include)
target_link_libraries(ai_core PUBLIC physics_core)
target_compile_features(ai_core PRIVATE cxx_std_20)
target_compile_options(ai_core PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)

# Network Core Library
add_library(network_core STATIC
    src/network_core/network_core.cpp
)

target_include_directories(network_core PUBLIC include)
target_compile_features(network_core PRIVATE cxx_std_20)
target_compile_options(network_core PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)

# Performance Monitoring Library
add_library(performance_monitoring STATIC
    src/performance_monitoring.cpp
)

target_include_directories(performance_monitoring PUBLIC include)
target_compile_features(performance_monitoring PRIVATE cxx_std_20)
target_compile_options(performance_monitoring PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)

# Find ASIO for networking (header-only)
find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(network_core PRIVATE asio::asio)
    target_compile_definitions(network_core PRIVATE ASIO_STANDALONE)
    message(STATUS "Found ASIO package")
else()
    # Try to find standalone ASIO
    find_path(ASIO_INCLUDE_DIR asio.hpp)
    if(ASIO_INCLUDE_DIR)
        target_include_directories(network_core PRIVATE ${ASIO_INCLUDE_DIR})
        target_compile_definitions(network_core PRIVATE ASIO_STANDALONE)
        message(STATUS "Found standalone ASIO at ${ASIO_INCLUDE_DIR}")
    else()
        message(WARNING "ASIO not found. Network functionality will be limited. Install ASIO or add it to third_party/asio/include")
        target_compile_definitions(network_core PRIVATE NO_ASIO)
    endif()
endif()

# Optional: Physics core shared library for C API
add_library(physics_core_shared SHARED
    src/physics_core/physics_core_c_api.cpp
)

target_link_libraries(physics_core_shared PRIVATE physics_core)
target_include_directories(physics_core_shared PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find Vulkan for rendering engine
find_package(Vulkan REQUIRED)

# Find GLFW for window creation (optional, will try to find if available)
find_package(glfw3 CONFIG QUIET)
if(glfw3_FOUND)
    message(STATUS "Found GLFW3 package")
else()
    # Try alternative approaches
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        message(STATUS "Found GLFW3")
    else()
        # For now, warning - we'll handle in rendering engine
        message(STATUS "GLFW3 not found - will attempt runtime detection or disable GLFW support")
    endif()
endif()

find_program(GLSLC_EXECUTABLE NAMES glslc glslangValidator)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc/glslangValidator not found. Install Vulkan SDK or set CMAKE_PREFIX_PATH.")
endif()

set(DEFERRED_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/gbuffer.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/gbuffer.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/lighting.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/lighting.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/tonemap.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/tonemap.frag
)

set(DEFERRED_SHADER_SPV "")
foreach(shader IN LISTS DEFERRED_SHADER_SOURCES)
    get_filename_component(shader_ext ${shader} EXT)
    if(shader_ext STREQUAL ".vert")
        set(stage vert)
    elseif(shader_ext STREQUAL ".frag")
        set(stage frag)
    else()
        message(FATAL_ERROR "Unsupported deferred shader extension: ${shader_ext}")
    endif()

    get_filename_component(shader_name ${shader} NAME)
    set(output ${shader}.spv)

    if("${GLSLC_EXECUTABLE}" MATCHES "glslc$")
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=${stage} ${shader} -o ${output}
            DEPENDS ${shader}
            COMMENT "Compiling shader ${shader_name}"
        )
    else()
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${GLSLC_EXECUTABLE} -V -S ${stage} ${shader} -o ${output}
            DEPENDS ${shader}
            COMMENT "Compiling shader ${shader_name}"
        )
    endif()

    list(APPEND DEFERRED_SHADER_SPV ${output})
endforeach()

add_custom_target(deferred_shaders ALL DEPENDS ${DEFERRED_SHADER_SPV})

# Rendering Engine Resource Manager
add_subdirectory(src/rendering_engine/resource_manager)

# Rendering Engine Library
add_library(rendering_engine STATIC
    src/rendering_engine/rendering_engine.cpp
    src/rendering_engine/model_system.cpp
    src/rendering_engine/render_pipeline_manager.cpp
    src/rendering_engine/cameras/camera_controller.cpp
    src/rendering_engine/pipelines/base_pipeline.cpp
    src/rendering_engine/pipelines/forward_plus_pipeline.cpp
    src/rendering_engine/pipelines/deferred_shading_pipeline.cpp
    src/rendering_engine/pipelines/clustered_shading_pipeline.cpp
)
add_dependencies(rendering_engine deferred_shaders)
target_include_directories(rendering_engine PUBLIC include)

if(glfw3_FOUND)
    target_link_libraries(rendering_engine PUBLIC 
        resource_manager
        Vulkan::Vulkan
        glfw
    )
    target_compile_definitions(rendering_engine PUBLIC RENDERING_ENGINE_WITH_GLFW)
else()
    target_link_libraries(rendering_engine PUBLIC 
        resource_manager
        Vulkan::Vulkan
    )
    message(STATUS "Building rendering_engine without GLFW support - window creation will be stubbed")
endif()

# Testing
# enable_testing()
# add_subdirectory(tests)

# Optional: Example/Demo executable
add_executable(physics_demo
    examples/physics_demo.cpp
)

target_link_libraries(physics_demo PRIVATE physics_core)

# Network Demo
add_executable(network_demo
    examples/network_demo.cpp
)

target_link_libraries(network_demo PRIVATE network_core)
