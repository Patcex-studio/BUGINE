# Resource Manager CMakeLists

add_library(resource_manager STATIC
    memory_pool.cpp
    gpu_allocator.cpp
    asset_manager.cpp
    resource_manager.cpp
)

target_include_directories(resource_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_compile_features(resource_manager PRIVATE cxx_std_20)
target_compile_options(resource_manager PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-O3 -march=native -fPIC>
)

# Link Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(resource_manager PUBLIC Vulkan::Vulkan)

# Make header only if needed
set_target_properties(resource_manager PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)
