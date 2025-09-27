cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL Quick Qml)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Qt6 setup
qt6_standard_project_setup()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(render)
add_subdirectory(game)
add_subdirectory(ui)
add_subdirectory(tools)

# Main executable
qt6_add_executable(standard_of_iron
    main.cpp
)

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
)

target_link_libraries(standard_of_iron
    PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::Quick
    Qt6::Qml
    ${OPENGL_LIBRARIES}
    engine_core
    render_gl
    game_systems
)

# Copy assets to build directory
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)