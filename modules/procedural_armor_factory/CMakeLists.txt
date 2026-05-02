# Procedural Armor Factory Module
add_library(procedural_armor_factory STATIC
    src/armor_generator.cpp
)

find_package(glm REQUIRED)

target_include_directories(procedural_armor_factory PUBLIC include)
target_link_libraries(procedural_armor_factory PUBLIC glm::glm rendering_engine physics_core)
target_compile_features(procedural_armor_factory PRIVATE cxx_std_20)
target_compile_options(procedural_armor_factory PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)
