

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>

#include "../creature/spec.h"
#include "horse_renderer_base.h"
#include "horse_source_asset.h"

namespace Render::GL {
class ISubmitter;
}

namespace Render::Horse {

enum class HorseBone : std::uint8_t {
  SourceBody = 0,
  SourceBack = 1,
  SourceTorso = 2,
  SourceTorso2 = 3,
  SourceTorso3 = 4,
  SourceNeck1 = 5,
  SourceNeck2 = 6,
  SourceNeck3 = 7,
  SourceHead = 8,
  SourceEar1L = 9,
  SourceEar2L = 10,
  SourceEar3L = 11,
  SourceEar4L = 12,
  SourceEar1R = 13,
  SourceEar2R = 14,
  SourceEar3R = 15,
  SourceEar4R = 16,
  SourceFrontShoulderL = 17,
  SourceFrontUpperLegL = 18,
  SourceFrontLowerLegL = 19,
  SourceFrontShoulderR = 20,
  SourceFrontUpperLegR = 21,
  SourceFrontLowerLegR = 22,
  SourceBackShoulderL = 23,
  SourceBackLegL = 24,
  SourceBackUpperLegL = 25,
  SourceBackLowerLegL = 26,
  SourceBackShoulderR = 27,
  SourceBackLegR = 28,
  SourceBackUpperLegR = 29,
  SourceBackLowerLegR = 30,
  SourceTail1 = 31,
  SourceTail2 = 32,
  SourceTail3 = 33,
  SourceTail4 = 34,
  SourceTail5 = 35,
  SourceTail6 = 36,
  SourceTail7 = 37,
  SourcePoleTargetBackL = 38,
  SourcePoleTargetL = 39,
  SourcePoleTargetBackR = 40,
  SourcePoleTargetR = 41,
  SourceIkBackLegL = 42,
  SourceBackFootL = 43,
  SourceIkFrontLegL = 44,
  SourceFrontFootL = 45,
  SourceIkBackLegR = 46,
  SourceBackFootR = 47,
  SourceIkFrontLegR = 48,
  SourceFrontFootR = 49,
  Count = 50,

  Root = SourceBody,
  Body = SourceTorso2,
  ShoulderFL = SourceFrontUpperLegL,
  KneeFL = SourceFrontLowerLegL,
  FootFL = SourceFrontFootL,
  ShoulderFR = SourceFrontUpperLegR,
  KneeFR = SourceFrontLowerLegR,
  FootFR = SourceFrontFootR,
  ShoulderBL = SourceBackLegL,
  KneeBL = SourceBackLowerLegL,
  FootBL = SourceBackFootL,
  ShoulderBR = SourceBackLegR,
  KneeBR = SourceBackLowerLegR,
  FootBR = SourceBackFootR,
  NeckTop = SourceNeck3,
  Head = SourceHead,
};

inline constexpr std::size_t k_horse_bone_count = k_horse_source_bone_count;

using BonePalette = std::array<QMatrix4x4, k_horse_bone_count>;

struct HorseSpecPose {
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

  float leg_radius{0.08F};

  QVector3D body_half{};

  QVector3D neck_base{};
  QVector3D neck_top{};
  float neck_radius{0.1F};

  QVector3D head_center{};
  QVector3D head_half{};

  QVector3D shoulder_offset_pose_fl{};
  QVector3D shoulder_offset_pose_fr{};
  QVector3D shoulder_offset_pose_bl{};
  QVector3D shoulder_offset_pose_br{};

  QVector3D knee_fl{};
  QVector3D knee_fr{};
  QVector3D knee_bl{};
  QVector3D knee_br{};

  float pose_leg_radius{0.11F};

  QVector3D hoof_scale{};
};

[[nodiscard]] auto
horse_topology() noexcept -> const Render::Creature::SkeletonTopology&;

void evaluate_horse_skeleton(const HorseSpecPose& pose,
                             BonePalette& out_palette) noexcept;

void make_horse_spec_pose(const Render::GL::HorseDimensions& dims,
                          float bob,
                          HorseSpecPose& out_pose) noexcept;

struct HorsePoseMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
  bool is_fighting{false};
};
void make_horse_spec_pose_animated(const Render::GL::HorseDimensions& dims,
                                   const Render::GL::HorseGait& gait,
                                   HorsePoseMotion motion,
                                   HorseSpecPose& out_pose) noexcept;

void fill_horse_role_colors(const Render::GL::HorseVariant& variant,
                            std::array<QVector3D, 8>& out_roles) noexcept;

[[nodiscard]] auto
horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec&;

auto compute_horse_bone_palette(const HorseSpecPose& pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4>;

} // namespace Render::Horse
