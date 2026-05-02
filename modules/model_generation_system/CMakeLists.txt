# Model Generation System Module
find_package(Vulkan REQUIRED)

add_library(model_generation_system STATIC
    src/blueprint_engine.cpp
    src/procedural_geometry_generator.cpp
    src/texture_synthesis_system.cpp
    src/model_generation_system.cpp
)

target_include_directories(model_generation_system PUBLIC include)
target_link_libraries(model_generation_system PUBLIC 
    Vulkan::Vulkan 
    physics_core 
    rendering_engine 
    historical_vehicle_system
    glm
)
target_compile_features(model_generation_system PRIVATE cxx_std_20)

# SIMD optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(model_generation_system PRIVATE -mavx2 -mfma)
elseif(MSVC)
    target_compile_options(model_generation_system PRIVATE /arch:AVX2)
endif()