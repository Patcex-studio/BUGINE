# Code Quality Assurance & Refactoring System
cmake_minimum_required(VERSION 3.20)
project(CodeQuality VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Code Quality Library
add_library(code_quality STATIC
    src/dead_code_detector.cpp
    src/duplicate_code_detector.cpp
    src/bug_detector.cpp
    src/code_quality_analyzer.cpp
    src/code_refactorer.cpp
    src/test_executor.cpp
)

target_include_directories(code_quality PUBLIC
    include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(code_quality
    ${LLVM_LIBRARIES}
    ${CLANG_LIBRARIES}
)

# Main executable
add_executable(code_quality_tool src/main.cpp)
target_link_libraries(code_quality_tool code_quality)

# Tests
enable_testing()
add_subdirectory(tests)