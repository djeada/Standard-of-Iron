cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 1.0.0 LANGUAGES CXX)

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Qt ----
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    Quick
    Qml
    QuickControls2
)

find_package(OpenGL REQUIRED)

if(COMMAND qt_standard_project_setup)
  qt_standard_project_setup()
elseif(COMMAND qt6_standard_project_setup)
  qt6_standard_project_setup()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(engine)
add_subdirectory(render)
add_subdirectory(game)
add_subdirectory(ui)
add_subdirectory(tools)

# ---- Executable ----
qt6_add_executable(standard_of_iron
    main.cpp
)

# ---- QML module ----
qt6_add_qml_module(standard_of_iron
    URI StandardOfIron
    VERSION 1.0
    QML_FILES
        ui/qml/Main.qml
        ui/qml/HUD.qml
        ui/qml/GameView.qml
    RESOURCES
        assets/shaders/basic.vert
        assets/shaders/basic.frag
        assets/maps/test_map.txt
    DEPENDENCIES
        Qt6::QuickControls2
)

target_link_libraries(standard_of_iron
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
)

# Copy assets next to the binary for dev runs
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
