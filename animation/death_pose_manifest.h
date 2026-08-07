#pragma once

#include <cstdint>
#include <string_view>

#include "pose_manifest.h"
#include "rig/pose_fk.h"

namespace Animation {

enum class HumanoidDeathCollapse : std::uint8_t {
  None = 0,

  BackSprawl,

  FacePlant,

  SideCrumple,

  MountedUnseat,
  Count
};

[[nodiscard]] constexpr auto humanoid_infantry_death_collapse(
    std::uint8_t variant) noexcept -> HumanoidDeathCollapse {
  switch (variant % k_humanoid_infantry_death_variant_count) {
  case 1U:
    return HumanoidDeathCollapse::FacePlant;
  case 2U:
    return HumanoidDeathCollapse::SideCrumple;
  default:
    return HumanoidDeathCollapse::BackSprawl;
  }
}

struct HumanoidDeathPoseInputs {
  HumanoidDeathCollapse collapse{HumanoidDeathCollapse::None};

  float phase{0.0F};
  float height_scale{1.0F};
  PoseFk::HumanoidRigMetrics rig{};
};

struct HumanoidDeathPoseSample {
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
  float foot_pitch_l{0.0F};
  float foot_pitch_r{0.0F};
};

[[nodiscard]] auto humanoid_death_collapse_name(HumanoidDeathCollapse collapse) noexcept
    -> std::string_view;

[[nodiscard]] constexpr auto
humanoid_death_collapse_duration(HumanoidDeathCollapse collapse) noexcept -> float {
  switch (collapse) {
  case HumanoidDeathCollapse::BackSprawl:
    return 1.15F;
  case HumanoidDeathCollapse::FacePlant:
    return 1.00F;
  case HumanoidDeathCollapse::SideCrumple:
    return 1.10F;
  case HumanoidDeathCollapse::MountedUnseat:
    return 1.25F;
  case HumanoidDeathCollapse::None:
  case HumanoidDeathCollapse::Count:
    break;
  }
  return 1.0F;
}

inline constexpr float k_humanoid_death_bake_fps = 30.0F;

[[nodiscard]] constexpr auto humanoid_death_collapse_frames(
    HumanoidDeathCollapse collapse) noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(
      (humanoid_death_collapse_duration(collapse) * k_humanoid_death_bake_fps) + 0.5F);
}

[[nodiscard]] auto resolve_humanoid_death_pose(
    const HumanoidDeathPoseInputs& inputs) noexcept -> HumanoidDeathPoseSample;

} // namespace Animation
