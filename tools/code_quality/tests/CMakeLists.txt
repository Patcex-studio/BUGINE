# Tests for Code Quality Assurance System

# Unit tests
add_executable(test_code_quality
    test_dead_code_detector.cpp
    test_duplicate_code_detector.cpp
    test_bug_detector.cpp
    test_code_quality_analyzer.cpp
)

target_link_libraries(test_code_quality
    code_quality
    # Add testing framework (e.g., Google Test, Catch2)
)

# Integration tests
add_executable(test_integration
    test_integration.cpp
)

target_link_libraries(test_integration
    code_quality
)

# Enable testing
enable_testing()
add_test(NAME unit_tests COMMAND test_code_quality)
add_test(NAME integration_tests COMMAND test_integration)