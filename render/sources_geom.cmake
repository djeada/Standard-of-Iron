# render/geom: procedural meshes. See scripts/check-render-layers.py.
set(RENDER_GEOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/arrow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/banner_cloth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/flag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/icon_glyph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/instance_block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/mode_indicator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/parts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/primitives.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/rigged_mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/selection_disc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/shared_geometry_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/stone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/texture_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/transforms.cpp
)
