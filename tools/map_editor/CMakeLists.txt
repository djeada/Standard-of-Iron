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
    tool_catalog.cpp
    element_edit_json.cpp
    generator_client.cpp
    element_ops.cpp
    json_schema.cpp
    hill_projection_model.cpp
    world_document.cpp
    road_network.cpp
    batch_edit.cpp
    town_block.cpp
    json_text.cpp
    world_backdrop.cpp
    world_sprites.cpp
)
target_include_directories(
    map_editor_core
    PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    map_editor_core
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui soi_strategic game_sim
)

# Share the editor widgets with interaction tests so they exercise the same
# canvas and window that ship with the tool.
add_library(
    map_editor_widgets
    STATIC
    editor_window.cpp
    mission_panel.cpp
    generator_panel.cpp
    generation_preview_dialog.cpp
    map_canvas.cpp
    map_canvas_paint.cpp
    spawn_icon_library.cpp
    tool_panel.cpp
    terrain_projection_widget.cpp
    hill_projection_widget.cpp
    mountain_projection_widget.cpp
    json_edit_dialog.cpp
    resize_dialog.cpp
    world_canvas.cpp
    campaign_workspace.cpp
    command_palette.cpp
)
target_link_libraries(
    map_editor_widgets
    PUBLIC map_editor_core Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE ui_shell
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor main.cpp)
else()
    add_executable(map_editor main.cpp)
endif()

target_link_libraries(map_editor PRIVATE map_editor_widgets ui_shell)

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

# The editor's document model driven from the command line: list, audit and
# batch-edit a map without opening a window.
add_executable(map_cli map_cli_main.cpp)
target_link_libraries(map_cli PRIVATE Qt${QT_VERSION_MAJOR}::Core map_editor_core game_sim)
set_target_properties(
    map_cli
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
