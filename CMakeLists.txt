cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Clang-Tidy integration ----
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)

if (ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
    # Prefer using .clang-tidy config, but define fallback checks for safety
    set(CLANG_TIDY_ARGS "-header-filter=^(?!.*third_party/).*/Standard-of-Iron/.*")
    set(CLANG_TIDY_FILTER "${CMAKE_SOURCE_DIR}/tools/clang_tidy_filter.sh")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-tidy")
        list(APPEND CLANG_TIDY_ARGS
            "-checks=bugprone-*,performance-*,readability-*,modernize-*,cppcoreguidelines-*,clang-analyzer-*")
    endif()
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_FILTER} ${CLANG_TIDY_EXE} ${CLANG_TIDY_ARGS})
elseif (ENABLE_CLANG_TIDY)
    message(WARNING "clang-tidy requested but not found! Static analysis skipped.")
else()
    message(STATUS "clang-tidy disabled (ENABLE_CLANG_TIDY=OFF)")
endif()

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Windows-specific definitions ----
if(WIN32)
    add_compile_definitions(NOMINMAX)  # Prevent min/max macros from windows.h
endif()

# ---- Compiler Optimization Flags ----
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        # MSVC Debug flags
        set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
        set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    else()
        # GCC/Clang Debug flags
        set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
        set(CMAKE_C_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
        # Additional GDB-friendly flags
        add_compile_options(-ggdb3)           # Maximum debug info for GDB
        add_compile_options(-fno-omit-frame-pointer)  # Keep frame pointers for better backtraces
        add_compile_options(-fno-inline)      # Don't inline functions for easier debugging
        # Disable optimizations that make debugging harder
        add_compile_options(-fno-optimize-sibling-calls)
    endif()
    message(STATUS "Building in DEBUG mode with GDB support")
else()
    # Release/RelWithDebInfo: optimize for performance
    if(MSVC)
        # MSVC Release flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
    else()
        # GCC/Clang Release flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)  # Enable LTO
    message(STATUS "Building in RELEASE mode with optimizations")
endif()

# ---- Qt ----
# Try Qt6 first, fall back to Qt5 if not available
find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2 Sql Multimedia)
if(Qt6_FOUND)
    message(STATUS "Using Qt6")
    set(QT_VERSION_MAJOR 6)
    find_package(OpenGL REQUIRED)
else()
    message(STATUS "Qt6 not found, trying Qt5")
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Quick Qml QuickControls2 Sql Multimedia)
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

# ---- Google Test Setup ----
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# engine core moved under game; no separate engine subdir target
add_subdirectory(render)
add_subdirectory(game)
add_subdirectory(ui)
add_subdirectory(tools)
add_subdirectory(tests)

# ---- Translation support ----
# Define default language (can be overridden with -DDEFAULT_LANG=de)
if(NOT DEFINED DEFAULT_LANG)
    set(DEFAULT_LANG "en")
endif()
add_compile_definitions(DEFAULT_LANG="${DEFAULT_LANG}")

# ---- Executable ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(standard_of_iron
        main.cpp
        app/core/game_engine.cpp
        app/core/production_manager.cpp
        app/core/campaign_manager.cpp
        app/core/selection_query_service.cpp
        app/core/language_manager.cpp
        app/core/minimap_manager.cpp
        app/core/ambient_state_manager.cpp
        app/core/audio_resource_loader.cpp
        app/core/renderer_bootstrap.cpp
        app/core/level_orchestrator.cpp
        app/core/loading_progress_tracker.cpp
        app/core/input_command_handler.cpp
        app/core/camera_controller.cpp
        app/core/game_state_restorer.cpp
        app/models/audio_system_proxy.cpp
        app/models/graphics_settings_proxy.cpp
        app/models/cursor_manager.cpp
        app/models/hover_tracker.cpp
        app/models/selected_units_model.cpp
        app/models/minimap_image_provider.cpp
        app/models/map_preview_image_provider.cpp
        app/controllers/command_controller.cpp
        app/controllers/action_vfx.cpp
        app/utils/json_vec_utils.cpp
        ui/campaign_map_view.cpp
        ui/gl_view.cpp
        ui/theme.cpp
    )
else()
    add_executable(standard_of_iron
        main.cpp
        app/core/game_engine.cpp
        app/core/production_manager.cpp
        app/core/campaign_manager.cpp
        app/core/selection_query_service.cpp
        app/core/language_manager.cpp
        app/core/minimap_manager.cpp
        app/core/ambient_state_manager.cpp
        app/core/audio_resource_loader.cpp
        app/core/renderer_bootstrap.cpp
        app/core/level_orchestrator.cpp
        app/core/loading_progress_tracker.cpp
        app/core/input_command_handler.cpp
        app/core/camera_controller.cpp
        app/core/game_state_restorer.cpp
        app/models/audio_system_proxy.cpp
        app/models/graphics_settings_proxy.cpp
        app/models/cursor_manager.cpp
        app/models/hover_tracker.cpp
        app/models/selected_units_model.cpp
        app/models/minimap_image_provider.cpp
        app/models/map_preview_image_provider.cpp
        app/controllers/command_controller.cpp
        app/controllers/action_vfx.cpp
        app/utils/json_vec_utils.cpp
        ui/campaign_map_view.cpp
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
            ui/qml/MapPreview.qml
            ui/qml/MapListPanel.qml
            ui/qml/PlayerListItem.qml
            ui/qml/PlayerConfigPanel.qml
            ui/qml/CampaignMenu.qml
            ui/qml/CampaignScreen.qml
            ui/qml/MissionListItem.qml
            ui/qml/MissionDetailPanel.qml
            ui/qml/MediterraneanMapPanel.qml
            ui/qml/StyleGuide.qml
            ui/qml/StyledButton.qml
            ui/qml/HUD.qml
            ui/qml/HUDTop.qml
            ui/qml/HUDBottom.qml
            ui/qml/ProductionPanel.qml
            ui/qml/SaveGamePanel.qml
            ui/qml/LoadGamePanel.qml
            ui/qml/ObjectivesPanel.qml
            ui/qml/SettingsPanel.qml
            ui/qml/HUDVictory.qml
            ui/qml/BattleSummary.qml
            ui/qml/GameView.qml
            ui/qml/CursorManager.qml
            ui/qml/LoadScreen.qml
        RESOURCES
            assets/shaders/archer.frag
            assets/shaders/archer.vert
            assets/shaders/archer_roman_republic.frag
            assets/shaders/archer_roman_republic.vert
            assets/shaders/archer_carthage.frag
            assets/shaders/archer_carthage.vert
            assets/shaders/basic.frag
            assets/shaders/basic.vert
            assets/shaders/bridge.frag
            assets/shaders/bridge.vert
            assets/shaders/cylinder_instanced.frag
            assets/shaders/cylinder_instanced.vert
            assets/shaders/firecamp.frag
            assets/shaders/firecamp.vert
            assets/shaders/fog_instanced.frag
            assets/shaders/fog_instanced.vert
            assets/shaders/grass_instanced.frag
            assets/shaders/grass_instanced.vert
            assets/shaders/grid.frag
            assets/shaders/ground_plane.frag
            assets/shaders/ground_plane.vert
            assets/shaders/swordsman.frag
            assets/shaders/swordsman.vert
            assets/shaders/swordsman_roman_republic.frag
            assets/shaders/swordsman_roman_republic.vert
            assets/shaders/swordsman_carthage.frag
            assets/shaders/swordsman_carthage.vert
            assets/shaders/horse_swordsman.frag
            assets/shaders/horse_swordsman.vert
            assets/shaders/horse_swordsman_roman_republic.frag
            assets/shaders/horse_swordsman_roman_republic.vert
            assets/shaders/horse_swordsman_carthage.frag
            assets/shaders/horse_swordsman_carthage.vert
            assets/shaders/horse_archer_roman_republic.frag
            assets/shaders/horse_archer_roman_republic.vert
            assets/shaders/horse_archer_carthage.frag
            assets/shaders/horse_archer_carthage.vert
            assets/shaders/horse_spearman_roman_republic.frag
            assets/shaders/horse_spearman_roman_republic.vert
            assets/shaders/horse_spearman_carthage.frag
            assets/shaders/horse_spearman_carthage.vert
            assets/shaders/healer.frag
            assets/shaders/healer.vert
            assets/shaders/healer_roman_republic.frag
            assets/shaders/healer_roman_republic.vert
            assets/shaders/healer_carthage.frag
            assets/shaders/healer_carthage.vert
            assets/shaders/pine_instanced.frag
            assets/shaders/pine_instanced.vert
            assets/shaders/plant_instanced.frag
            assets/shaders/plant_instanced.vert
            assets/shaders/river.frag
            assets/shaders/river.vert
            assets/shaders/riverbank.frag
            assets/shaders/riverbank.vert
            assets/shaders/spearman.frag
            assets/shaders/spearman.vert
            assets/shaders/spearman_roman_republic.frag
            assets/shaders/spearman_roman_republic.vert
            assets/shaders/spearman_carthage.frag
            assets/shaders/spearman_carthage.vert
            assets/shaders/stone_instanced.frag
            assets/shaders/stone_instanced.vert
            assets/shaders/terrain_chunk.frag
            assets/shaders/terrain_chunk.vert
            assets/maps/map_forest.json
            assets/maps/map_rivers.json
            assets/maps/map_mountain.json
            assets/campaigns/second_punic_war.json
            assets/campaign_map/campaign_base_color.png
            assets/campaign_map/campaign_water.png
            assets/campaign_map/coastlines_uv.json
            assets/campaign_map/rivers_uv.json
            assets/campaign_map/land_mesh.bin
            assets/campaign_map/hannibal_path.json
            assets/campaign_map/campaign_state.json
            assets/campaign_map/provinces.json
            assets/visuals/unit_visuals.json
            assets/visuals/emblems/rome.png
            assets/visuals/emblems/cartaghe.png
            assets/visuals/load_screen.png
            assets/visuals/hannibal.png
            assets/visuals/icons/archer_rome.png
            assets/visuals/icons/archer_cartaghe.png
            assets/visuals/icons/swordsman_rome.png
            assets/visuals/icons/swordsman_cartaghe.png
            assets/visuals/icons/spearman_rome.png
            assets/visuals/icons/spearman_cartaghe.png
            assets/visuals/icons/horse_swordsman_rome.png
            assets/visuals/icons/horse_swordsman_cartaghe.png
            assets/visuals/icons/horse_archer_rome.png
            assets/visuals/icons/horse_archer_cartaghe.png
            assets/visuals/icons/horse_spearman_rome.png
            assets/visuals/icons/horse_spearman_cartaghe.png
            assets/visuals/icons/healer_rome.png
            assets/visuals/icons/healer_cartaghe.png
            assets/visuals/icons/catapult_rome.png
            assets/visuals/icons/catapult_cartaghe.png
            assets/visuals/icons/ballista_rome.png
            assets/visuals/icons/ballista_cartaghe.png
            assets/visuals/icons/elephant_cartaghe.png
            assets/visuals/icons/builder_rome.png
            assets/visuals/icons/builder_cartaghe.png
            assets/visuals/icons/defense_tower_rome.png
            assets/visuals/icons/defense_tower_cartaghe.png
            assets/visuals/icons/wall_rome.png
            assets/visuals/icons/wall_cartaghe.png
            assets/visuals/icons/house_rome.png
            assets/visuals/icons/house_cartaghe.png
            assets/visuals/icons/attack_mode.png
            assets/visuals/icons/defend_mode.png
            assets/visuals/icons/hold_mode.png
            assets/visuals/icons/patrol_mode.png
            assets/visuals/icons/formation_mode.png
            assets/visuals/icons/build_mode.png
            assets/visuals/icons/heal_mode.png
            assets/visuals/icons/ai_player.png
            assets/visuals/icons/human_player.png
            assets/visuals/icons/troop_count.png
            assets/visuals/icons/defeated.png
            translations/app_en.qm
            translations/app_de.qm
            translations/app_pt_br.qm
        DEPENDENCIES
            Qt6::QuickControls2
    )
else()
    qt5_add_resources(qml_resources qml_resources.qrc)
    qt5_add_resources(assets_resources assets.qrc)
    qt5_add_resources(translations_resources translations.qrc)
    target_sources(standard_of_iron PRIVATE ${qml_resources} ${assets_resources} ${translations_resources})
endif()

target_link_libraries(standard_of_iron
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Multimedia
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
        audio_system
)

# Windows-specific OpenGL linking
if(WIN32)
    target_link_libraries(standard_of_iron PRIVATE opengl32)
    # Ensure we have access to wglGetProcAddress and related functions
    target_compile_definitions(standard_of_iron PRIVATE NOMINMAX)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(standard_of_iron PRIVATE Qt6::QuickControls2)
else()
    target_link_libraries(standard_of_iron PRIVATE Qt5::QuickControls2)
endif()

set_target_properties(standard_of_iron PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if (WIN32)
  set_target_properties(standard_of_iron PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS standard_of_iron RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)

# Copy assets next to the binary for dev runs
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/bin/assets/)


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

# ---- Content Validation ----
# Add custom target to validate missions and campaigns
add_custom_target(validate-content
  COMMAND content_validator "${CMAKE_SOURCE_DIR}/assets"
  DEPENDS content_validator
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Validating mission and campaign content"
)

# Optionally make validation run as part of the build
# Uncomment the following line to make build fail on invalid content:
# add_dependencies(standard_of_iron validate-content)
