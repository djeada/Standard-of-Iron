qt6_add_executable(map_editor
    main.cpp
    editor_window.cpp
)

target_link_libraries(map_editor
    PRIVATE
    Qt6::Core
    Qt6::Widgets
    engine_core
    render_gl
)