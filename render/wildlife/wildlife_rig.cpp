#include "wildlife_rig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace Render::Wildlife {

namespace {

using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::BoneResolution;
using Render::Creature::k_invalid_bone;

constexpr std::array<BoneDef, k_bone_count> k_bones{{
    {"Root", k_invalid_bone},
    {"Body", bone_index(Bone::Root)},
    {"ShoulderFL", bone_index(Bone::Root)},
    {"KneeFL", bone_index(Bone::ShoulderFL)},
    {"FootFL", bone_index(Bone::KneeFL)},
    {"ShoulderFR", bone_index(Bone::Root)},
    {"KneeFR", bone_index(Bone::ShoulderFR)},
    {"FootFR", bone_index(Bone::KneeFR)},
    {"ShoulderBL", bone_index(Bone::Root)},
    {"KneeBL", bone_index(Bone::ShoulderBL)},
    {"FootBL", bone_index(Bone::KneeBL)},
    {"ShoulderBR", bone_index(Bone::Root)},
    {"KneeBR", bone_index(Bone::ShoulderBR)},
    {"FootBR", bone_index(Bone::KneeBR)},
    {"NeckTop", bone_index(Bone::Body)},
    {"Head", bone_index(Bone::NeckTop)},
    {"Jaw", bone_index(Bone::Head)},
    {"EarL", bone_index(Bone::Head)},
    {"EarR", bone_index(Bone::Head)},
    {"TailBase", bone_index(Bone::Body)},
    {"TailTip", bone_index(Bone::TailBase)},
}};

constexpr std::array<Render::Creature::SocketDef, 0> k_sockets{};

auto resolve_joint(void* user, BoneIndex bone) -> BoneResolution {
  const auto& pose = *static_cast<const RigPose*>(user);
  BoneResolution out;
  out.kind = Render::Creature::BoneBasisKind::FromHeadTail;

  auto leg_of = [&pose](std::size_t index) -> const LegJoints& {
    return pose.legs[index];
  };

  switch (static_cast<Bone>(bone)) {
  case Bone::Root:
    out.kind = Render::Creature::BoneBasisKind::FromRootUp;
    out.head = pose.root;
    break;
  case Bone::Body:
    out.head = pose.body_rear;
    out.tail = pose.body_front;
    break;
  case Bone::ShoulderFL:
    out.head = leg_of(0).shoulder;
    out.tail = leg_of(0).knee;
    break;
  case Bone::KneeFL:
    out.head = leg_of(0).knee;
    out.tail = leg_of(0).foot;
    break;
  case Bone::FootFL:
    out.head = leg_of(0).foot;
    out.tail = leg_of(0).toe;
    break;
  case Bone::ShoulderFR:
    out.head = leg_of(1).shoulder;
    out.tail = leg_of(1).knee;
    break;
  case Bone::KneeFR:
    out.head = leg_of(1).knee;
    out.tail = leg_of(1).foot;
    break;
  case Bone::FootFR:
    out.head = leg_of(1).foot;
    out.tail = leg_of(1).toe;
    break;
  case Bone::ShoulderBL:
    out.head = leg_of(2).shoulder;
    out.tail = leg_of(2).knee;
    break;
  case Bone::KneeBL:
    out.head = leg_of(2).knee;
    out.tail = leg_of(2).foot;
    break;
  case Bone::FootBL:
    out.head = leg_of(2).foot;
    out.tail = leg_of(2).toe;
    break;
  case Bone::ShoulderBR:
    out.head = leg_of(3).shoulder;
    out.tail = leg_of(3).knee;
    break;
  case Bone::KneeBR:
    out.head = leg_of(3).knee;
    out.tail = leg_of(3).foot;
    break;
  case Bone::FootBR:
    out.head = leg_of(3).foot;
    out.tail = leg_of(3).toe;
    break;
  case Bone::NeckTop:
    out.head = pose.withers;
    out.tail = pose.poll;
    break;
  case Bone::Head:
    out.head = pose.poll;
    out.tail = pose.muzzle;
    break;
  case Bone::Jaw:
    out.head = pose.jaw_hinge;
    out.tail = pose.jaw_tip;
    break;
  case Bone::EarL:
    out.head = pose.ear_base_l;
    out.tail = pose.ear_tip_l;
    break;
  case Bone::EarR:
    out.head = pose.ear_base_r;
    out.tail = pose.ear_tip_r;
    break;
  case Bone::TailBase:
    out.head = pose.tail_base;
    out.tail = pose.tail_mid;
    break;
  case Bone::TailTip:
    out.head = pose.tail_mid;
    out.tail = pose.tail_tip;
    break;
  case Bone::Count:
  default:
    out.kind = Render::Creature::BoneBasisKind::FromParent;
    out.head = pose.root;
    break;
  }
  return out;
}

} // namespace

auto wildlife_topology() noexcept -> const Render::Creature::SkeletonTopology& {
  static const Render::Creature::SkeletonTopology topology{
      std::span<const BoneDef>(k_bones),
      std::span<const Render::Creature::SocketDef>(k_sockets)};
  return topology;
}

void evaluate_wildlife_skeleton(const RigPose& pose, BonePalette& out) noexcept {
  Render::Creature::evaluate_skeleton(wildlife_topology(),
                                      &resolve_joint,
                                      const_cast<RigPose*>(&pose),
                                      QVector3D(1.0F, 0.0F, 0.0F),
                                      std::span<QMatrix4x4>(out));
}

namespace {

[[nodiscard]] auto saturate(float value) noexcept -> float {
  return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] auto smoothstep01(float t) noexcept -> float {
  float const x = saturate(t);
  return x * x * (3.0F - (2.0F * x));
}

[[nodiscard]] auto ease_out(float t) noexcept -> float {
  float const inv = 1.0F - saturate(t);
  return 1.0F - (inv * inv);
}

[[nodiscard]] auto ease_in(float t) noexcept -> float {
  float const x = saturate(t);
  return x * x;
}

} // namespace

auto death_motion(float phase) noexcept -> DeathMotion {
  float const p = saturate(phase);
  constexpr float k_impact = 0.58F;

  DeathMotion motion;
  motion.buckle = ease_out(p / 0.22F);
  motion.fall = ease_in(p / k_impact);
  motion.roll = smoothstep01((p - 0.18F) / 0.62F);
  motion.head = ease_in((p - 0.10F) / 0.62F);

  if (p > k_impact) {
    float const since = (p - k_impact) / (1.0F - k_impact);
    motion.settle = std::exp(-5.0F * since) * std::sin(since * 12.0F);
  }

  motion.thrash =
      std::sin(p * 22.0F) * (1.0F - smoothstep01(p / 0.30F)) * saturate(p / 0.06F);
  return motion;
}

auto roll_about_spine(const QVector3D& point,
                      const QVector3D& pivot,
                      float radians) noexcept -> QVector3D {
  QVector3D const d = point - pivot;
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return pivot + QVector3D((d.x() * c) - (d.y() * s), (d.x() * s) + (d.y() * c), d.z());
}

} // namespace Render::Wildlife
