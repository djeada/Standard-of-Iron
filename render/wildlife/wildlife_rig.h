#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/creature/skeleton.h"

namespace Render::Wildlife {

enum class Bone : std::uint8_t {
  Root = 0,
  Body = 1,
  ShoulderFL = 2,
  KneeFL = 3,
  FootFL = 4,
  ShoulderFR = 5,
  KneeFR = 6,
  FootFR = 7,
  ShoulderBL = 8,
  KneeBL = 9,
  FootBL = 10,
  ShoulderBR = 11,
  KneeBR = 12,
  FootBR = 13,
  NeckTop = 14,
  Head = 15,
  Jaw = 16,
  EarL = 17,
  EarR = 18,
  TailBase = 19,
  TailTip = 20,
  Count = 21,
};

inline constexpr std::size_t k_bone_count = static_cast<std::size_t>(Bone::Count);
inline constexpr std::size_t k_leg_count = 4U;

inline constexpr int k_wildlife_material_id = 7;

using BonePalette = std::array<QMatrix4x4, k_bone_count>;

[[nodiscard]] constexpr auto
bone_index(Bone bone) noexcept -> Render::Creature::BoneIndex {
  return static_cast<Render::Creature::BoneIndex>(bone);
}

struct LegJoints {
  QVector3D shoulder{};
  QVector3D knee{};
  QVector3D foot{};
  QVector3D toe{};
};

struct RigPose {
  QVector3D root{};

  QVector3D body_rear{};
  QVector3D body_front{};

  std::array<LegJoints, k_leg_count> legs{};

  QVector3D withers{};
  QVector3D poll{};
  QVector3D muzzle{};
  QVector3D jaw_hinge{};
  QVector3D jaw_tip{};

  QVector3D ear_base_l{};
  QVector3D ear_tip_l{};
  QVector3D ear_base_r{};
  QVector3D ear_tip_r{};

  QVector3D tail_base{};
  QVector3D tail_mid{};
  QVector3D tail_tip{};
};

[[nodiscard]] auto
wildlife_topology() noexcept -> const Render::Creature::SkeletonTopology&;

void evaluate_wildlife_skeleton(const RigPose& pose, BonePalette& out) noexcept;

struct DeathMotion {

  float buckle{0.0F};

  float fall{0.0F};

  float roll{0.0F};

  float head{0.0F};

  float settle{0.0F};

  float thrash{0.0F};
};

[[nodiscard]] auto death_motion(float phase) noexcept -> DeathMotion;

[[nodiscard]] auto roll_about_spine(const QVector3D& point,
                                    const QVector3D& pivot,
                                    float radians) noexcept -> QVector3D;

} // namespace Render::Wildlife
