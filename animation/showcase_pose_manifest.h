#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "pose_manifest.h"

namespace Animation {

enum class HumanoidShowcaseMove : std::uint8_t {
  None = 0,
  Jump,
  FrontFlip,
  Handstand,
  SideAerial,
  SwordFlourish,
  SpearThrow,
  RestSit,
  RestSitKnees,
  RestKneel,
  RestSitDown,
  RestSitKneesDown,
  TauntDismissive,
  TauntCynical,
  Count
};

[[nodiscard]] constexpr auto
humanoid_showcase_move_is_taunt(HumanoidShowcaseMove move) noexcept -> bool {
  return move == HumanoidShowcaseMove::TauntDismissive ||
         move == HumanoidShowcaseMove::TauntCynical;
}

[[nodiscard]] constexpr auto
humanoid_showcase_move_is_resting(HumanoidShowcaseMove move) noexcept -> bool {
  return move == HumanoidShowcaseMove::RestSit ||
         move == HumanoidShowcaseMove::RestSitKnees ||
         move == HumanoidShowcaseMove::RestKneel ||
         move == HumanoidShowcaseMove::RestSitDown ||
         move == HumanoidShowcaseMove::RestSitKneesDown;
}

inline constexpr std::size_t k_humanoid_showcase_move_count =
    static_cast<std::size_t>(HumanoidShowcaseMove::Count);

struct HumanoidShowcaseRig {
  float pelvis_y{0.975F};
  float neck_rise{0.540F};
  float head_rise{0.140F};
  float shoulder_rise{0.450F};
  float shoulder_half_width{0.252F};
  float hip_half_width{0.100F};
  float hip_drop{0.020F};
  float upper_arm_len{0.320F};
  float fore_arm_len{0.270F};
  float upper_leg_len{0.500F};
  float lower_leg_len{0.470F};
};

struct HumanoidShowcasePoseInputs {
  HumanoidShowcaseMove move{HumanoidShowcaseMove::None};
  float phase{0.0F};
  float height_scale{1.0F};
  HumanoidShowcaseRig rig{};
};

struct HumanoidShowcasePoseSample {
  bool active{false};
  PoseVec3 pelvis{};
  PoseVec3 neck_base{};
  PoseVec3 head{};
  PoseVec3 shoulder_l{};
  PoseVec3 shoulder_r{};
  PoseVec3 elbow_l{};
  PoseVec3 elbow_r{};
  PoseVec3 hand_l{};
  PoseVec3 hand_r{};
  PoseVec3 knee_l{};
  PoseVec3 knee_r{};
  PoseVec3 foot_l{};
  PoseVec3 foot_r{};
  PoseVec3 grip_axis_r{};
  bool has_grip_axis{false};
};

[[nodiscard]] auto
humanoid_showcase_move_name(HumanoidShowcaseMove move) noexcept -> std::string_view;

[[nodiscard]] auto humanoid_showcase_move_from_name(std::string_view name) noexcept
    -> HumanoidShowcaseMove;

[[nodiscard]] auto
humanoid_showcase_move_duration(HumanoidShowcaseMove move) noexcept -> float;

[[nodiscard]] auto
humanoid_showcase_move_frames(HumanoidShowcaseMove move) noexcept -> std::uint32_t;

[[nodiscard]] auto humanoid_showcase_root_travel(HumanoidShowcaseMove move,
                                                 float phase) noexcept -> PoseVec3;

[[nodiscard]] auto
humanoid_showcase_release_phase(HumanoidShowcaseMove move) noexcept -> float;

[[nodiscard]] auto resolve_humanoid_showcase_pose(
    const HumanoidShowcasePoseInputs& inputs) noexcept -> HumanoidShowcasePoseSample;

} // namespace Animation
