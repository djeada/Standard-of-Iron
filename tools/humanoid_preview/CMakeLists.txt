add_executable(
    humanoid_preview
    main.cpp
    ${CMAKE_SOURCE_DIR}/render/software/software_rasterizer.cpp
)

target_link_libraries(
    humanoid_preview
    PRIVATE animation_core Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(humanoid_preview PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(
    humanoid_preview
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
