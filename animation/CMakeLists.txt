add_library(animation_core STATIC
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
    state_manifest.cpp
)

target_include_directories(animation_core PUBLIC ${CMAKE_SOURCE_DIR})
