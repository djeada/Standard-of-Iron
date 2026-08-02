# The editor's document model, with no widget in it: the map and mission
# documents, element operations, the JSON schema, the derived-commander rules
# and the hill projection maths.
#
# It is a library rather than a list of sources on the executable so the tests
# can link the objects the editor actually ships. Recompiling these `.cpp` files
# a second time inside a test binary is how a test ends up passing against a
# translation unit nobody runs.
add_library(
    map_editor_core
    STATIC
    map_data.cpp
    mission_data.cpp
    commander_preview.cpp
    element_ops.cpp
    json_schema.cpp
    hill_projection_model.cpp
)
target_include_directories(
    map_editor_core
    PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(map_editor_core PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor
        main.cpp
        ${CMAKE_SOURCE_DIR}/game/map/environment_lighting.cpp
        editor_window.cpp
        mission_panel.cpp
        map_canvas.cpp
        spawn_icon_library.cpp
        tool_panel.cpp
        terrain_projection_widget.cpp
        hill_projection_widget.cpp
        mountain_projection_widget.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
        ../../ui/widget_shell.cpp
    )
else()
    add_executable(
        map_editor
        main.cpp
        ${CMAKE_SOURCE_DIR}/game/map/environment_lighting.cpp
        editor_window.cpp
        mission_panel.cpp
        map_canvas.cpp
        spawn_icon_library.cpp
        tool_panel.cpp
        terrain_projection_widget.cpp
        hill_projection_widget.cpp
        mountain_projection_widget.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
        ../../ui/widget_shell.cpp
    )
endif()

target_link_libraries(
    map_editor
    PRIVATE
        ui_shell
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Qml
        map_editor_core
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
