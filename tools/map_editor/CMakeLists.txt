if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor
        main.cpp
        ${CMAKE_SOURCE_DIR}/game/map/environment_lighting.cpp
        editor_window.cpp
        map_data.cpp
        mission_data.cpp
        mission_panel.cpp
        map_canvas.cpp
        commander_preview.cpp
        element_ops.cpp
        json_schema.cpp
        spawn_icon_library.cpp
        tool_panel.cpp
        hill_projection_model.cpp
        terrain_projection_widget.cpp
        hill_projection_widget.cpp
        mountain_projection_widget.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
        ../../ui/theme.cpp
        ../../ui/preferences.cpp
        ../../ui/widget_shell.cpp
    )
else()
    add_executable(
        map_editor
        main.cpp
        ${CMAKE_SOURCE_DIR}/game/map/environment_lighting.cpp
        editor_window.cpp
        map_data.cpp
        mission_data.cpp
        mission_panel.cpp
        map_canvas.cpp
        commander_preview.cpp
        element_ops.cpp
        json_schema.cpp
        spawn_icon_library.cpp
        tool_panel.cpp
        hill_projection_model.cpp
        terrain_projection_widget.cpp
        hill_projection_widget.cpp
        mountain_projection_widget.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
        ../../ui/theme.cpp
        ../../ui/preferences.cpp
        ../../ui/widget_shell.cpp
    )
endif()

target_link_libraries(
    map_editor
    PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Qml
)

# Keep tool binaries alongside the main game binary for consistent dev runs
# (Makefile expects executables under build/bin)
set_target_properties(
    map_editor
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
