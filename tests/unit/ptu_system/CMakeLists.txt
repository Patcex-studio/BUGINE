# PTU System Unit Tests
set(PTU_TEST_SOURCE "${CMAKE_SOURCE_DIR}/tests/ptu_system/test_ptu_system.cpp")

if(EXISTS ${PTU_TEST_SOURCE})
    add_executable(test_ptu_system ${PTU_TEST_SOURCE})

    target_link_libraries(test_ptu_system PRIVATE ptu_system)
    target_include_directories(test_ptu_system PRIVATE ${CMAKE_SOURCE_DIR}/modules/ptu_system/include)
    add_test(NAME PTUSystemTest COMMAND test_ptu_system)
else()
    message(WARNING "PTU system unit test source ${PTU_TEST_SOURCE} not found; skipping PTUSystemTest.")
endif()
