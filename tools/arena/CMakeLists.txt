if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        unit_panel.cpp
        ../../app/core/renderer_bootstrap.cpp
    )
else()
    add_executable(arena_app
        main.cpp
        arena_window.cpp
        arena_viewport.cpp
        terrain_panel.cpp
        unit_panel.cpp
        ../../app/core/renderer_bootstrap.cpp
    )
endif()

target_include_directories(arena_app PRIVATE "${CMAKE_SOURCE_DIR}")

target_link_libraries(arena_app
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::OpenGLWidgets>:Qt${QT_VERSION_MAJOR}::OpenGLWidgets>
    ${OPENGL_LIBRARIES}
    engine_core
    render_gl
    game_systems
)

set_target_properties(arena_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS arena_app RUNTIME DESTINATION .)
