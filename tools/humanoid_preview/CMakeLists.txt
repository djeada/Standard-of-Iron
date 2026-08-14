add_executable(humanoid_preview main.cpp)

# soi_software_raster rather than render_gl: the preview wants the rasteriser
# the renderer uses, not a GL context. It links the same objects render_gl does
# instead of compiling software_rasterizer.cpp a second time.
target_link_libraries(
    humanoid_preview
    PRIVATE
        animation_core
        soi_software_raster
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(humanoid_preview PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(
    humanoid_preview
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
