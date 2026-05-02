add_executable(ui_widgets_demo main.cpp)

target_link_libraries(ui_widgets_demo
    ui
    # Other deps
)

target_compile_features(ui_widgets_demo PUBLIC cxx_std_20)