add_library(render_gl STATIC
    gl/shader.cpp
    gl/buffer.cpp
    gl/mesh.cpp
    gl/texture.cpp
    gl/primitives.cpp
    scene_renderer.cpp
    gl/camera.cpp
    gl/resources.cpp
    gl/bootstrap.cpp
    gl/backend.cpp
    gl/shader_cache.cpp
    gl/state_scopes.cpp
    draw_queue.cpp
    ground/ground_renderer.cpp
    ground/fog_renderer.cpp
    ground/terrain_renderer.cpp
    ground/river_renderer.cpp
    ground/riverbank_renderer.cpp
    ground/riverbank_asset_renderer.cpp
    ground/bridge_renderer.cpp
    ground/biome_renderer.cpp
    ground/stone_renderer.cpp
    ground/plant_renderer.cpp
    ground/pine_renderer.cpp
    ground/firecamp_renderer.cpp
    entity/registry.cpp
    entity/archer_renderer.cpp
    entity/knight_renderer.cpp
    entity/horse_renderer.cpp
    entity/mounted_knight_renderer.cpp
    entity/spearman_renderer.cpp
    entity/barracks_renderer.cpp
    # entity/arrow.cpp removed; arrow VFX renderer code moved to geom/arrow.cpp
    geom/selection_ring.cpp
    geom/selection_disc.cpp
    geom/arrow.cpp
    geom/flag.cpp
    geom/patrol_flags.cpp
    geom/transforms.cpp
    humanoid_math.cpp
    palette.cpp
    humanoid_base.cpp
)

target_include_directories(render_gl PUBLIC .)
target_link_libraries(render_gl PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::OpenGL ${OPENGL_LIBRARIES} engine_core)