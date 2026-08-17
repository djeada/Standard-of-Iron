cmake_minimum_required(VERSION 3.21)
project(StandardOfIron VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Clang-Tidy integration ----
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)

if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
    # Prefer using .clang-tidy config, but define fallback checks for safety
    set(CLANG_TIDY_ARGS "-header-filter=^(?!.*third_party/).*/Standard-of-Iron/.*")
    set(CLANG_TIDY_FILTER "${CMAKE_SOURCE_DIR}/tools/clang_tidy_filter.sh")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-tidy")
        list(
            APPEND CLANG_TIDY_ARGS
            "-checks=bugprone-*,performance-*,readability-*,modernize-*,cppcoreguidelines-*,clang-analyzer-*"
        )
    endif()
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_FILTER} ${CLANG_TIDY_EXE} ${CLANG_TIDY_ARGS})
elseif(ENABLE_CLANG_TIDY)
    message(WARNING "clang-tidy requested but not found! Static analysis skipped.")
else()
    message(STATUS "clang-tidy disabled (ENABLE_CLANG_TIDY=OFF)")
endif()

# ---- Sanitizer and coverage lanes (used by the weekly CI workflow) ----
# ENABLE_SANITIZER accepts a comma-separated -fsanitize list, e.g.
#   cmake -DENABLE_SANITIZER=address,undefined ..
set(ENABLE_SANITIZER
    ""
    CACHE STRING
    "Comma-separated sanitizer list (address, undefined, thread, ...)"
)
option(ENABLE_COVERAGE "Instrument the build for coverage reporting" OFF)

if(ENABLE_SANITIZER)
    if(MSVC)
        message(WARNING "ENABLE_SANITIZER is not supported with MSVC; ignoring")
    else()
        message(STATUS "Sanitizers enabled: ${ENABLE_SANITIZER}")
        add_compile_options(
            -fsanitize=${ENABLE_SANITIZER}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -g
        )
        add_link_options(-fsanitize=${ENABLE_SANITIZER})
        set(SOI_INSTRUMENTED_BUILD ON)
    endif()
endif()

if(ENABLE_COVERAGE)
    if(MSVC)
        message(WARNING "ENABLE_COVERAGE is not supported with MSVC; ignoring")
    else()
        message(STATUS "Coverage instrumentation enabled")
        add_compile_options(--coverage -g -O0 -fprofile-update=atomic)
        add_link_options(--coverage)
        set(SOI_INSTRUMENTED_BUILD ON)
    endif()
endif()

# A sanitized or coverage-instrumented build runs several times slower than the
# one players get. Assertions about wall-clock behaviour measure the
# instrumentation in that configuration rather than the game, so the few places
# that make them ask for this first. It is not a feature toggle: nothing
# changes about what the code does, only about which timing claims are
# meaningful to check.
if(SOI_INSTRUMENTED_BUILD)
    add_compile_definitions(SOI_INSTRUMENTED_BUILD=1)
endif()

# Prefer Qt6 if both Qt5/Qt6 are installed
set(QT_DEFAULT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Windows-specific definitions ----
if(WIN32)
    add_compile_definitions(NOMINMAX) # Prevent min/max macros from windows.h
endif()

# Optimised builds are stripped so shipped artifacts carry no debug info. A
# sampling profiler needs the symbol table, so let a profiling build opt out.
option(SOI_KEEP_SYMBOLS "Keep the symbol table in optimised builds" OFF)

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
        add_compile_options(-ggdb3) # Maximum debug info for GDB
        add_compile_options(-fno-omit-frame-pointer) # Keep frame pointers for better backtraces
        add_compile_options(-fno-inline) # Don't inline functions for easier debugging
        # Disable optimizations that make debugging harder
        add_compile_options(-fno-optimize-sibling-calls)
    endif()
    message(STATUS "Building in DEBUG mode with GDB support")
else()
    # Release/RelWithDebInfo: optimize for performance
    # ---- Floating point ----
    # The codebase guards roughly forty inputs with std::isfinite: volumes read
    # back from the settings file, camera angles, impact geometry. Those guards
    # only exist if the compiler is not allowed to assume operands are finite.
    #
    # Both -ffast-math and MSVC's /fp:fast grant exactly that permission, so an
    # optimised build folds every one of those checks to "true" and the clamp
    # behind it disappears. It is invisible in development because Debug builds
    # do not use these flags, and it diverges per platform, which is worse than
    # being uniformly wrong. Clang says so out loud (-Wnan-infinity-disabled);
    # GCC and MSVC do it silently.
    #
    # GCC and Clang can keep the rest of fast math and give the finiteness
    # assumption back with -fno-finite-math-only. MSVC has no such sub-switch,
    # so Windows uses /fp:precise -- the same reasoning, and the only setting
    # there that keeps the guards alive.
    if(MSVC)
        # MSVC Release flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Oi /Ot /GL /fp:precise")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    else()
        # GCC/Clang Release flags
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -O3 -ffast-math -fno-finite-math-only -funroll-loops -ftree-vectorize"
        )
        # Keep release artifacts on the compiler's portable architecture
        # baseline. CI-runner-specific -march flags can make packaged builds
        # crash on otherwise supported user CPUs.
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # Enable LTO

    # Give the linker per-symbol granularity so unreferenced code is dropped
    # rather than carried, and leave no debug info in the shipped artifact.
    # Keep an unstripped copy from the build tree if you need to symbolise a
    # crash report; -g is still available through RelWithDebInfo.
    if(NOT MSVC)
        add_compile_options(-ffunction-sections -fdata-sections)
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        elseif(SOI_KEEP_SYMBOLS)
            add_link_options(-Wl,--gc-sections)
        else()
            add_link_options(-Wl,--gc-sections -s)
        endif()
    endif()
    message(STATUS "Building in RELEASE mode with optimizations")
endif()

# ---- Portability warnings ----
# The game is developed on Linux/GCC and shipped on macOS/AppleClang and
# Windows/MSVC. This option turns on the warnings that catch, on Linux, the
# constructs that behave differently on those other two toolchains. It is OFF
# by default so a normal build stays quiet; CI turns it on (see `make
# portability` and the portability job in quality.yml).
#
# The set is split in two deliberately:
#
#   * ERRORS are the diagnostics that indicate real divergence between
#     compilers -- unspecified evaluation order, dangling references, missing
#     returns, disabled NaN handling. Every one of these is at zero, so the
#     gate is meaningful: a new hit is a regression, not a backlog item.
#   * The rest stay warnings. There are ~2,700 of them (mostly -Wswitch and
#     -Wmissing-field-initializers) and they are stylistic, not portability
#     signal. Promoting them would drown the ones that matter.
#
# Anything suppressed here is suppressed because it is noise, never because a
# real finding was inconvenient.
option(SOI_STRICT_WARNINGS "Compile with the cross-platform portability warning set" OFF)
if(SOI_STRICT_WARNINGS)
    if(MSVC)
        add_compile_options(
            /W4
            /permissive- # Reject the Microsoft language extensions
            /we4715 # Not all control paths return a value
            /we4172 # Returning the address of a local
            /we4239 # Non-standard binding of a temporary to a reference
            /wd4100 # Unreferenced formal parameter
            /wd4189 # Local variable initialised but not referenced
            /wd4244 # Narrowing conversion
            /wd4267 # size_t narrowing
            /wd4505 # Unreferenced local function
        )
    else()
        add_compile_options(-Wall -Wextra)
        # -Werror=unused-result belongs to this set on merit but cannot live
        # here: moc emits `_t->some_getter();` for every invokable, so a
        # [[nodiscard]] getter on a Q_OBJECT fails the build from generated
        # code we do not own, and 42 headers have that shape. It is enforced in
        # scripts/check-portability.py instead, which only sees our own
        # translation units.
        add_compile_options(
            -Werror=return-type
            -Werror=sign-compare
            -Werror=range-loop-construct
            -Wno-switch
            -Wno-missing-field-initializers
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-unused-function
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(
                # The Apple toolchain is Clang, so these are the ones that
                # speak for macOS.
                -Werror=unsequenced
                -Werror=dangling-gsl
                -Werror=inconsistent-missing-override
                # Fires if anyone reinstates -ffast-math without
                # -fno-finite-math-only and silently deletes the isfinite
                # guards again. See the floating point note above.
                -Werror=nan-infinity-disabled
                -Wno-unused-private-field
                -Wno-unused-lambda-capture
                -Wno-unused-local-typedef
                -Wno-mismatched-tags
                -Wno-shadow-field
                -Wno-range-loop-bind-reference
            )
        else()
            add_compile_options(
                -Werror=sequence-point
                -Wno-unused-but-set-variable
                -Wno-unused-local-typedefs
            )
            if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
                add_compile_options(-Werror=dangling-reference)
            endif()
        endif()
    endif()
    message(STATUS "Portability warning set enabled (SOI_STRICT_WARNINGS)")
endif()

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(
    ENABLE_GENERATED_CAMPAIGN_MAP_ASSETS
    "Embed the campaign map assets required by the shipped campaign UI"
    ON
)

# Runtime outputs from tools/map_pipeline. They are committed so that a build
# never depends on the Natural Earth or NOAA hosts; release workflows still run
# scripts/generate-campaign-map.py first to refresh them where the sources are
# reachable. The configure-time checks below prevent any build from silently
# shipping the campaign UI without them. hannibal_path.json is authored
# separately.
set(GENERATED_CAMPAIGN_MAP_RESOURCES
    assets/campaign_map/campaign_base_color.png
    assets/campaign_map/campaign_water.png
    assets/campaign_map/coastlines_uv.json
    assets/campaign_map/rivers_uv.json
    assets/campaign_map/land_mesh.bin
    assets/campaign_map/provinces.json
    assets/campaign_map/terrain_height.png
    assets/campaign_map/terrain_height.json
)

set(GENERATED_CAMPAIGN_MAP_QRC)
if(ENABLE_GENERATED_CAMPAIGN_MAP_ASSETS)
    message(STATUS "Generated campaign map assets enabled")
    set(GENERATED_CAMPAIGN_MAP_QRC "${CMAKE_CURRENT_BINARY_DIR}/campaign_map_generated.qrc")
    # Absolute paths plus an alias keep the runtime lookup at
    # ":/assets/campaign_map/..." even though the .qrc lives in the build tree.
    set(_campaign_map_qrc_entries "")
    foreach(_campaign_map_resource IN LISTS GENERATED_CAMPAIGN_MAP_RESOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_campaign_map_resource}")
            message(
                FATAL_ERROR
                "Required campaign map runtime asset is missing: ${_campaign_map_resource}"
            )
        endif()
        string(
            APPEND _campaign_map_qrc_entries
            "        <file alias=\"${_campaign_map_resource}\">${CMAKE_CURRENT_SOURCE_DIR}/${_campaign_map_resource}</file>\n"
        )
    endforeach()
    file(
        WRITE "${GENERATED_CAMPAIGN_MAP_QRC}"
        "<RCC>\n    <qresource prefix=\"/\">\n${_campaign_map_qrc_entries}    </qresource>\n</RCC>\n"
    )
else()
    message(STATUS "Generated campaign map assets disabled")
    set(GENERATED_CAMPAIGN_MAP_RESOURCES)
endif()

# ---- Google Test Setup ----
option(BUILD_TESTING "Build and register the test suite" ON)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # GoogleTest installs its headers and static libraries by default, which would
    # otherwise land in the shipped package: a player build has no use for
    # include/gtest or libgtest.a.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# engine core moved under game; no separate engine subdir target
# Layering guard.  render/ reads game state to draw it; the simulation must not
# depend on the renderer.  game_systems does not link render_gl, so symbol-level
# violations already fail to link -- this catches header-only ones at build time
# and names the offending file instead of producing a confusing link error.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(
        check_layering
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-layering.py ${CMAKE_SOURCE_DIR}
        COMMENT "Checking layer dependencies"
        VERBATIM
    )
    # The boundaries *inside* game/. Every module in scripts/module_rules.json
    # is its own CMake target (or shares one with a neighbour it is allowed to
    # use), so the linker enforces the split at symbol level; this is the
    # header-level counterpart, and the only thing that separates the modules
    # that still share an archive. It also fails if game/CMakeLists.txt puts a
    # source in a target its module does not name.
    add_custom_target(
        check_modules
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-modules.py ${CMAKE_SOURCE_DIR}
        COMMENT "Checking module boundaries inside game/"
        VERBATIM
    )
    # The renderer draws the match it was handed and does not resolve the
    # ambient session for itself. That is the precondition for a client
    # rendering state it received rather than state it simulated, so it is a
    # build failure rather than a review comment. See render/world_view.h.
    add_custom_target(
        check_render_boundary
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-render-boundary.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the render/simulation boundary"
        VERBATIM
    )
    # The client (app/, ui/) submits orders as Game::Command payloads and never
    # applies them itself. That is the precondition for the same client driving
    # a local match, a remote one and a replay, so it is a build failure too.
    # See scripts/check-command-boundary.py.
    add_custom_target(
        check_command_boundary
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-command-boundary.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the client/command boundary"
        VERBATIM
    )
    # Ambient `X::instance()` lookups are the last thing standing between one
    # match per process and several; their number may only go down. See
    # scripts/check-ambient-instances.py and scripts/ambient_instance_budget.json.
    add_custom_target(
        check_ambient_instances
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-ambient-instances.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the ambient-lookup budget"
        VERBATIM
    )
else()
    message(STATUS "Python3 not found; skipping the layering and module checks")
endif()

add_subdirectory(animation)
# ---- Translation support ----
# Define default language (can be overridden with -DDEFAULT_LANG=de)
if(NOT DEFINED DEFAULT_LANG)
    set(DEFAULT_LANG "en")
endif()
add_compile_definitions(DEFAULT_LANG="${DEFAULT_LANG}")

# Strings the player reads that live in assets/ instead of source. lupdate only
# parses code, so scripts/extract-asset-strings.py mirrors them into a generated
# stub it can read; the stub is compiled here so a malformed extraction fails at
# build time instead of silently producing an empty catalogue.
#
# The `translations` / `translations-check` Makefile targets drive the
# extraction and the lupdate run. See game/util/asset_text.h.
#
# This one stays tracked on purpose, unlike the .qm below: it is the only place
# a reviewer can see that a change to a mission or map file altered player-facing
# text, and `translations-check` diffs it to catch asset text nobody extracted.
set(SOI_ASSET_STRINGS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/translations/asset_strings_generated.cpp")

# .qm catalogues are compiled from the tracked .ts at build time rather than
# committed. They are derived binaries: unreviewable in a diff, a recurring
# merge conflict, and a standing invitation to ship a .qm that no longer matches
# its .ts. lrelease is already a declared dependency (scripts/setup-deps.sh
# installs qt6-tools), so requiring it costs nothing.
set(SOI_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_en.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_de.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_es.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_pt_br.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_ar.ts"
)

# Some distributions ship the lrelease binary without the LinguistTools CMake
# package, so fall back to locating it directly before giving up.
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
    set(SOI_LRELEASE Qt${QT_VERSION_MAJOR}::lrelease)
else()
    get_target_property(_qt_core_location Qt${QT_VERSION_MAJOR}::Core LOCATION)
    get_filename_component(_qt_lib_dir "${_qt_core_location}" DIRECTORY)
    find_program(
        SOI_LRELEASE
        NAMES lrelease-qt${QT_VERSION_MAJOR} lrelease
        HINTS "${_qt_lib_dir}/../bin" "${_qt_lib_dir}/qt${QT_VERSION_MAJOR}/bin"
        PATHS /usr/lib/qt${QT_VERSION_MAJOR}/bin /usr/lib/qt${QT_VERSION_MAJOR}/libexec
    )
    if(NOT SOI_LRELEASE)
        message(
            FATAL_ERROR
            "lrelease not found. It compiles translations/*.ts into the .qm "
            "catalogues embedded in the binary, which are no longer tracked in "
            "git. Install the Qt Linguist tools (Debian/Ubuntu: "
            "qt6-tools-dev-tools, Arch: qt6-tools, Fedora: qt6-qttools-devel) "
            "or run scripts/setup-deps.sh, then re-run CMake."
        )
    endif()
endif()

set(SOI_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
file(MAKE_DIRECTORY "${SOI_QM_DIR}")

set(SOI_QM_FILES)
set(_soi_qm_qrc_entries "")
foreach(_ts IN LISTS SOI_TS_FILES)
    get_filename_component(_ts_name "${_ts}" NAME_WE)
    set(_qm "${SOI_QM_DIR}/${_ts_name}.qm")
    add_custom_command(
        OUTPUT "${_qm}"
        COMMAND ${SOI_LRELEASE} -silent "${_ts}" -qm "${_qm}"
        DEPENDS "${_ts}"
        COMMENT "Compiling ${_ts_name}.qm"
        VERBATIM
    )
    list(APPEND SOI_QM_FILES "${_qm}")
    # Absolute path plus an alias keeps the runtime lookup at
    # ":/translations/app_xx.qm" even though the .qm lives in the build tree.
    string(APPEND _soi_qm_qrc_entries "        <file alias=\"${_ts_name}.qm\">${_qm}</file>\n")
endforeach()

set(SOI_TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations_generated.qrc")
file(
    WRITE "${SOI_TRANSLATIONS_QRC}"
    "<RCC>\n    <qresource prefix=\"/translations\">\n${_soi_qm_qrc_entries}    </qresource>\n</RCC>\n"
)
# AUTORCC reads the .qrc at build time but does not know the listed files are
# generated, so the dependency has to be spelled out or a clean build races.
set_source_files_properties("${SOI_TRANSLATIONS_QRC}" PROPERTIES OBJECT_DEPENDS "${SOI_QM_FILES}")
add_custom_target(translations_qm DEPENDS ${SOI_QM_FILES})

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

# ---- Icon resources ----
# The whole tracked icon directory is embedded rather than a hand-kept list: a
# missing entry only shows up as a blank image at runtime, which is exactly how
# marketplace.png went unnoticed. These files are tracked in git, so the glob is
# reproducible on CI.
file(
    GLOB SOI_UNIT_ICON_ABS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/visuals/icons/*.png"
)
if(NOT SOI_UNIT_ICON_ABS)
    message(FATAL_ERROR "No icons found under assets/visuals/icons")
endif()
set(SOI_UNIT_ICON_RESOURCES)
foreach(_icon IN LISTS SOI_UNIT_ICON_ABS)
    file(RELATIVE_PATH _icon_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_icon}")
    list(APPEND SOI_UNIT_ICON_RESOURCES "${_icon_rel}")
endforeach()
list(SORT SOI_UNIT_ICON_RESOURCES)

# ---- Application layer ----
# The composition root, view models and input adapters, built once as a library
# so the executable and the test suite consume the same binary instead of each
# recompiling the same translation units with their own flags. Tests that
# recompiled these files could disagree with the shipped build; linking one
# target removes that whole class of drift.
add_library(
    app_core
    STATIC
    # app/core -- the composition root and the frame it drives.
    app/core/frame_ui_coordinator.cpp
    app/core/game_engine.cpp
    app/core/language_manager.cpp
    app/core/runtime_frame_orchestrator.cpp
    # app/session -- bringing one match up: world, renderer, level, skirmish.
    app/session/environment.cpp
    app/session/level_loader.cpp
    app/session/level_orchestrator.cpp
    app/session/loading_progress_tracker.cpp
    app/session/renderer_bootstrap.cpp
    app/session/skirmish_loader.cpp
    app/session/skirmish_runtime_coordinator.cpp
    app/session/world_bootstrap.cpp
    # app/mission -- campaigns, mission scripts, waves and the tutorial.
    app/mission/campaign_manager.cpp
    app/mission/mission_commander_setup.cpp
    app/mission/mission_definition_view.cpp
    app/mission/mission_setup_coordinator.cpp
    app/mission/mission_wave_director.cpp
    app/mission/mission_waves.cpp
    app/mission/tutorial_director.cpp
    # app/commander -- the first-person control mode and its camera.
    app/commander/commander_camera_rig.cpp
    app/commander/commander_control_controller.cpp
    app/commander/commander_mode_coordinator.cpp
    app/commander/commander_status_builder.cpp
    # app/input -- pointer and key gestures, before they become orders.
    app/input/cursor_manager.cpp
    app/input/hover_tracker.cpp
    app/input/input_command_handler.cpp
    app/input/rts_camera_controller.cpp
    # app/orders -- gestures turned into Game::Command payloads, and the
    # feedback the player gets back for them.
    app/orders/action_vfx.cpp
    app/orders/army_formation_controller.cpp
    app/orders/command_controller.cpp
    app/orders/movement_utils.cpp
    app/orders/order_feedback.cpp
    app/orders/order_issuer.cpp
    app/orders/order_markers.cpp
    app/orders/order_submission.cpp
    app/orders/rts_action_model.cpp
    # app/economy -- production, trade and the resource coach.
    app/economy/economy_overview.cpp
    app/economy/harvest_targeting.cpp
    app/economy/production_manager.cpp
    app/economy/production_readouts.cpp
    app/economy/resource_text.cpp
    app/economy/unit_profile.cpp
    # app/persistence -- saving and restoring a match.
    app/persistence/game_state_restorer.cpp
    app/persistence/save_load_coordinator.cpp
    # app/world -- world state read back as client presentation state.
    app/world/ambient_state_manager.cpp
    app/world/combat_feedback.cpp
    app/world/focus_target.cpp
    app/world/minimap_manager.cpp
    app/world/selection_query_service.cpp
    app/world/unit_queries.cpp
    app/world/visibility_coordinator.cpp
    # app/audio -- cue routing and the QML-facing audio proxy.
    app/audio/audio_coordinator.cpp
    app/audio/audio_resource_loader.cpp
    app/audio/audio_system_proxy.cpp
    app/audio/weather_audio.cpp
    # app/viewmodels -- the QML API, one coherent slice per model.
    app/viewmodels/activity_view_model.cpp
    app/viewmodels/camera_view_model.cpp
    app/viewmodels/commander_view_model.cpp
    app/viewmodels/economy_view_model.cpp
    app/viewmodels/match_setup_view_model.cpp
    app/viewmodels/minimap_view_model.cpp
    app/viewmodels/mission_view_model.cpp
    app/viewmodels/orders_view_model.cpp
    app/viewmodels/placement_view_model.cpp
    app/viewmodels/production_view_model.cpp
    app/viewmodels/save_slots_view_model.cpp
    app/viewmodels/wave_view_model.cpp
    # app/models -- Qt item models and image providers.
    app/models/graphics_settings_proxy.cpp
    app/models/map_preview_image_provider.cpp
    app/models/minimap_image_provider.cpp
    app/models/selected_units_model.cpp
    app/models/selection_grouping.cpp
)

target_include_directories(app_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    app_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Sql
        $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::Multimedia>:Qt${QT_VERSION_MAJOR}::Multimedia>
        engine_core
        render_gl
        game_sim
        game_view
        # The application is the layer that legitimately needs all of gameplay:
        # it starts missions, restores saves, installs the system registry and
        # hands the AI its opponents. Named one by one rather than through the
        # game_systems facade so this list stays a true statement of what the
        # composition root touches -- and so removing a responsibility from it
        # shows up here.
        soi_ai
        soi_runtime
        soi_missions
        soi_campaign
        soi_persistence
        audio_system
)
set_target_properties(app_core PROPERTIES AUTOMOC ON)

# The name docs/ARCHITECTURE.md gives this layer. See the alias note in
# game/CMakeLists.txt.
add_library(soi_app ALIAS app_core)

# ---- Executable ----
qt6_add_executable(standard_of_iron
    main.cpp
    ui/campaign_map_view.cpp
    ui/gl_view.cpp
    ${SOI_ASSET_STRINGS_CPP}
)

# ---- QML module ----
if(QT_VERSION_MAJOR EQUAL 6)
    # Compile the complete resource collections into the executable. Listing a
    # .qrc file under qt_add_qml_module(RESOURCES) only embeds the XML file; it
    # does not register the files referenced by that collection at runtime.
    target_sources(
        standard_of_iron
        PRIVATE
            assets.qrc
            design_resources.qrc
            ${SOI_TRANSLATIONS_QRC}
            ${GENERATED_CAMPAIGN_MAP_QRC}
    )

    # qt6_add_qml_module writes the module's qmldir itself, and a `pragma
    # Singleton` file only becomes a singleton there when the source file carries
    # this property. Without it the type registers as an ordinary component and
    # every `StyleGuide.x` style reference silently evaluates to undefined.
    set_source_files_properties(
        ui/qml/StyleGuide.qml
        ui/qml/EconomyGuide.qml
        ui/qml/CameraGuide.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE
    )

    qt6_add_qml_module(standard_of_iron
        URI StandardOfIron
        VERSION 1.0
        QML_FILES
            ui/qml/Main.qml
            ui/qml/MainMenu.qml
            ui/qml/MapSelect.qml
            ui/qml/MapPreview.qml
            ui/qml/SkirmishChip.qml
            ui/qml/MapListPanel.qml
            ui/qml/PlayerListItem.qml
            ui/qml/PlayerConfigPanel.qml
            ui/qml/CampaignScreen.qml
            ui/qml/MissionListItem.qml
            ui/qml/MissionDetailPanel.qml
            ui/qml/MediterraneanMapPanel.qml
            ui/qml/StyleGuide.qml
            ui/qml/StyledButton.qml
            ui/qml/StyledComboBox.qml
            ui/qml/HUD.qml
            ui/qml/HUDTop.qml
            ui/qml/CameraGuide.qml
            ui/qml/CameraLegend.qml
            ui/qml/EconomyGuide.qml
            ui/qml/EconomyCoach.qml
            ui/qml/EconomyHelpPanel.qml
            ui/qml/WaveTracker.qml
            ui/qml/HUDBottom.qml
            ui/qml/HUDBottomCommander.qml
            ui/qml/UnitInspectPanel.qml
            ui/qml/RecruitCard.qml
            ui/qml/RpgFpvOverlay.qml
            ui/qml/RpgDamageNumbers.qml
            ui/qml/CombatDamageNumbers.qml
            ui/qml/FormationPanel.qml
            ui/qml/FormationShape.qml
            ui/qml/FormationReadoutCell.qml
            ui/qml/FormationOptionRow.qml
            ui/qml/FormationStatusBadge.qml
            ui/qml/ProductionPanel.qml
            ui/qml/SaveGamePanel.qml
            ui/qml/LoadGamePanel.qml
            ui/qml/SaveProgressOverlay.qml
            ui/qml/ObjectivesPanel.qml
            ui/qml/HelpPanel.qml
            ui/qml/TutorialOverlay.qml
            ui/qml/SettingsPanel.qml
            ui/qml/ControlsBindingList.qml
            ui/qml/HUDVictory.qml
            ui/qml/BattleSummary.qml
            ui/qml/GameView.qml
            ui/qml/CommanderInputLayer.qml
            ui/qml/RtsInputLayer.qml
            ui/qml/CursorManager.qml
            ui/qml/LoadScreen.qml
        RESOURCES
            ui/qml/ui_audio.js
        assets/shaders/include/environment_lighting.glsl
        assets/shaders/include/local_lighting.glsl
        assets/shaders/include/directional_shadows.glsl
        assets/shaders/include/visibility_mask.glsl
        assets/shaders/include/noise.glsl
        assets/shaders/directional_shadow_depth.vert
        assets/shaders/directional_shadow_rigged.vert
        assets/shaders/directional_shadow_depth.frag
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
            assets/shaders/pine_instanced.frag
            assets/shaders/pine_instanced.vert
            assets/shaders/plant_instanced.frag
            assets/shaders/plant_instanced.vert
            assets/shaders/river.frag
            assets/shaders/river.vert
            assets/shaders/riverbank.frag
            assets/shaders/riverbank.vert
            assets/shaders/stone_instanced.frag
            assets/shaders/stone_instanced.vert
            assets/shaders/tent_instanced.frag
            assets/shaders/tent_instanced.vert
            assets/shaders/supply_cart_instanced.frag
            assets/shaders/supply_cart_instanced.vert
            assets/shaders/weapon_rack_instanced.frag
            assets/shaders/weapon_rack_instanced.vert
            assets/shaders/ruins_instanced.frag
            assets/shaders/ruins_instanced.vert
            assets/shaders/dead_tree_instanced.frag
            assets/shaders/dead_tree_instanced.vert
            assets/shaders/terrain_chunk.frag
            assets/shaders/terrain_chunk.vert
            assets/maps/map_forest.json
            assets/maps/map_rivers.json
            assets/maps/map_mountain.json
            assets/campaigns/second_punic_war.json
            assets/campaign_map/campaign_state.json
            assets/visuals/unit_visuals.json
            assets/visuals/unit_equipment_loadouts.json
            assets/visuals/emblems/rome.png
            assets/visuals/emblems/cartaghe.png
            assets/visuals/load_screen.png
            assets/visuals/hannibal.png
            assets/visuals/scipio.png
            assets/visuals/standard_of_iron.png
            ${SOI_UNIT_ICON_RESOURCES}
        DEPENDENCIES
            Qt6::QuickControls2
    )
endif()

add_dependencies(standard_of_iron translations_qm)

target_link_libraries(
    standard_of_iron
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Sql
        $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::Multimedia>:Qt${QT_VERSION_MAJOR}::Multimedia>
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
        audio_system
        app_core
        ui_shell
)

add_dependencies(
    standard_of_iron
    engine_core
    render_gl
    game_sim
    game_view
    audio_system
    app_core
)

# The version reaches the running game from exactly one place: the project()
# call above. main.cpp feeds it to QCoreApplication::applicationVersion(), and
# QML reads it back as Qt.application.version. Nothing else may spell it out --
# a hard-coded copy in a .qml or a .ts file silently outlives the tag it was
# written for.
target_compile_definitions(standard_of_iron PRIVATE SOI_VERSION="${PROJECT_VERSION}")

# Windows-specific OpenGL linking
if(WIN32)
    target_link_libraries(standard_of_iron PRIVATE opengl32)
    # Ensure we have access to wglGetProcAddress and related functions
    target_compile_definitions(standard_of_iron PRIVATE NOMINMAX)
endif()

target_link_libraries(standard_of_iron PRIVATE Qt6::QuickControls2)

set_target_properties(
    standard_of_iron
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(APPLE)
    # The macOS release workflow deploys, signs, and packages the application
    # as a bundle. Without this property CMake emits only a bare executable,
    # so macdeployqt cannot find the expected .app directory.
    set_target_properties(standard_of_iron PROPERTIES MACOSX_BUNDLE TRUE)
endif()
if(WIN32)
    set_target_properties(standard_of_iron PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# What a player receives. `cmake --install` with no component argument
# produces exactly the game and the content it loads at runtime -- notably the
# music and ambience, which are deliberately not embedded in the executable.
install(TARGETS standard_of_iron RUNTIME DESTINATION . BUNDLE DESTINATION . COMPONENT runtime)
install(DIRECTORY assets/ DESTINATION assets COMPONENT runtime)

if(TARGET bake_creature_assets)
    add_dependencies(standard_of_iron bake_creature_assets)
endif()

if(TARGET synthesize_ambience_assets)
    add_dependencies(standard_of_iron synthesize_ambience_assets)
endif()

# Stage assets next to the binary for dev runs.
#
# The runtime resolves ":/assets/..." against the filesystem before the Qt
# resource system, so this staged copy -- not the repo tree, and not the qrc --
# is what a dev build actually reads. It used to be a bare configure-time
# `file(COPY)`, which meant editing a shader or a map and rebuilding changed
# nothing at all until someone happened to re-run CMake. Staging on every build
# makes an edit take effect when you build, which is the only behaviour anyone
# expects.
file(
    GLOB_RECURSE SOI_RUNTIME_ASSET_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/bin/assets"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    DEPENDS ${SOI_RUNTIME_ASSET_SOURCES}
    COMMENT "Staging assets into bin/assets"
    VERBATIM
)
add_custom_target(stage_runtime_assets DEPENDS "${CMAKE_BINARY_DIR}/bin/assets/.stamp")
add_dependencies(standard_of_iron stage_runtime_assets)
if(TARGET synthesize_ambience_assets)
    add_dependencies(stage_runtime_assets synthesize_ambience_assets)
endif()

# ---- clang-format helpers (optional but convenient) ----
# Provides:
#   - clang-format        : formats all C/C++ sources using .clang-format
#   - clang-format-check  : CI-style check (no changes), fails on violations
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

    file(
        GLOB_RECURSE ALL_CXX
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/*.c"
        "${CMAKE_SOURCE_DIR}/*.cpp"
        "${CMAKE_SOURCE_DIR}/*.h"
        "${CMAKE_SOURCE_DIR}/*.hpp"
    )

    # Exclude generated, vendored, and local build-tree sources.
    list(FILTER ALL_CXX EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/(build|build-[^/]*|third_party)/.*")
    list(FILTER ALL_CXX EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")

    if(ALL_CXX)
        add_custom_target(
            clang-format
            COMMAND "${CLANG_FORMAT_EXE}" -i --style=file ${ALL_CXX}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Running clang-format with .clang-format (strict)"
        )

        add_custom_target(
            clang-format-check
            COMMAND "${CLANG_FORMAT_EXE}" --dry-run -Werror --style=file ${ALL_CXX}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Checking clang-format compliance"
        )
    else()
        add_custom_target(
            clang-format
            COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to format."
        )
        add_custom_target(
            clang-format-check
            COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to check."
        )
    endif()
endif()

# ---- Content Validation ----
# Add custom target to validate missions and campaigns
add_custom_target(
    validate-content
    COMMAND content_validator "${CMAKE_SOURCE_DIR}/assets"
    DEPENDS content_validator
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Validating mission and campaign content"
)

# Optionally make validation run as part of the build
# Uncomment the following line to make build fail on invalid content:
# add_dependencies(standard_of_iron validate-content)
