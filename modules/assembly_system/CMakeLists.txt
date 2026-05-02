# Assembly System Module
add_library(assembly_system STATIC
    src/blueprint_compiler.cpp
    src/constraint_validator.cpp
    src/ptu_validator.cpp
    src/vehicle_assembly.cpp
    src/universal_assembly_engine.cpp
    src/assembly_system.cpp
)

target_include_directories(assembly_system PUBLIC include)
target_link_libraries(assembly_system PUBLIC physics_core ecs_core)
target_compile_features(assembly_system PRIVATE cxx_std_20)