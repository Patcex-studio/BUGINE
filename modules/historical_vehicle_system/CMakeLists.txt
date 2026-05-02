# Historical Vehicle System Module
add_library(historical_vehicle_system STATIC
    src/historical_vehicle_system.cpp
)

target_include_directories(historical_vehicle_system PUBLIC include)
target_link_libraries(historical_vehicle_system PUBLIC assembly_system physics_core ecs_core rendering_engine)

# Link procedural armor factory if available
if(TARGET procedural_armor_factory)
    target_link_libraries(historical_vehicle_system PUBLIC procedural_armor_factory)
endif()

target_compile_features(historical_vehicle_system PRIVATE cxx_std_20)
target_compile_options(historical_vehicle_system PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -march=native -O3 -fPIC>
)
