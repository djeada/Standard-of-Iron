if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor
        main.cpp
        editor_window.cpp
    )
else()
    add_executable(map_editor
        main.cpp
        editor_window.cpp
    )
endif()

target_link_libraries(map_editor
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    engine_core
    render_gl
)