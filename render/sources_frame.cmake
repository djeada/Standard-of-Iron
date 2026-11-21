# render/frame: draw commands, queue, submitters, per-frame values. See scripts/check-render-layers.py.
set(RENDER_FRAME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/bone_palette_arena.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/camera_visibility.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/draw_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/effects_submitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/entity_appearance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/palette.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/pass/frame_pass_runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/pipeline/instance_coalescer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/primitive_batch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/render_archetype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame/world_view.cpp
)
