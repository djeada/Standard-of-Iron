# render/gl: GL handles and wrappers. See scripts/check-render-layers.py.
set(RENDER_GL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_lifetime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/instance_buffer_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/mesh_gpu_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/render_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/resources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/rigged_mesh_gpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/rigged_mesh_gpu_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/terrain_noise_atlas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/texture_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl/texture_upload.cpp
)
