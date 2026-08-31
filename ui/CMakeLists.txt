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

# ---- StandardOfIron.Core: the app's tokens as a real QML module -------------
#
# Theme, the persisted preferences and the input bindings used to reach QML
# through qmlRegisterSingletonType calls in main.cpp. That works at runtime and
# is invisible to everything else: qmllint saw a module with no types in it, so
# every `Theme.spacingSmall` in the UI read as an unqualified access to an
# unknown name and roughly seven hundred warnings said so.
#
# Declaring the module here registers the same singletons at build time, which
# gives the tooling a type to resolve. The URI stays `StandardOfIron.Core` and
# not `StandardOfIron` on purpose: StandardOfIron.Design reads these tokens, and
# the app module imports Design, so putting them in the app module would close a
# cycle.
if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_qml_module(
        ui_shell
        URI StandardOfIron.Core
        VERSION 1.0
        # The library is its own plugin: a separate plugin target would call the
        # library's compiled-in registration and claim the namespace twice.
        PLUGIN_TARGET ui_shell
        # The tokens are QColor, and a module that names a type from another
        # module has to say so or every colour property reads as an unknown type.
        DEPENDENCIES QtQuick
        # The module has to land where its URI says it lives, or the import path
        # the tooling is given cannot find it.
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/StandardOfIron/Core"
    )
    # A library acting as its own QML plugin has to be built as a static plugin,
    # or Q_PLUGIN_METADATA emits the shared-object entry points and the
    # executable's Q_IMPORT_PLUGIN has nothing to bind to.
    target_compile_definitions(ui_shell PRIVATE QT_STATICPLUGIN)
endif()

# ---- StandardOfIron.Views: the three QQuickFramebufferObject views ----------
#
# GLView, CampaignMapView and CommanderPortraitView are QML types backed by C++,
# and they used to reach QML through qmlRegisterType calls in main.cpp. An
# executable cannot carry a QML plugin, so a module declared on the executable
# registers nothing the type loader will look at -- which is why these three sat
# unresolved in every tool that reads the UI. A static library can carry one, so
# they live in their own module here.
#
# They are not part of ui_shell: ui_shell is the shared theme/preferences layer
# that the map editor and the arena link, and these three pull in the renderer
# and the running match.
if(QT_VERSION_MAJOR EQUAL 6)
    add_library(
        soi_qml_views
        STATIC
        gl_view.cpp
        campaign_map_view.cpp
        commander_portrait_view.cpp
        commander_portrait_scenes.cpp
    )
    target_include_directories(soi_qml_views PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(
        soi_qml_views
        PUBLIC
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::OpenGL
            ui_shell
            app_core
            render_gl
            animation_core
    )
    set_target_properties(soi_qml_views PROPERTIES AUTOMOC ON)
    qt_add_qml_module(
        soi_qml_views
        URI StandardOfIron.Views
        VERSION 1.0
        PLUGIN_TARGET soi_qml_views
        # The views derive from QQuickFramebufferObject.
        DEPENDENCIES QtQuick
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/StandardOfIron/Views"
    )
    target_compile_definitions(soi_qml_views PRIVATE QT_STATICPLUGIN)
endif()
