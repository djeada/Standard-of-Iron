

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>

#include "../creature/spec.h"
#include "../gl/humanoid/humanoid_types.h"
#include "elephant_renderer_base.h"
#include "elephant_source_asset.h"

namespace Render::GL {
class ISubmitter;
}

namespace Render::Elephant {

enum class ElephantBone : std::uint8_t {
  SourceBone = 0,
  SourceBone001 = 1,
  SourceBone002 = 2,
  SourceBone003 = 3,
  SourceBone012 = 4,
  SourceBone013 = 5,
  SourceBone014 = 6,
  SourceEarL = 7,
  SourceEarR = 8,
  SourceUpperFrontL = 9,
  SourceLowerFrontL = 10,
  SourceFootFrontL = 11,
  SourceUpperFrontR = 12,
  SourceLowerFrontR = 13,
  SourceFootFrontR = 14,
  SourceUpperBackL = 15,
  SourceLowerBackL = 16,
  SourceFootBackL = 17,
  SourceUpperBackR = 18,
  SourceLowerBackR = 19,
  SourceFootBackR = 20,
  SourceTail = 21,
  SourceKneeBackL = 22,
  SourceKneeFrontL = 23,
  SourceBone016 = 24,
  SourceBone017 = 25,
  SourceBone018 = 26,
  SourceTrunkIk = 27,
  SourceKneeBackR = 28,
  SourceKneeFrontR = 29,
  SourceBone019 = 30,
  SourceBone020 = 31,
  Count = 32,

  Root = SourceBone,
  Body = SourceBone001,
  ShoulderFL = SourceUpperFrontL,
  KneeFL = SourceLowerFrontL,
  FootFL = SourceFootFrontL,
  ShoulderFR = SourceUpperFrontR,
  KneeFR = SourceLowerFrontR,
  FootFR = SourceFootFrontR,
  ShoulderBL = SourceUpperBackL,
  KneeBL = SourceLowerBackL,
  FootBL = SourceFootBackL,
  ShoulderBR = SourceUpperBackR,
  KneeBR = SourceLowerBackR,
  FootBR = SourceFootBackR,
  Head = SourceBone003,
  TrunkTip = SourceBone014,
};

inline constexpr std::size_t k_elephant_bone_count = k_elephant_source_bone_count;

using BonePalette = std::array<QMatrix4x4, k_elephant_bone_count>;

struct ElephantSpecPose {
  QVector3D barrel_center{};
  QVector3D foot_fl{};
  QVector3D foot_fr{};
  QVector3D foot_bl{};
  QVector3D foot_br{};

  float body_ellipsoid_x{1.0F};
  float body_ellipsoid_y{1.0F};
  float body_ellipsoid_z{1.0F};

  QVector3D shoulder_offset_fl{};
  QVector3D shoulder_offset_fr{};
  QVector3D shoulder_offset_bl{};
  QVector3D shoulder_offset_br{};

  float leg_radius{0.1F};

  QVector3D body_half{};

  QVector3D neck_base_offset{};
  float neck_radius{0.1F};

  QVector3D head_center{};
  QVector3D head_half{};

  QVector3D trunk_end{};
  float trunk_base_radius{0.1F};

  QVector3D shoulder_offset_pose_fl{};
  QVector3D shoulder_offset_pose_fr{};
  QVector3D shoulder_offset_pose_bl{};
  QVector3D shoulder_offset_pose_br{};

  QVector3D knee_fl{};
  QVector3D knee_fr{};
  QVector3D knee_bl{};
  QVector3D knee_br{};

  QVector3D foot_pose_fl{};
  QVector3D foot_pose_fr{};
  QVector3D foot_pose_bl{};
  QVector3D foot_pose_br{};

  float pose_leg_radius{0.1F};

  float foot_pad_offset_y{0.0F};
  QVector3D foot_pad_half{};
};

[[nodiscard]] auto
elephant_topology() noexcept -> const Render::Creature::SkeletonTopology&;

void evaluate_elephant_skeleton(const ElephantSpecPose& pose,
                                BonePalette& out_palette) noexcept;

void make_elephant_spec_pose(const Render::GL::ElephantDimensions& dims,
                             float bob,
                             ElephantSpecPose& out_pose) noexcept;

struct ElephantPoseMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
  bool is_fighting{false};

  float anim_time{0.0F};
  Render::GL::CombatAnimPhase combat_phase{Render::GL::CombatAnimPhase::Idle};
};

void make_elephant_spec_pose_animated(const Render::GL::ElephantDimensions& dims,
                                      const Render::GL::ElephantGait& gait,
                                      const ElephantPoseMotion& motion,
                                      ElephantSpecPose& out_pose) noexcept;

inline constexpr std::size_t k_elephant_role_count = 10;

void fill_elephant_role_colors(
    const Render::GL::ElephantVariant& variant,
    std::array<QVector3D, k_elephant_role_count>& out_roles) noexcept;

[[nodiscard]] auto
elephant_creature_spec() noexcept -> const Render::Creature::CreatureSpec&;

auto compute_elephant_bone_palette(const ElephantSpecPose& pose,
                                   std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto elephant_bind_palette() noexcept -> std::span<const QMatrix4x4>;

} // namespace Render::Elephant
