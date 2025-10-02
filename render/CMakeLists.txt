add_library(render_gl STATIC
    gl/shader.cpp
    gl/buffer.cpp
    gl/mesh.cpp
    gl/texture.cpp
    gl/renderer.cpp
    gl/camera.cpp
    gl/resources.cpp
    gl/bootstrap.cpp
    entity/registry.cpp
    entity/archer_renderer.cpp
    entity/barracks_renderer.cpp
    # entity/arrow.cpp removed; arrow VFX renderer code moved to geom/arrow.cpp
    geom/selection_ring.cpp
    geom/arrow.cpp
)

target_include_directories(render_gl PUBLIC .)
target_link_libraries(render_gl PUBLIC Qt6::Core Qt6::OpenGL ${OPENGL_LIBRARIES} engine_core)