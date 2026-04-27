add_executable(bpat_baker
    main.cpp
)

target_link_libraries(bpat_baker
    PRIVATE
        render_gl
        engine_core
        game_systems
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(bpat_baker
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

set_target_properties(bpat_baker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
