#pragma once

#include <cstdint>

#include "clip_manifest.h"

namespace Animation {

struct HumanoidAmbientScheduleInputs {
  std::uint32_t seed{0U};
  float idle_duration{0.0F};
};

struct HumanoidAmbientScheduleSample {
  bool active{false};
  HumanoidAmbientIdle type{HumanoidAmbientIdle::None};
  float phase{0.0F};
};

struct HumanoidAmbientSelectionInputs {
  bool jump_active{false};
  float jump_phase{0.0F};
  bool flag_rally_active{false};
  float flag_rally_phase{0.0F};
  bool mounted{false};
  bool has_locomotion{false};
  bool attacking{false};
  bool in_hold_mode{false};
  bool guarding{false};
  bool exiting_guard{false};
  bool constructing{false};
  bool healing{false};
  bool hit_reacting{false};
  bool dying{false};
  bool dead{false};
  std::uint32_t seed{0U};
  float idle_duration{0.0F};
};

struct HumanoidAmbientPoseInputs {
  HumanoidAmbientIdle type{HumanoidAmbientIdle::None};
  float phase{0.0F};
  bool airborne{false};
};

struct HumanoidAmbientPoseSample {
  float pelvis_x_delta{0.0F};
  float pelvis_y_delta{0.0F};
  float pelvis_z_delta{0.0F};
  float shoulder_l_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_x_delta{0.0F};
  float neck_y_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_x_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
  float knee_l_y_delta{0.0F};
  float knee_l_z_delta{0.0F};
  float knee_r_y_delta{0.0F};
  float knee_r_z_delta{0.0F};
  float foot_l_x_delta{0.0F};
  float foot_l_y_delta{0.0F};
  float foot_l_z_delta{0.0F};
  float foot_r_x_delta{0.0F};
  float foot_r_y_delta{0.0F};
  float foot_r_z_delta{0.0F};
  float hand_l_x_delta{0.0F};
  float hand_l_y_delta{0.0F};
  float hand_l_z_delta{0.0F};
  float hand_r_x_delta{0.0F};
  float hand_r_y_delta{0.0F};
  float hand_r_z_delta{0.0F};
  float elbow_l_x_delta{0.0F};
  float elbow_l_y_delta{0.0F};
  float elbow_l_z_delta{0.0F};
  float elbow_r_x_delta{0.0F};
  float elbow_r_y_delta{0.0F};
  float elbow_r_z_delta{0.0F};
};

[[nodiscard]] auto
resolve_humanoid_ambient_schedule(const HumanoidAmbientScheduleInputs& inputs) noexcept
    -> HumanoidAmbientScheduleSample;

[[nodiscard]] auto
resolve_humanoid_ambient_selection(const HumanoidAmbientSelectionInputs& inputs) noexcept
    -> HumanoidAmbientScheduleSample;

[[nodiscard]] auto resolve_humanoid_ambient_pose(
    const HumanoidAmbientPoseInputs& inputs) noexcept -> HumanoidAmbientPoseSample;

} // namespace Animation
