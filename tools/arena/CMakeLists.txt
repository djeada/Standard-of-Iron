add_library(
    arena_scenario_harness
    STATIC
    arena_scenario.cpp
    arena_scenario_catalog.cpp
    terrain_alignment.cpp
)
target_include_directories(arena_scenario_harness PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(
    arena_scenario_harness
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui engine_core render_gl game_systems
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        unit_panel.cpp
        building_panel.cpp
        prop_panel.cpp
        ../../app/core/renderer_bootstrap.cpp
        ../../app/core/commander_control_controller.cpp
        ../../app/core/world_bootstrap.cpp
        ../../app/core/rts_action_model.cpp
        ../../ui/theme.cpp
        ../../ui/preferences.cpp
        ../../ui/widget_shell.cpp
    )
else()
    add_executable(
        arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        unit_panel.cpp
        building_panel.cpp
        prop_panel.cpp
        ../../app/core/renderer_bootstrap.cpp
        ../../app/core/commander_control_controller.cpp
        ../../app/core/world_bootstrap.cpp
        ../../app/core/rts_action_model.cpp
        ../../ui/theme.cpp
        ../../ui/preferences.cpp
        ../../ui/widget_shell.cpp
    )
endif()

target_include_directories(arena_app PRIVATE "${CMAKE_SOURCE_DIR}")

target_link_libraries(
    arena_app
    PRIVATE
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
        arena_scenario_harness
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
set(_environment_lighting_include
    "${CMAKE_SOURCE_DIR}/assets/shaders/include/environment_lighting.glsl"
)
set(_environment_lighting_output
    "${CMAKE_BINARY_DIR}/bin/assets/shaders/include/environment_lighting.glsl"
)
set(_local_lighting_include "${CMAKE_SOURCE_DIR}/assets/shaders/include/local_lighting.glsl")
set(_local_lighting_output "${CMAKE_BINARY_DIR}/bin/assets/shaders/include/local_lighting.glsl")
set(_directional_shadows_include
    "${CMAKE_SOURCE_DIR}/assets/shaders/include/directional_shadows.glsl"
)
set(_directional_shadows_output
    "${CMAKE_BINARY_DIR}/bin/assets/shaders/include/directional_shadows.glsl"
)
add_custom_command(
    OUTPUT "${_environment_lighting_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/shaders/include"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "${_environment_lighting_include}"
        "${_environment_lighting_output}"
    DEPENDS "${_environment_lighting_include}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_local_lighting_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/shaders/include"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "${_local_lighting_include}"
        "${_local_lighting_output}"
    DEPENDS "${_local_lighting_include}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_directional_shadows_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/shaders/include"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "${_directional_shadows_include}"
        "${_directional_shadows_output}"
    DEPENDS "${_directional_shadows_include}"
    VERBATIM
)
set_property(
    TARGET arena_shaders
    APPEND
    PROPERTY
        SOURCES
            "${_environment_lighting_include}"
            "${_local_lighting_include}"
            "${_directional_shadows_include}"
)
add_custom_target(
    arena_shader_includes
    DEPENDS
        "${_environment_lighting_output}"
        "${_local_lighting_output}"
        "${_directional_shadows_output}"
)
add_dependencies(arena_shaders arena_shader_includes)
add_dependencies(arena_app arena_shaders)

install(TARGETS arena_app RUNTIME DESTINATION .)
