add_library(
    arena_scenario_harness
    STATIC
    arena_scenario.cpp
    arena_scenario_catalog.cpp
    arena_ai_duel_scenarios.cpp
    arena_ambience_scenarios.cpp
    arena_city_scenarios.cpp
    arena_formation_scenarios.cpp
    arena_navigation_scenarios.cpp
    arena_showcase_scenarios.cpp
    arena_trailer_scenarios.cpp
    arena_wildlife_scenarios.cpp
    terrain_alignment.cpp
    # The promo camera and capture schedule. It needs no GL, so it belongs in
    # the harness rather than only inside arena_app -- which is what forced
    # tools_tests to compile promo_spec.cpp a second time to reach it.
    promo_spec.cpp
    # The ffmpeg pipe writer, here for the same reason: arena_tests asserts on
    # its backpressure, and a second compiled copy could pass while the object
    # the arena ships is broken.
    video_encoder.cpp
    # The matchup short generator: it turns "20 swordsman vs 20 archer" into a
    # scenario and a promo spec, so it needs both and no GL.
    matchup_short.cpp
)
target_include_directories(arena_scenario_harness PUBLIC "${CMAKE_SOURCE_DIR}")
# game_sim, not game_systems: every scenario is scripted, so the harness spawns
# units, issues commands and ticks combat without ever asking for an opponent, a
# mission file, a save or a camera service.
target_link_libraries(
    arena_scenario_harness
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui engine_core render_gl game_sim
)

# The floating combat and economy numbers. They live in their own library for
# the same reason the panels do: arena_tests asserts on the ticks the arena
# actually paints, not on a second compiled copy of them.
add_library(arena_feedback STATIC arena_feedback.cpp)

# The casting strip painted over cast-match promo frames. Its own library so
# arena_tests can paint it onto an image and look at the pixels the arena
# ships, and because it needs the arena typography but no GL.
add_library(arena_casting_overlay STATIC promo_casting_overlay.cpp arena_typography.cpp)
target_include_directories(
    arena_casting_overlay
    PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    arena_casting_overlay
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui arena_scenario_harness ui_shell
)
target_include_directories(
    arena_feedback
    PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    arena_feedback
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui engine_core app_core
)

# The two panels whose population rules the tests assert. Same reasoning as
# map_editor_core: the test links the panel the arena ships, not a second copy
# of its source.
add_library(arena_panels STATIC unit_panel.cpp building_panel.cpp panel_wheel_guard.cpp)
target_include_directories(arena_panels PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    arena_panels
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        arena_scenario_harness
)

# Only the sources that exist nowhere else. The scenario files and promo_spec
# come from arena_scenario_harness, the five app/core translation units from
# app_core, and widget_shell.cpp from ui_shell -- all of which this target
# already links. Listing them here as well compiled a private copy of each into
# the arena, so a review run in the arena could disagree with the game about
# camera framing, world bootstrap or the RTS action model.
set(ARENA_APP_SOURCES
    main.cpp
    arena_typography.cpp
    arena_window.cpp
    arena_viewport.cpp
    terrain_panel.cpp
    prop_panel.cpp
    promo_runner.cpp
    arena_audio_recorder.cpp
)
# arena_typography.cpp moved into arena_casting_overlay, which arena_app links.
list(REMOVE_ITEM ARENA_APP_SOURCES arena_typography.cpp)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(arena_app ${ARENA_APP_SOURCES})
else()
    add_executable(arena_app ${ARENA_APP_SOURCES})
endif()

target_include_directories(arena_app PRIVATE "${CMAKE_SOURCE_DIR}")

target_link_libraries(
    arena_app
    PRIVATE
        ui_shell
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::OpenGL
        $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::OpenGLWidgets>:Qt${QT_VERSION_MAJOR}::OpenGLWidgets>
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
        audio_system
        app_core
        arena_scenario_harness
        arena_panels
        arena_feedback
        arena_casting_overlay
)

set_target_properties(
    arena_app
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

# Arena loads loose shaders beside its executable. Keep every renderer shader
# in sync so visual reviews exercise current source instead of the configure-
# time asset snapshot.
file(
    GLOB _arena_shader_sources
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/assets/shaders/*.frag"
)
set(_arena_shader_outputs)
foreach(_source IN LISTS _arena_shader_sources)
    get_filename_component(_shader "${_source}" NAME)
    set(_output "${CMAKE_BINARY_DIR}/bin/assets/shaders/${_shader}")
    add_custom_command(
        OUTPUT "${_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_source}" "${_output}"
        DEPENDS "${_source}"
        VERBATIM
    )
    list(APPEND _arena_shader_outputs "${_output}")
endforeach()
add_custom_target(arena_shaders DEPENDS ${_arena_shader_outputs})
# Shader includes are globbed for the same reason as the shaders themselves:
# a new .glsl helper must reach the arena without anyone remembering to list it.
file(
    GLOB _arena_shader_include_sources
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/shaders/include/*.glsl"
)
set(_arena_shader_include_outputs)
foreach(_include_source IN LISTS _arena_shader_include_sources)
    get_filename_component(_include_name "${_include_source}" NAME)
    set(_include_output "${CMAKE_BINARY_DIR}/bin/assets/shaders/include/${_include_name}")
    add_custom_command(
        OUTPUT "${_include_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/shaders/include"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_include_source}" "${_include_output}"
        DEPENDS "${_include_source}"
        VERBATIM
    )
    list(APPEND _arena_shader_include_outputs "${_include_output}")
endforeach()
set_property(TARGET arena_shaders APPEND PROPERTY SOURCES ${_arena_shader_include_sources})
add_custom_target(arena_shader_includes DEPENDS ${_arena_shader_include_outputs})
add_dependencies(arena_shaders arena_shader_includes)
add_dependencies(arena_app arena_shaders)

# The arena is a development playground, not something a player needs. Keep it
# out of the default install so release packages stay to the game itself; fetch
# it deliberately with `cmake --install <dir> --component tools`.
install(TARGETS arena_app RUNTIME DESTINATION . COMPONENT tools EXCLUDE_FROM_ALL)
