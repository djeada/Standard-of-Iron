# ---- Qt ----
# Try Qt6 first, fall back to Qt5 if not available
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(
    Qt6
    QUIET
    COMPONENTS Core Widgets OpenGL OpenGLWidgets Quick Qml QuickControls2 Sql
    OPTIONAL_COMPONENTS Multimedia
)
if(Qt6_FOUND)
    message(STATUS "Using Qt6")
    set(QT_VERSION_MAJOR 6)
    find_package(OpenGL REQUIRED)
else()
    # The Qt5 fallback is gone rather than merely unused. Its resource manifest,
    # qml_resources.qrc, listed 36 of the 88 QML files and two -- Constants.qml
    # and Colors.js -- that have not existed for a long time. Nothing built from
    # it could have started: SettingsPanel, CampaignScreen and the whole design
    # system were absent. Because only the Qt6 branch is exercised, that rotted
    # silently for as long as it took someone to audit it.
    #
    # Failing here is the honest outcome. A build that configures on Qt5 and
    # then dies at runtime on missing QML is strictly worse than one that says
    # up front what it needs.
    message(
        FATAL_ERROR
        "Qt6 was not found. Standard of Iron requires Qt 6.4 or newer "
        "(see the Requirements section of README.md). The Qt5 code paths were "
        "removed after the audit for v0.1.0: they had rotted past the point of "
        "producing a runnable game."
    )
endif()

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
elseif(COMMAND qt6_standard_project_setup)
    qt6_standard_project_setup()
endif()

# Qt 6.5+ warns on every configure until these are answered. Both are pinned to
# the behaviour the project already relies on: QML lives under qrc:/StandardOfIron
# rather than the qrc:/qt/qml prefix QTP0001 NEW would impose (see the import
# paths in main.cpp), and the module ships its own qmldir files. Adopting NEW is a
# separate migration, not a warning fix.
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 OLD)
    if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
        qt_policy(SET QTP0004 OLD)
    endif()
endif()
