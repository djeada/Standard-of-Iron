cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 0.2.0 LANGUAGES CXX)

# This file is the composition root: project-level settings, the order in which
# the pieces below are brought in, and the subdirectories. Each cmake/*.cmake
# file is include()d, not add_subdirectory()d, so it shares this scope and
# behaves exactly as if its lines were written here. Order matters (compile
# options and definitions only reach what is declared after them), so the
# includes below are in dependency order, not alphabetical.
#
# Include            Owns
# -----------------  ---------------------------------------------------------
# StaticAnalysis     clang-tidy integration (ENABLE_CLANG_TIDY)
# BuildOptions       build-speed knobs, sanitizer and coverage lanes
# CompilerFlags      Debug / Release flags, LTO, symbol stripping
# PortabilityWarnings SOI_STRICT_WARNINGS, SOI_PROFILE_ALLOCATIONS
# QtSetup            Qt6 discovery and Qt policies
# CampaignMapAssets  committed campaign map resources and their generated .qrc
# GoogleTestSetup    BUILD_TESTING and the GoogleTest fetch
# BoundaryChecks     check_* architecture targets (scripts/check-*.py)
# Translations       .ts -> .qm pipeline, translations_qm
# IconResources      SOI_UNIT_ICON_RESOURCES
# AppCore            app_core / soi_app: the application layer library
# GameExecutable     standard_of_iron, its QML module, version, bundle setup
# RuntimeAssets      install rules and stage_runtime_assets
# DevTargets         clang-format, clang-format-check, validate-content
# PrecompiledHeaders shared precompiled header (cmake/PrecompiledHeaders.cmake)
#
# Tests that read build definitions as text (release_contract_test,
# icon_resources_test, commander_control_regression_test) read this file plus
# cmake/*.cmake, so a moved section stays checked.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/StaticAnalysis.cmake)
include(cmake/BuildOptions.cmake)

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerFlags.cmake)
include(cmake/PortabilityWarnings.cmake)
include(cmake/QtSetup.cmake)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(cmake/CampaignMapAssets.cmake)
include(cmake/GoogleTestSetup.cmake)
include(cmake/BoundaryChecks.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/zstd.cmake)
add_subdirectory(animation)

include(cmake/Translations.cmake)

add_subdirectory(scene)
add_subdirectory(render)
add_subdirectory(game)
# render/ reads game state to draw it: terrain, visibility, nations, and the
# shared world placement rules.  Declared here because render/ is configured
# before game/.  This is a one-way edge -- game_sim does not link render_gl --
# so the simulation builds and runs without the renderer.
#
# game_sim and not game_systems: the renderer includes nothing from game/view/,
# and nothing from the AI, the mission loaders or the save stack. Naming the
# facade here handed all of that to every binary that draws anything, which is
# how a renderer test ended up linking the save database.
target_link_libraries(render_gl PUBLIC game_sim)

add_subdirectory(ui)
add_subdirectory(tools)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(cmake/IconResources.cmake)
include(cmake/AppCore.cmake)
include(cmake/GameExecutable.cmake)
include(cmake/RuntimeAssets.cmake)
include(cmake/DevTargets.cmake)

include(cmake/PrecompiledHeaders.cmake)
soi_precompile_common_headers()
