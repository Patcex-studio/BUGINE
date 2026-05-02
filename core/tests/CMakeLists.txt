enable_testing()

# Find testing framework
find_package(GTest REQUIRED)

add_executable(physics_tests
    test_types.cpp
    test_world_space.cpp
    test_integrators.cpp
    test_matter_systems.cpp
    test_simd.cpp
    test_hybrid_precision.cpp
    test_physics_container.cpp
    test_collision_system.cpp
    test_ballistics_system.cpp
    test_damage_system.cpp
    test_flight_dynamics.cpp
    test_environment_system.cpp
    test_environment_integration.cpp
    test_repair_system.cpp
)

target_include_directories(physics_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(physics_tests PRIVATE
    physics_core
)

# ECS Core Tests
add_executable(ecs_tests
    test_entity_component_system.cpp
)

target_include_directories(ecs_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(ecs_tests PRIVATE
    ecs_core
)

add_test(NAME ECSTests COMMAND ecs_tests)

# AI Core Tests
add_executable(ai_tests
    unit/test_ai_system.cpp
    unit/perception_system_test.cpp
    unit/test_replay_system.cpp
)

target_include_directories(ai_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(ai_tests PRIVATE
    ai_core
    GTest::GTest
    GTest::Main
)

add_test(NAME AITests COMMAND ai_tests)

# Network Core Tests
add_executable(network_core_tests
    test_network_core.cpp
)

target_include_directories(network_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(network_core_tests PRIVATE
    network_core
    GTest::GTest
    GTest::Main
)

add_test(NAME NetworkCoreTests COMMAND network_core_tests)

# Performance Monitoring Tests
add_executable(performance_monitoring_tests
    test_performance_monitoring.cpp
)

target_include_directories(performance_monitoring_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(performance_monitoring_tests PRIVATE
    performance_monitoring
    GTest::GTest
    GTest::Main
)

add_test(NAME PerformanceMonitoringTests COMMAND performance_monitoring_tests)

# Model System Tests
add_executable(model_system_tests
    test_model_system.cpp
)

target_include_directories(model_system_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(model_system_tests PRIVATE
    rendering_engine
    GTest::gtest_main
)

# Register tests
add_test(NAME PhysicsTypesTest COMMAND physics_tests)
add_test(NAME WorldSpaceTest COMMAND physics_tests)
add_test(NAME IntegratorsTest COMMAND physics_tests)
add_test(NAME MatterSystemsTest COMMAND physics_tests)
add_test(NAME SIMDTest COMMAND physics_tests)
add_test(NAME HybridPrecisionTest COMMAND physics_tests)
add_test(NAME PhysicsContainerTest COMMAND physics_tests)
add_test(NAME BallisticsSystemTest COMMAND physics_tests)
add_test(NAME DamageSystemTest COMMAND physics_tests)
add_test(NAME ModelSystemTest COMMAND model_system_tests)

# Resource Manager Tests (if GTest is available)
if(GTest_FOUND)
    add_executable(resource_manager_tests
        test_resource_manager.cpp
    )
    
    target_include_directories(resource_manager_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(resource_manager_tests PRIVATE
        resource_manager
        GTest::GTest
        GTest::Main
        Vulkan::Vulkan
    )
    
    add_test(NAME ResourceManagerTests COMMAND resource_manager_tests)
endif()
