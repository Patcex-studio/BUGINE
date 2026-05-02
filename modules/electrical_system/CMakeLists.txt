# Electrical System Module
add_library(electrical_system STATIC
    src/electrical_system.cpp
)

target_include_directories(electrical_system PUBLIC include)

# Core dependencies
target_link_libraries(electrical_system PUBLIC physics_core ecs_core)

# C++20 standard required for this_thread, atomic operations
target_compile_features(electrical_system PRIVATE cxx_std_20)

# Optional: Enable debug features
option(ENABLE_ELECTRICAL_DEBUG "Enable electrical system debug hooks" OFF)
if(ENABLE_ELECTRICAL_DEBUG)
    target_compile_definitions(electrical_system PRIVATE ENABLE_ELECTRICAL_DEBUG)
endif()

# Tests (optional)
option(BUILD_ELECTRICAL_TESTS "Build electrical system tests" ON)
if(BUILD_ELECTRICAL_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
