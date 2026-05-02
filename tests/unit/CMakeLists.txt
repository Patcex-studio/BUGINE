# Unit Tests

find_package(Catch2 CONFIG QUIET)
if(NOT Catch2_FOUND)
    message(WARNING "Catch2 not found. Catch2-based unit tests will be skipped. Install Catch2 or add it to third_party/catch2 if you want unit test support.")
endif()

if(Catch2_FOUND)
    # Physics core unit tests
    add_subdirectory(physics_core)

    # Module unit tests requiring Catch2
    add_subdirectory(assembly_system)
    add_subdirectory(historical_vehicle_system)

    # Phase 3 AI tactical tests
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/test_phase3_tactical.cpp)
        add_executable(test_phase3_tactical ${CMAKE_SOURCE_DIR}/tests/unit/test_phase3_tactical.cpp)
        target_link_libraries(test_phase3_tactical PRIVATE ai_core Catch2::Catch2WithMain)
        target_include_directories(test_phase3_tactical PRIVATE ${CMAKE_SOURCE_DIR}/core/include)
        add_test(NAME test_phase3_tactical COMMAND test_phase3_tactical)
    else()
        message(WARNING "Phase 3 AI tactical test source not found; skipping test_phase3_tactical.")
    endif()
endif()

# PTU system unit tests do not depend on Catch2
add_subdirectory(ptu_system)

# ============================================================================
# Phase 2 AI Dynamic Modifiers Unit Tests (no Catch2 required)
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_dynamic_modifiers.cpp)
    add_executable(test_dynamic_modifiers test_dynamic_modifiers.cpp)
    target_include_directories(test_dynamic_modifiers PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
    )
    target_link_libraries(test_dynamic_modifiers PRIVATE ai_core)
    target_compile_features(test_dynamic_modifiers PRIVATE cxx_std_20)
    target_compile_options(test_dynamic_modifiers PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3>
    )
    add_test(NAME test_dynamic_modifiers COMMAND test_dynamic_modifiers)
else()
    message(WARNING "Dynamic modifiers unit test source not found; skipping test_dynamic_modifiers.")
endif()

