# Physics Core Unit Tests

set(CORE_TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/core/tests")

if(EXISTS ${CORE_TEST_SOURCE_DIR}/test_types.cpp)
    add_executable(test_types ${CORE_TEST_SOURCE_DIR}/test_types.cpp)
    target_link_libraries(test_types PRIVATE physics_core Catch2::Catch2WithMain)
    add_test(NAME test_types COMMAND test_types)
else()
    message(WARNING "Physics core unit test source test_types.cpp not found; skipping target.")
endif()

if(EXISTS ${CORE_TEST_SOURCE_DIR}/test_world_space.cpp)
    add_executable(test_world_space ${CORE_TEST_SOURCE_DIR}/test_world_space.cpp)
    target_link_libraries(test_world_space PRIVATE physics_core Catch2::Catch2WithMain)
    add_test(NAME test_world_space COMMAND test_world_space)
else()
    message(WARNING "Physics core unit test source test_world_space.cpp not found; skipping target.")
endif()

if(EXISTS ${CORE_TEST_SOURCE_DIR}/test_constraint_softbody.cpp)
    add_executable(test_constraint_softbody ${CORE_TEST_SOURCE_DIR}/test_constraint_softbody.cpp)
    target_link_libraries(test_constraint_softbody PRIVATE physics_core Catch2::Catch2WithMain)
    add_test(NAME test_constraint_softbody COMMAND test_constraint_softbody)
else()
    message(WARNING "Physics core unit test source test_constraint_softbody.cpp not found; skipping target.")
endif()

# Add more test executables as needed
# TODO: Add all physics core unit tests