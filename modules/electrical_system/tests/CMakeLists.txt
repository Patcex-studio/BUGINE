# Electrical System Tests
add_executable(electrical_system_tests
    electrical_system_test.cpp
)

target_link_libraries(electrical_system_tests PRIVATE
    electrical_system
    physics_core
    ecs_core
    Catch2::Catch2WithMain
)

target_compile_features(electrical_system_tests PRIVATE cxx_std_20)

add_test(
    NAME ElectricalSystemTests
    COMMAND electrical_system_tests
)
