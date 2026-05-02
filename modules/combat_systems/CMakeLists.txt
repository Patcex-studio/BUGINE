# Combat Systems Module
add_library(combat_systems STATIC
    src/active_protection.cpp
    src/thermal_vision.cpp
    src/electronic_warfare.cpp
    src/nbc_warfare.cpp
    src/combat_systems.cpp
)

target_include_directories(combat_systems PUBLIC include)
target_link_libraries(combat_systems PUBLIC physics_core)
target_compile_features(combat_systems PRIVATE cxx_std_20)