if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor
        main.cpp
        editor_window.cpp
        map_data.cpp
        map_canvas.cpp
        tool_panel.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
    )
else()
    add_executable(map_editor
        main.cpp
        editor_window.cpp
        map_data.cpp
        map_canvas.cpp
        tool_panel.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
    )
endif()

target_link_libraries(map_editor
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)