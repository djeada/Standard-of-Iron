# render_gl static library.
# Source lists are split into per-area fragments so that independent refactors
# can edit disjoint files without merge conflicts:
#   sources_gl.cmake        -> render/gl/*          (GL backend layer)
#   sources_entity.cmake    -> render/entity/*      (entity & nation renderers)
#   sources_equipment.cmake -> render/equipment/*   (equipment renderers)
#   sources_creature.cmake  -> render/{humanoid,creature,horse,elephant}/*
#   sources_ground.cmake    -> render/ground/*      (terrain & scatter)
#   sources_core.cmake      -> everything else      (scene_renderer, geom, caches, ...)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_gl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_entity.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_equipment.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_creature.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_ground.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_core.cmake)

# The CPU triangle rasteriser, on its own because it has no dependency on
# anything -- not GL, not the scene, not the game. Three preview tools and the
# software backend all draw through it.
#
# It is a target rather than a source file the tools each list because
# humanoid_preview did exactly that: it compiled software_rasterizer.cpp into
# itself to avoid dragging in GL, and so reviewed poses through a second copy of
# the rasteriser that no shipped binary contains.
add_library(soi_software_raster STATIC software/software_rasterizer.cpp)
target_include_directories(soi_software_raster PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(
    soi_software_raster
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
)

add_library(
    render_gl
    STATIC
    ${RENDER_GL_SOURCES}
    ${RENDER_ENTITY_SOURCES}
    ${RENDER_EQUIPMENT_SOURCES}
    ${RENDER_CREATURE_SOURCES}
    ${RENDER_GROUND_SOURCES}
    ${RENDER_CORE_SOURCES}
)

target_include_directories(render_gl PUBLIC .)
target_link_libraries(
    render_gl
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::OpenGL
        ${OPENGL_LIBRARIES}
        engine_core
        animation_core
        scene_core
        soi_software_raster
)

# The name docs/ARCHITECTURE.md gives this layer. See the alias note in
# game/CMakeLists.txt.
add_library(soi_render_gl ALIAS render_gl)

# Optimization flags for render hot paths. Distribution builds must not use
# -march=native: that would make the artifact depend on the CI runner's CPU.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(render_gl PRIVATE -fno-math-errno -fno-trapping-math)
    endif()
endif()
