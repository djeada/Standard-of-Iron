# ---- tools_tests -------------------------------------------------------
# The map editor document model and the balance runner. Neither needs a
# renderer, so this binary links neither render_gl nor game_view -- an editor
# change that starts reaching for the scene graph fails here.
add_executable(
    tools_tests
    tools/map_editor_map_data_test.cpp
    tools/map_editor_mission_data_test.cpp
    tools/map_editor_canvas_transform_test.cpp
    tools/map_editor_canvas_input_test.cpp
    tools/map_editor_wall_geometry_test.cpp
    tools/map_editor_element_ops_test.cpp
    tools/map_editor_wildlife_test.cpp
    tools/map_editor_json_schema_test.cpp
    tools/map_editor_commander_preview_test.cpp
    tools/hill_projection_model_test.cpp
    tools/balance_sim_test.cpp
    widget_main.cpp
)
target_link_libraries(
    tools_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        game_sim
        map_editor_core
        balance_sim_harness
)
soi_register_test_binary(tools_tests)
if(TARGET bake_creature_assets)
    add_dependencies(tools_tests bake_creature_assets)
endif()
