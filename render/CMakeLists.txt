# render_gl static library. One source list per layer folder, lowest first;
# the order is the one scripts/check-render-layers.py enforces.
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_profiling.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_gl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_geom.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_frame.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_cache.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_backend.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_ground.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_creature.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_equipment.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_entity.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_overlay.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources_scene.cmake)

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
    ${RENDER_PROFILING_SOURCES}
    ${RENDER_GL_SOURCES}
    ${RENDER_GEOM_SOURCES}
    ${RENDER_FRAME_SOURCES}
    ${RENDER_CACHE_SOURCES}
    ${RENDER_BACKEND_SOURCES}
    ${RENDER_GROUND_SOURCES}
    ${RENDER_CREATURE_SOURCES}
    ${RENDER_EQUIPMENT_SOURCES}
    ${RENDER_ENTITY_SOURCES}
    ${RENDER_OVERLAY_SOURCES}
    ${RENDER_SCENE_SOURCES}
)

target_include_directories(render_gl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
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

# Bake recipes: how a species' animation assets are produced from its source
# asset. Deliberately a separate library so the game executable never links the
# source-clip samplers or the bake clip tables — drawing a creature must not
# require knowing how it was built. Only the baker (and preview tools that
# regenerate assets) may link this.
add_library(
    creature_bake
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/creature/horse/horse_bake_recipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/creature/elephant/elephant_bake_recipe.cpp
)
target_include_directories(creature_bake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(creature_bake PUBLIC render_gl)

# Optimization flags for render hot paths. Distribution builds must not use
# -march=native: that would make the artifact depend on the CI runner's CPU.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(render_gl PRIVATE -fno-math-errno -fno-trapping-math)
    endif()
endif()
