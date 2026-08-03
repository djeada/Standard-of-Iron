add_library(
    arena_scenario_harness
    STATIC
    arena_scenario.cpp
    arena_scenario_catalog.cpp
    arena_formation_scenarios.cpp
    arena_showcase_scenarios.cpp
    terrain_alignment.cpp
)
target_include_directories(arena_scenario_harness PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(
    arena_scenario_harness
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui engine_core render_gl game_systems
)

# The two panels whose population rules the tests assert. Same reasoning as
# map_editor_core: the test links the panel the arena ships, not a second copy
# of its source.
add_library(arena_panels STATIC unit_panel.cpp building_panel.cpp)
target_include_directories(arena_panels PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    arena_panels
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        arena_scenario_harness
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        prop_panel.cpp
        arena_formation_scenarios.cpp
        arena_showcase_scenarios.cpp
        promo_spec.cpp
        promo_runner.cpp
        video_encoder.cpp
        ../../app/core/renderer_bootstrap.cpp
        ../../app/core/commander_control_controller.cpp
        ../../app/core/world_bootstrap.cpp
        ../../app/core/rts_action_model.cpp
        ../../app/models/activity_markers.cpp
        ../../ui/widget_shell.cpp
    )
else()
    add_executable(
        arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        prop_panel.cpp
        arena_formation_scenarios.cpp
        arena_showcase_scenarios.cpp
        promo_spec.cpp
        promo_runner.cpp
        video_encoder.cpp
        ../../app/core/renderer_bootstrap.cpp
        ../../app/core/commander_control_controller.cpp
        ../../app/core/world_bootstrap.cpp
        ../../app/core/rts_action_model.cpp
        ../../app/models/activity_markers.cpp
        ../../ui/widget_shell.cpp
    )
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
        arena_scenario_harness
        arena_panels
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
