# render/cache: baked static meshes and attachment specs. See scripts/check-render-layers.py.
set(RENDER_CACHE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/static_attachment_spec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/static_building_batch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/static_mesh_pack.cpp
)
