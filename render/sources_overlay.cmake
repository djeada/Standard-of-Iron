# render/overlay: projectiles and world overlays. See scripts/check-render-layers.py.
set(RENDER_OVERLAY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/arrow_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/attack_target_markers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/formation_arrow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/interaction_target_markers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/patrol_flags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/projectile_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/projectile_stone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/raft_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/range_rings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/rockfall_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/target_focus_rings.cpp
)
