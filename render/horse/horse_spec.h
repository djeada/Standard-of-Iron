

#pragma once

#include "../creature/spec.h"
#include "horse_renderer_base.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>
#include <span>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Horse {

enum class HorseBone : std::uint8_t {
  Root = 0,
  Body,
  FootFL,
  FootFR,
  FootBL,
  FootBR,
  NeckTop,
  Head,
  Count,
};

inline constexpr std::size_t kHorseBoneCount =
    static_cast<std::size_t>(HorseBone::Count);

using BonePalette = std::array<QMatrix4x4, kHorseBoneCount>;

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

  QVector3D reduced_body_half{};

  QVector3D neck_base{};
  QVector3D neck_top{};
  float neck_radius{0.1F};

  QVector3D head_center{};
  QVector3D head_half{};

  QVector3D shoulder_offset_reduced_fl{};
  QVector3D shoulder_offset_reduced_fr{};
  QVector3D shoulder_offset_reduced_bl{};
  QVector3D shoulder_offset_reduced_br{};

  float leg_radius_reduced{0.11F};

  QVector3D hoof_scale{};
};

[[nodiscard]] auto
horse_topology() noexcept -> const Render::Creature::SkeletonTopology &;

void evaluate_horse_skeleton(const HorseSpecPose &pose,
                             BonePalette &out_palette) noexcept;

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept;

struct HorseReducedMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
};
void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept;

void fill_horse_role_colors(const Render::GL::HorseVariant &variant,
                            std::array<QVector3D, 8> &out_roles) noexcept;

[[nodiscard]] auto
horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec &;

auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4>;

} // namespace Render::Horse
