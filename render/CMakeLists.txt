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
    gl/backend/cylinder_pipeline.cpp
    gl/backend/vegetation_pipeline.cpp
    gl/backend/terrain_pipeline.cpp
    gl/backend/character_pipeline.cpp
    gl/backend/water_pipeline.cpp
    gl/backend/effects_pipeline.cpp
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
    entity/nations/kingdom/archer_renderer.cpp
    entity/nations/roman/archer_renderer.cpp
    entity/nations/carthage/archer_renderer.cpp
    entity/nations/kingdom/archer_style.cpp
    entity/nations/roman/archer_style.cpp
    entity/nations/carthage/archer_style.cpp
    entity/nations/kingdom/spearman_style.cpp
    entity/nations/roman/swordsman_renderer.cpp
    entity/nations/kingdom/swordsman_renderer.cpp
    entity/nations/carthage/swordsman_renderer.cpp
    entity/nations/roman/swordsman_style.cpp
    entity/nations/carthage/swordsman_style.cpp
    entity/nations/kingdom/swordsman_style.cpp
    entity/nations/roman/spearman_renderer.cpp
    entity/nations/kingdom/spearman_renderer.cpp
    entity/nations/carthage/spearman_renderer.cpp
    entity/nations/roman/spearman_style.cpp
    entity/nations/carthage/spearman_style.cpp
    horse/rig.cpp
    horse/horse_animation_controller.cpp
    entity/horse_renderer.cpp
    entity/nations/roman/horse_swordsman_renderer.cpp
    entity/nations/kingdom/horse_swordsman_renderer.cpp
    entity/nations/carthage/horse_swordsman_renderer.cpp
    entity/barracks_renderer.cpp
    entity/nations/kingdom/barracks_renderer.cpp
    entity/nations/roman/barracks_renderer.cpp
    entity/nations/carthage/barracks_renderer.cpp
    # entity/arrow.cpp removed; arrow VFX renderer code moved to geom/arrow.cpp
    geom/selection_ring.cpp
    geom/selection_disc.cpp
    geom/arrow.cpp
    geom/flag.cpp
    geom/patrol_flags.cpp
    geom/transforms.cpp
    humanoid/humanoid_math.cpp
    palette.cpp
    humanoid/rig.cpp
    humanoid/style_palette.cpp
    humanoid/pose_controller.cpp
    humanoid/mounted_pose_controller.cpp
    equipment/equipment_registry.cpp
    equipment/register_equipment.cpp
    equipment/armor/tunic_renderer.cpp
    equipment/armor/kingdom_armor.cpp
    equipment/armor/roman_armor.cpp
    equipment/armor/armor_light_carthage.cpp
    equipment/armor/armor_heavy_carthage.cpp
    equipment/weapons/bow_renderer.cpp
    equipment/weapons/quiver_renderer.cpp
    equipment/weapons/roman_scutum.cpp
    equipment/weapons/sword_renderer.cpp
    equipment/weapons/sword_carthage.cpp
    equipment/weapons/sword_roman.cpp
    equipment/weapons/sword_kingdom.cpp
    equipment/weapons/spear_renderer.cpp
    equipment/weapons/shield_renderer.cpp
    equipment/weapons/shield_carthage.cpp
    equipment/weapons/shield_roman.cpp
    equipment/weapons/shield_kingdom.cpp
    equipment/helmets/headwrap.cpp
    equipment/helmets/roman_heavy_helmet.cpp
    equipment/helmets/roman_light_helmet.cpp
    equipment/helmets/kingdom_heavy_helmet.cpp
    equipment/helmets/kingdom_light_helmet.cpp
    equipment/helmets/carthage_heavy_helmet.cpp
    equipment/helmets/carthage_light_helmet.cpp
    equipment/armor/chainmail_armor.cpp
    equipment/horse/saddles/roman_saddle_renderer.cpp
    equipment/horse/saddles/carthage_saddle_renderer.cpp
    equipment/horse/saddles/light_cavalry_saddle_renderer.cpp
    equipment/horse/tack/bridle_renderer.cpp
    equipment/horse/tack/stirrup_renderer.cpp
    equipment/horse/tack/blanket_renderer.cpp
    equipment/horse/tack/reins_renderer.cpp
    equipment/horse/armor/scale_barding_renderer.cpp
    equipment/horse/armor/leather_barding_renderer.cpp
    equipment/horse/armor/champion_renderer.cpp
    equipment/horse/armor/crupper_renderer.cpp
    equipment/horse/decorations/plume_renderer.cpp
    equipment/horse/decorations/tail_ribbon_renderer.cpp
    equipment/horse/decorations/saddle_bag_renderer.cpp
)

target_include_directories(render_gl PUBLIC .)
target_link_libraries(render_gl PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::OpenGL ${OPENGL_LIBRARIES} engine_core)

# Optimization flags for render hot paths
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(render_gl PRIVATE
            -march=native          # Use CPU-specific instructions
            -mtune=native         # Optimize for current CPU
            -fno-math-errno       # Faster math
            -fno-trapping-math
        )
    endif()
endif()
