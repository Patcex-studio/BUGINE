# Performance Benchmarks

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/physics_benchmark.cpp)
    add_executable(physics_benchmark physics_benchmark.cpp)
    target_link_libraries(physics_benchmark PRIVATE physics_core)
else()
    message(WARNING "physics_benchmark.cpp not found; skipping physics_benchmark target.")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/collision_benchmark.cpp)
    add_executable(collision_benchmark collision_benchmark.cpp)
    target_link_libraries(collision_benchmark PRIVATE physics_core)
else()
    message(WARNING "collision_benchmark.cpp not found; skipping collision_benchmark target.")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/terrain_benchmark.cpp)
    add_executable(terrain_benchmark terrain_benchmark.cpp)
    target_link_libraries(terrain_benchmark PRIVATE physics_core)
else()
    message(WARNING "terrain_benchmark.cpp not found; skipping terrain_benchmark target.")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/flight_benchmark.cpp)
    add_executable(flight_benchmark flight_benchmark.cpp)
    target_link_libraries(flight_benchmark PRIVATE physics_core)
else()
    message(WARNING "flight_benchmark.cpp not found; skipping flight_benchmark target.")
endif()

# Network benchmark (TO BE IMPLEMENTED)
# add_executable(network_benchmark network_benchmark.cpp)