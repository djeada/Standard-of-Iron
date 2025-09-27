add_library(render_gl STATIC
    gl/shader.cpp
    gl/buffer.cpp
    gl/mesh.cpp
    gl/texture.cpp
    gl/renderer.cpp
    gl/camera.cpp
)

target_include_directories(render_gl PUBLIC .)
target_link_libraries(render_gl PUBLIC Qt6::Core Qt6::OpenGL ${OPENGL_LIBRARIES} engine_core)