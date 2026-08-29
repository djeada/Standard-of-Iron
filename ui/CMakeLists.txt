# Shared UI shell pieces: the theme tokens, the persisted preferences, and the
# widget-side shell the two Qt Widgets tools dress themselves with.
#
# They are a library rather than sources compiled into each consumer because
# both the executable and the test binaries need them, and a test that
# recompiles a production file can quietly diverge from what ships.
#
# widget_shell.cpp is here for that reason and not by accident: the map editor
# and the arena both link ui_shell *and* compiled widget_shell.cpp into
# themselves, so each tool carried its own copy of the theme application while
# believing it used the shared one.
#
# The rest of ui/ (GLView, CampaignMapView) stays in the executable: it is
# registered with QML by main.cpp and has no other consumer.
add_library(
    ui_shell
    STATIC
    theme.cpp
    preferences.cpp
    hints.cpp
    input_bindings.cpp
    edge_scroll.cpp
    game_speeds.cpp
    icon_art.cpp
    brand_fonts.cpp
    widget_shell.cpp
)

target_include_directories(ui_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(
    ui_shell
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Widgets
        accessibility_runtime
)
set_target_properties(ui_shell PROPERTIES AUTOMOC ON)
