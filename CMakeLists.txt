cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 1.0.0 LANGUAGES CXX)

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Qt ----
# Try Qt6 first, fall back to Qt5 if not available
find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2)
if(Qt6_FOUND)
    message(STATUS "Using Qt6")
    set(QT_VERSION_MAJOR 6)
    find_package(OpenGL REQUIRED)
else()
    message(STATUS "Qt6 not found, trying Qt5")
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2)
    message(STATUS "Using Qt5")
    set(QT_VERSION_MAJOR 5)
    find_package(OpenGL REQUIRED)
endif()

if(COMMAND qt_standard_project_setup)
  qt_standard_project_setup()
elseif(COMMAND qt6_standard_project_setup)
  qt6_standard_project_setup()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# engine core moved under game; no separate engine subdir target
add_subdirectory(render)
add_subdirectory(game)
add_subdirectory(ui)
add_subdirectory(tools)

# ---- Executable ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(standard_of_iron
        main.cpp
        app/game_engine.cpp
        app/selected_units_model.cpp
        ui/gl_view.cpp
    )
else()
    add_executable(standard_of_iron
        main.cpp
        app/game_engine.cpp
        app/selected_units_model.cpp
        ui/gl_view.cpp
    )
endif()

# ---- QML module ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_qml_module(standard_of_iron
        URI StandardOfIron
        VERSION 1.0
        QML_FILES
            ui/qml/Main.qml
            ui/qml/MainMenu.qml
            ui/qml/MapSelect.qml
            ui/qml/HUD.qml
            ui/qml/HUDTop.qml
            ui/qml/HUDBottom.qml
            ui/qml/HUDVictory.qml
            ui/qml/GameView.qml
        RESOURCES
            assets/shaders/basic.vert
            assets/shaders/basic.frag
            assets/shaders/grid.frag
            assets/maps/test_map.json
            assets/visuals/unit_visuals.json
        DEPENDENCIES
            Qt6::QuickControls2
    )
else()
    qt5_add_resources(standard_of_iron "qml_resources"
        PREFIX "/StandardOfIron"
        FILES
            ui/qml/Main.qml
            ui/qml/MainMenu.qml
            ui/qml/MapSelect.qml
            ui/qml/HUD.qml
            ui/qml/HUDTop.qml
            ui/qml/HUDBottom.qml
            ui/qml/HUDVictory.qml
            ui/qml/GameView.qml
    )
    qt5_add_resources(standard_of_iron "assets" 
        PREFIX "/"
        FILES
            assets/shaders/basic.vert
            assets/shaders/basic.frag
            assets/shaders/grid.frag
            assets/maps/test_map.json
            assets/visuals/unit_visuals.json
    )
endif()

target_link_libraries(standard_of_iron
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(standard_of_iron PRIVATE Qt6::QuickControls2)
else()
    target_link_libraries(standard_of_iron PRIVATE Qt5::QuickControls2)
endif()

# Copy assets next to the binary for dev runs
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
