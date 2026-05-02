# Camera + Model System Demo
add_executable(camera_model_demo main.cpp)
target_link_libraries(camera_model_demo PRIVATE rendering_engine model_generation)
target_include_directories(camera_model_demo PRIVATE ${CMAKE_SOURCE_DIR})
