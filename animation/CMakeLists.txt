add_library(
    animation_core
    STATIC
    rig/quadruped_gait.cpp
    bpat/bpat_reader.cpp
    bpat/bpat_registry.cpp
    bpat/bpat_writer.cpp
    bpat/bpat_playback.cpp
    ambient_pose_manifest.cpp
    clip_manifest.cpp
    combat_manifest.cpp
    action_manifest.cpp
    elephant_gait_manifest.cpp
    attack_pose_manifest.cpp
    guard_manifest.cpp
    hold_pose_manifest.cpp
    horse_gait_manifest.cpp
    intent_manifest.cpp
    layout_manifest.cpp
    locomotion_manifest.cpp
    micro_variation_manifest.cpp
    mounted_pose_manifest.cpp
    playback_manifest.cpp
    posture_pose_manifest.cpp
    quadruped_gait_manifest.cpp
    selection_manifest.cpp
    showcase_pose_manifest.cpp
    state_manifest.cpp
)

target_include_directories(animation_core PUBLIC ${CMAKE_SOURCE_DIR})

# The baked-pose (BPAT) readers hand back QMatrix4x4 socket transforms.
target_link_libraries(animation_core PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
