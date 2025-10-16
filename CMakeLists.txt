cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 1.0.0 LANGUAGES CXX)

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Compiler Optimization Flags ----
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)  # Enable LTO
endif()

# ---- Qt ----
# Try Qt6 first, fall back to Qt5 if not available
find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2 Sql)
if(Qt6_FOUND)
    message(STATUS "Using Qt6")
    set(QT_VERSION_MAJOR 6)
    find_package(OpenGL REQUIRED)
else()
    message(STATUS "Qt6 not found, trying Qt5")
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2 Sql)
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

set(CMAKE_AUTOMOC ON)

# engine core moved under game; no separate engine subdir target
add_subdirectory(render)
add_subdirectory(game)
add_subdirectory(ui)
add_subdirectory(tools)

# ---- Executable ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(standard_of_iron
        main.cpp
        app/core/game_engine.cpp
        app/models/cursor_manager.cpp
        app/models/hover_tracker.cpp
        app/models/selected_units_model.cpp
        app/controllers/command_controller.cpp
        app/controllers/action_vfx.cpp
        app/utils/json_vec_utils.cpp
        ui/gl_view.cpp
        ui/theme.cpp
    )
else()
    add_executable(standard_of_iron
        main.cpp
        app/core/game_engine.cpp
        app/models/cursor_manager.cpp
        app/models/hover_tracker.cpp
        app/models/selected_units_model.cpp
        app/controllers/command_controller.cpp
        app/controllers/action_vfx.cpp
        app/utils/json_vec_utils.cpp
        ui/gl_view.cpp
        ui/theme.cpp
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
            ui/qml/ProductionPanel.qml
            ui/qml/SaveGamePanel.qml
            ui/qml/LoadGamePanel.qml
            ui/qml/HUDVictory.qml
            ui/qml/BattleSummary.qml
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
    qt5_add_resources(qml_resources qml_resources.qrc)
    qt5_add_resources(assets_resources assets.qrc)
    target_sources(standard_of_iron PRIVATE ${qml_resources} ${assets_resources})
endif()

target_link_libraries(standard_of_iron
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Sql
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

# ---- clang-format helpers (optional but convenient) ----
# Provides:
#   - clang-format        : formats all C/C++ sources using .clang-format
#   - clang-format-check  : CI-style check (no changes), fails on violations
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
  message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

  file(GLOB_RECURSE ALL_CXX CONFIGURE_DEPENDS
       "${CMAKE_SOURCE_DIR}/*.c"   "${CMAKE_SOURCE_DIR}/*.cpp"
       "${CMAKE_SOURCE_DIR}/*.h"   "${CMAKE_SOURCE_DIR}/*.hpp")

  # Exclude the current build tree (generated sources, moc_*.cpp, etc.)
  list(FILTER ALL_CXX EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")

  if (ALL_CXX)
    add_custom_target(clang-format
      COMMAND "${CLANG_FORMAT_EXE}" -i --style=file ${ALL_CXX}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Running clang-format with .clang-format (strict)")

    add_custom_target(clang-format-check
      COMMAND "${CLANG_FORMAT_EXE}" --dry-run -Werror --style=file ${ALL_CXX}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Checking clang-format compliance")
  else()
    add_custom_target(clang-format
      COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to format.")
    add_custom_target(clang-format-check
      COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to check.")
  endif()
endif()

