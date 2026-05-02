# Medical System Module
add_library(medical_system STATIC
    src/injury_simulation.cpp
    src/treatment_system.cpp
    src/crew_status.cpp
    src/medical_system.cpp
)

target_include_directories(medical_system PUBLIC include)
target_link_libraries(medical_system PUBLIC physics_core)
target_compile_features(medical_system PRIVATE cxx_std_20)