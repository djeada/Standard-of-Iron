

#pragma once

#include "../creature/spec.h"
#include "../gl/humanoid/humanoid_types.h"
#include "elephant_renderer_base.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Elephant {

enum class ElephantBone : std::uint8_t {
  Root = 0,
  Body,
  FootFL,
  FootFR,
  FootBL,
  FootBR,
  Head,
  TrunkTip,
  Count,
};

inline constexpr std::size_t kElephantBoneCount =
    static_cast<std::size_t>(ElephantBone::Count);

using BonePalette = std::array<QMatrix4x4, kElephantBoneCount>;

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

  QVector3D reduced_body_half{};

  QVector3D neck_base_offset{};
  float neck_radius{0.1F};

  QVector3D head_center{};
  QVector3D head_half{};

  QVector3D trunk_end{};
  float trunk_base_radius{0.1F};

  QVector3D shoulder_offset_reduced_fl{};
  QVector3D shoulder_offset_reduced_fr{};
  QVector3D shoulder_offset_reduced_bl{};
  QVector3D shoulder_offset_reduced_br{};

  QVector3D foot_reduced_fl{};
  QVector3D foot_reduced_fr{};
  QVector3D foot_reduced_bl{};
  QVector3D foot_reduced_br{};

  float leg_radius_reduced{0.1F};

  float foot_pad_offset_y{0.0F};
  QVector3D foot_pad_half{};
};

[[nodiscard]] auto
elephant_topology() noexcept -> const Render::Creature::SkeletonTopology &;

void evaluate_elephant_skeleton(const ElephantSpecPose &pose,
                                BonePalette &out_palette) noexcept;

void make_elephant_spec_pose(const Render::GL::ElephantDimensions &dims,
                             float bob, ElephantSpecPose &out_pose) noexcept;

struct ElephantReducedMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
  bool is_fighting{false};

  float anim_time{0.0F};
  Render::GL::CombatAnimPhase combat_phase{Render::GL::CombatAnimPhase::Idle};
};

void make_elephant_spec_pose_reduced(const Render::GL::ElephantDimensions &dims,
                                     const Render::GL::ElephantGait &gait,
                                     const ElephantReducedMotion &motion,
                                     ElephantSpecPose &out_pose) noexcept;

inline constexpr std::size_t kElephantRoleCount = 10;

void fill_elephant_role_colors(
    const Render::GL::ElephantVariant &variant,
    std::array<QVector3D, kElephantRoleCount> &out_roles) noexcept;

[[nodiscard]] auto
elephant_creature_spec() noexcept -> const Render::Creature::CreatureSpec &;

auto compute_elephant_bone_palette(const ElephantSpecPose &pose,
                                   std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto elephant_bind_palette() noexcept -> std::span<const QMatrix4x4>;

} // namespace Render::Elephant
