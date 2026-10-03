# Focused model binaries. They stay runnable when an unrelated test translation
# unit in the render suite has interface drift. Both test sources are also
# compiled into render_tests, which is where they run on the default path --
# these are a debugging convenience, not a second verification route.
# docs/HORSE_MODEL_ARCHITECTURE.md points at them.
add_executable(horse_model_tests render/horse/horse_anatomy_test.cpp test_main.cpp)
target_link_libraries(
    horse_model_tests
    PRIVATE
        GTest::gtest
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
)
soi_register_test_binary(horse_model_tests)
if(TARGET bake_creature_assets)
    add_dependencies(horse_model_tests bake_creature_assets)
endif()

add_executable(elephant_model_tests render/elephant/elephant_source_asset_test.cpp test_main.cpp)
target_link_libraries(
    elephant_model_tests
    PRIVATE
        GTest::gtest
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
)
soi_register_test_binary(elephant_model_tests)
if(TARGET bake_creature_assets)
    add_dependencies(elephant_model_tests bake_creature_assets)
endif()
