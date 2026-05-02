# PTU System Module
add_library(ptu_system STATIC
    src/content_security_validator.cpp
    src/content_integration_pipeline.cpp
    src/content_database.cpp
    src/content_version_control.cpp
    src/ptu_manager.cpp
)

target_include_directories(ptu_system PUBLIC include)
target_compile_features(ptu_system PRIVATE cxx_std_20)

# Find SQLite3
find_package(SQLite3 REQUIRED)
if(SQLite3_FOUND)
    target_include_directories(ptu_system PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(ptu_system PRIVATE ${SQLite3_LIBRARIES})
else()
    message(FATAL_ERROR "SQLite3 not found. Please install SQLite3 development libraries.")
endif()