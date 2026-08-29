#pragma once

#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

enum class CommanderSpearMove : std::uint8_t {
  None = 0,
  Thrust,
  Sweep,
  Launcher,
  Finisher,
};

struct CommanderSpearPoseKey {
  float phase{0.0F};
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  PoseVec3 spear_dir{0.05F, 0.55F, 0.85F};
  PoseVec3 pelvis_delta{};
  PoseVec3 shoulder_r_delta{};
  PoseVec3 shoulder_l_delta{};
  PoseVec3 neck_delta{};
  PoseVec3 head_delta{};
  PoseVec3 foot_r_delta{};
  PoseVec3 knee_r_delta{};
  PoseVec3 foot_l_delta{};
  PoseVec3 knee_l_delta{};
};

[[nodiscard]] auto
resolve_commander_spear_pose(CommanderSpearMove move,
                             float phase) noexcept -> CommanderSpearPoseKey;

[[nodiscard]] auto
commander_spear_move_for_clip(std::uint16_t clip_id) noexcept -> CommanderSpearMove;

[[nodiscard]] auto
commander_spear_clip_for_move(CommanderSpearMove move) noexcept -> std::uint16_t;

} // namespace Animation
