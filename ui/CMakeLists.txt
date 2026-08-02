# Shared UI shell pieces: the theme tokens and the persisted preferences.
#
# They are a library rather than sources compiled into each consumer because
# both the executable and the test binaries need them, and a test that
# recompiles a production file can quietly diverge from what ships.
#
# The rest of ui/ (GLView, CampaignMapView) stays in the executable: it is
# registered with QML by main.cpp and has no other consumer.
add_library(ui_shell STATIC theme.cpp preferences.cpp input_bindings.cpp)

target_include_directories(ui_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(
    ui_shell
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
        accessibility_runtime
)
set_target_properties(ui_shell PROPERTIES AUTOMOC ON)
