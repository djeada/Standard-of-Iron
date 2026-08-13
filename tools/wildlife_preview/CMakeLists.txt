add_executable(wildlife_preview main.cpp)

target_link_libraries(
    wildlife_preview
    PRIVATE
        render_gl
        engine_core
        game_systems
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(wildlife_preview PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(
    wildlife_preview
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
