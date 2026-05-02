# Tools CMake configuration
# Development tools for Historica Universalis

# Editor tool
option(BUILD_EDITOR "Build content editor tool" ON)
if(BUILD_EDITOR)
    add_subdirectory(editor)
endif()

# Converter tool
option(BUILD_CONVERTER "Build data converter tool" ON)
if(BUILD_CONVERTER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/converter/CMakeLists.txt)
    add_subdirectory(converter)
endif()

# Profiler tool
option(BUILD_PROFILER "Build performance profiler tool" ON)
if(BUILD_PROFILER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/profiler/CMakeLists.txt)
    add_subdirectory(profiler)
endif()

# Code Quality tool
option(BUILD_CODE_QUALITY "Build code quality assurance tool" ON)
if(BUILD_CODE_QUALITY AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/code_quality/CMakeLists.txt)
    add_subdirectory(code_quality)
endif()