add_executable(building_preview
    main.cpp
)

target_link_libraries(building_preview
    PRIVATE
        render_gl
        engine_core
        game_systems
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(building_preview
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

set_target_properties(building_preview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
