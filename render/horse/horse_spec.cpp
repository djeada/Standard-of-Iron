#include "horse_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

namespace Render::Horse {

namespace {

using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::CreatureLOD;
using Render::Creature::kLodMinimal;
using Render::Creature::kLodReduced;
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::Creature::PrimitiveParams;
using Render::Creature::PrimitiveShape;
using Render::Creature::SkeletonTopology;

constexpr std::array<BoneDef, kHorseBoneCount> kHorseBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", 0},
    {"FootFL", 0},
    {"FootFR", 0},
    {"FootBL", 0},
    {"FootBR", 0},
    {"NeckTop", 0},
    {"Head", 0},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kHorseSockets{};

constexpr SkeletonTopology kHorseTopology{
    std::span<const BoneDef>(kHorseBones),
    std::span<const Render::Creature::SocketDef>(kHorseSockets),
};

constexpr std::uint8_t kRoleCoat = 1;
constexpr std::uint8_t kRoleCoatDark = 2;
constexpr std::uint8_t kRoleCoatReducedLeg = 3;
constexpr std::uint8_t kRoleHoof = 4;
constexpr std::uint8_t kRoleMane = 5;
constexpr std::uint8_t kRoleTail = 6;
constexpr std::uint8_t kRoleMuzzle = 7;
constexpr std::uint8_t kRoleEye = 8;
constexpr std::size_t kHorseRoleCount = 8;

[[nodiscard]] auto
translation_matrix(const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setToIdentity();
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

} // namespace

auto horse_topology() noexcept -> const SkeletonTopology & {
  return kHorseTopology;
}

void evaluate_horse_skeleton(const HorseSpecPose &pose,
                             BonePalette &out_palette) noexcept {
  out_palette[static_cast<std::size_t>(HorseBone::Root)] =
      translation_matrix(pose.barrel_center);

  QMatrix4x4 body;
  body.setColumn(0, QVector4D(pose.body_ellipsoid_x, 0.0F, 0.0F, 0.0F));
  body.setColumn(1, QVector4D(0.0F, pose.body_ellipsoid_y, 0.0F, 0.0F));
  body.setColumn(2, QVector4D(0.0F, 0.0F, pose.body_ellipsoid_z, 0.0F));
  body.setColumn(3, QVector4D(pose.barrel_center, 1.0F));
  out_palette[static_cast<std::size_t>(HorseBone::Body)] = body;

  out_palette[static_cast<std::size_t>(HorseBone::FootFL)] =
      translation_matrix(pose.foot_fl);
  out_palette[static_cast<std::size_t>(HorseBone::FootFR)] =
      translation_matrix(pose.foot_fr);
  out_palette[static_cast<std::size_t>(HorseBone::FootBL)] =
      translation_matrix(pose.foot_bl);
  out_palette[static_cast<std::size_t>(HorseBone::FootBR)] =
      translation_matrix(pose.foot_br);

  out_palette[static_cast<std::size_t>(HorseBone::NeckTop)] =
      translation_matrix(pose.neck_top);
  out_palette[static_cast<std::size_t>(HorseBone::Head)] =
      translation_matrix(pose.head_center);
}

void fill_horse_role_colors(const Render::GL::HorseVariant &variant,
                            std::array<QVector3D, 8> &out_roles) noexcept {
  QVector3D const coat = variant.coat_color;
  out_roles[0] = coat;
  out_roles[1] = coat * 0.75F;
  out_roles[2] = coat * 0.85F;
  out_roles[3] = variant.hoof_color;
  out_roles[4] = variant.mane_color;
  out_roles[5] = variant.tail_color;
  out_roles[6] = variant.muzzle_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
}

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.90F;
  out_pose.body_ellipsoid_y = dims.body_height * 0.52F + dims.neck_rise * 0.18F;
  out_pose.body_ellipsoid_z = dims.body_length + dims.head_length * 0.68F;

  float const shoulder_dx = dims.body_width * 0.58F;
  float const shoulder_dy = -dims.body_height * 0.18F;
  float const front_dz = dims.body_length * 0.28F;
  float const rear_dz = -dims.body_length * 0.30F;

  out_pose.shoulder_offset_fl = QVector3D(shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr = QVector3D(-shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(shoulder_dx, shoulder_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-shoulder_dx, shoulder_dy, rear_dz);

  float const drop = -dims.leg_length * 0.84F;
  out_pose.foot_fl =
      center + out_pose.shoulder_offset_fl + QVector3D(0, drop, 0);
  out_pose.foot_fr =
      center + out_pose.shoulder_offset_fr + QVector3D(0, drop, 0);
  out_pose.foot_bl =
      center + out_pose.shoulder_offset_bl + QVector3D(0, drop, 0);
  out_pose.foot_br =
      center + out_pose.shoulder_offset_br + QVector3D(0, drop, 0);

  out_pose.leg_radius = dims.body_width * 0.11F;
}

namespace {

struct ReducedLegSample {
  QVector3D shoulder;
  QVector3D foot;
};

auto compute_reduced_leg(
    const Render::GL::HorseDimensions &dims, const Render::GL::HorseGait &gait,
    float phase_base, float phase_offset, float lateral_sign,
    float forward_bias, bool is_moving,
    const QVector3D &anchor_offset) noexcept -> ReducedLegSample {
  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  float const leg_phase = std::fmod(phase_base + phase_offset, 1.0F);
  float stride = 0.0F;
  float lift = 0.0F;
  float shoulder_compress = 0.0F;
  if (is_moving) {
    float const stance_fraction = gait.cycle_time > 0.9F
                                      ? 0.52F
                                      : (gait.cycle_time > 0.5F ? 0.38F : 0.28F);
    float const stride_extent = gait.stride_swing * 0.62F;
    if (leg_phase < stance_fraction) {
      float const t = ease_in_out(leg_phase / stance_fraction);
      stride = forward_bias + stride_extent * (0.45F - 1.10F * t);
      shoulder_compress =
          std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.10F;
    } else {
      float const t = ease_in_out((leg_phase - stance_fraction) /
                                  (1.0F - stance_fraction));
      stride = forward_bias - stride_extent * 0.65F + stride_extent * 1.10F * t;
      lift = std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.92F;
    }
  }

  float const shoulder_out = dims.body_width * 0.82F;
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out, lift * 0.05F - shoulder_compress,
                stride);

  float const leg_length = dims.leg_length * 0.95F;
  QVector3D const foot = shoulder + QVector3D(0.0F, -leg_length + lift, 0.0F);

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept {

  make_horse_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  out_pose.reduced_body_half =
      QVector3D(dims.body_width * 0.90F, dims.body_height * 0.42F,
                dims.body_length * 0.56F);

  float const head_height_scale = 1.0F + gait.head_height_jitter;
  out_pose.neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.42F * head_height_scale,
                         dims.body_length * 0.38F);
  out_pose.neck_top =
      out_pose.neck_base +
      QVector3D(0.0F, dims.neck_rise * head_height_scale,
                dims.neck_length * 1.10F);
  out_pose.neck_radius = dims.body_width * 0.48F;

  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(0.0F, dims.head_height * 0.18F, dims.head_length * 0.56F);
  out_pose.head_half =
      QVector3D(dims.head_width * 0.46F, dims.head_height * 0.40F,
                dims.head_length * 0.44F);

  QVector3D const front_anchor =
      QVector3D(0.0F, dims.body_height * 0.12F, dims.body_length * 0.34F);
  QVector3D const rear_anchor =
      QVector3D(0.0F, dims.body_height * 0.10F, -dims.body_length * 0.30F);

  float const front_bias = dims.body_length * 0.13F;
  float const rear_bias = -dims.body_length * 0.13F;

  Render::GL::HorseGait jittered = gait;
  jittered.stride_swing *= (1.0F + gait.stride_jitter);
  float const front_lat = gait.lateral_lead_front == 0.0F ? 0.48F : gait.lateral_lead_front;
  float const rear_lat = gait.lateral_lead_rear == 0.0F ? 0.52F : gait.lateral_lead_rear;

  auto fl =
      compute_reduced_leg(dims, jittered, motion.phase, gait.front_leg_phase, 1.0F,
                          front_bias, motion.is_moving, front_anchor);
  auto fr = compute_reduced_leg(dims, jittered, motion.phase,
                                gait.front_leg_phase + front_lat, -1.0F, front_bias,
                                motion.is_moving, front_anchor);
  auto bl = compute_reduced_leg(dims, jittered, motion.phase, gait.rear_leg_phase,
                                1.0F, rear_bias, motion.is_moving, rear_anchor);
  auto br =
      compute_reduced_leg(dims, jittered, motion.phase, gait.rear_leg_phase + rear_lat,
                          -1.0F, rear_bias, motion.is_moving, rear_anchor);

  out_pose.shoulder_offset_reduced_fl = fl.shoulder;
  out_pose.shoulder_offset_reduced_fr = fr.shoulder;
  out_pose.shoulder_offset_reduced_bl = bl.shoulder;
  out_pose.shoulder_offset_reduced_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl.foot;
  out_pose.foot_br = center + br.foot;

  out_pose.leg_radius_reduced = dims.body_width * 0.28F;

  out_pose.hoof_scale = QVector3D(dims.body_width * 0.42F, dims.hoof_height,
                                  dims.body_width * 0.45F);
}

namespace {

auto build_minimal_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 5> &out) noexcept
    -> std::size_t {
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.body";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = Render::GL::get_unit_sphere();
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Body);
    p.params.half_extents = QVector3D(1.0F, 1.12F, 1.0F);
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }

  struct LegSpec {
    std::string_view name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<LegSpec, 4> const legs{{
      {"horse.leg.fl", HorseBone::FootFL, pose.shoulder_offset_fl},
      {"horse.leg.fr", HorseBone::FootFR, pose.shoulder_offset_fr},
      {"horse.leg.bl", HorseBone::FootBL, pose.shoulder_offset_bl},
      {"horse.leg.br", HorseBone::FootBR, pose.shoulder_offset_br},
  }};

  for (std::size_t i = 0; i < 4; ++i) {
    PrimitiveInstance &p = out[1 + i];
    p.debug_name = legs[i].name;
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset = legs[i].shoulder_offset;
    p.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    p.params.tail_offset = QVector3D();
    p.params.radius = pose.leg_radius * 1.08F;
    p.color_role = kRoleCoatDark;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 5;
}

auto build_reduced_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 9> &out) noexcept
    -> std::size_t {

  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.body.reduced";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.half_extents = pose.reduced_body_half;
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[1];
    p.debug_name = "horse.chest.reduced";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, pose.reduced_body_half.y() * 0.18F,
                  pose.reduced_body_half.z() * 0.62F);
    p.params.half_extents =
        QVector3D(pose.reduced_body_half.x() * 0.84F,
                  pose.reduced_body_half.y() * 0.90F,
                  pose.reduced_body_half.z() * 0.42F);
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[2];
    p.debug_name = "horse.rump.reduced";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, pose.reduced_body_half.y() * 0.10F,
                  -pose.reduced_body_half.z() * 0.64F);
    p.params.half_extents =
        QVector3D(pose.reduced_body_half.x() * 0.88F,
                  pose.reduced_body_half.y() * 0.78F,
                  pose.reduced_body_half.z() * 0.32F);
    p.color_role = kRoleCoatDark;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[3];
    p.debug_name = "horse.neck_head.reduced";
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset = pose.neck_base - pose.barrel_center;
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Head);
    p.params.tail_offset =
        QVector3D(0.0F, pose.head_half.y() * 0.02F, pose.head_half.z() * 0.58F);
    p.params.radius = pose.neck_radius * 0.78F;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  struct RLeg {
    std::string_view leg_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<RLeg, 4> const legs{{
      {"horse.leg.fl.r", HorseBone::FootFL, pose.shoulder_offset_reduced_fl},
      {"horse.leg.fr.r", HorseBone::FootFR, pose.shoulder_offset_reduced_fr},
      {"horse.leg.bl.r", HorseBone::FootBL, pose.shoulder_offset_reduced_bl},
      {"horse.leg.br.r", HorseBone::FootBR, pose.shoulder_offset_reduced_br},
  }};
  for (std::size_t i = 0; i < 4; ++i) {

    PrimitiveInstance &leg = out[4 + i];
    leg.debug_name = legs[i].leg_name;
    leg.shape = PrimitiveShape::Cylinder;
    leg.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    leg.params.head_offset = legs[i].shoulder_offset;
    leg.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    leg.params.tail_offset = QVector3D();
    leg.params.radius = pose.leg_radius_reduced;
    leg.color_role = kRoleCoatReducedLeg;
    leg.material_id = 0;
    leg.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[8];
    p.debug_name = "horse.tail.reduced";
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, pose.reduced_body_half.y() * 0.28F,
                  -pose.reduced_body_half.z() * 0.94F);
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.tail_offset =
        QVector3D(0.0F, -pose.reduced_body_half.y() * 0.52F,
                  -pose.reduced_body_half.z() * 0.34F);
    p.params.radius = pose.leg_radius_reduced * 0.38F;
    p.color_role = kRoleTail;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  return 9;
}

} // namespace

} // namespace Render::Horse

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Horse {

namespace {

auto build_baseline_pose() noexcept -> HorseSpecPose {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  Render::GL::HorseGait gait{};
  HorseSpecPose pose{};
  make_horse_spec_pose_reduced(dims, gait, HorseReducedMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const HorseSpecPose & {
  static const HorseSpecPose pose = build_baseline_pose();
  return pose;
}

auto build_static_reduced_parts() noexcept
    -> std::array<PrimitiveInstance, 9> {
  std::array<PrimitiveInstance, 9> out{};
  build_reduced_primitives(baseline_pose(), out);
  return out;
}

auto static_reduced_parts() noexcept
    -> const std::array<PrimitiveInstance, 9> & {
  static const auto parts = build_static_reduced_parts();
  return parts;
}

auto build_static_minimal_parts() noexcept -> std::array<PrimitiveInstance, 5> {
  std::array<PrimitiveInstance, 5> out{};
  build_minimal_primitives(baseline_pose(), out);
  return out;
}

auto static_minimal_parts() noexcept
    -> const std::array<PrimitiveInstance, 5> & {
  static const auto parts = build_static_minimal_parts();
  return parts;
}

constexpr std::size_t kHorseFullPartCount = 43;

auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, kHorseFullPartCount> {
  HorseSpecPose const &pose = baseline_pose();
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  std::array<PrimitiveInstance, kHorseFullPartCount> out{};

  using Render::Creature::kLodFull;

  auto root = static_cast<BoneIndex>(HorseBone::Root);
  auto head_bone = static_cast<BoneIndex>(HorseBone::Head);
  auto neck_top_bone = static_cast<BoneIndex>(HorseBone::NeckTop);

  auto ell = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role = kRoleCoat, int material_id = 6) {
    p.debug_name = name;
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.half_extents = half_extents;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto sph = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &offset, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Sphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cyl = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &head_off, BoneIndex tail,
                 const QVector3D &tail_off, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cap = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &head_off, BoneIndex tail,
                 const QVector3D &tail_off, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cone_p = [&](PrimitiveInstance &p, std::string_view name,
                    BoneIndex anchor, const QVector3D &head_off, BoneIndex tail,
                    const QVector3D &tail_off, float radius, std::uint8_t role,
                    int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Cone;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  float const bw = dims.body_width;
  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const hl = dims.head_length;

  std::size_t i = 0;

  // ---- Body / barrel (10 primitives) ----------------------------------
  // Visual width/height override so the barrel reads wider + flatter from
  // the RTS camera angle (original bw/bh are structural, used elsewhere).
  float const bw_v = bw * 1.40F;
  float const bh_v = bh * 0.88F;

  // Ribcage: wide flat oval, elongated along Z.
  ell(out[i++], "horse.full.ribcage", root,
      QVector3D(bw_v * 0.60F, bh_v * 0.60F, bl * 0.52F),
      QVector3D(0.0F, bh_v * 0.08F, bl * 0.10F));
  // Withers hump (highest point of the back, just behind shoulders).
  ell(out[i++], "horse.full.withers", root,
      QVector3D(bw_v * 0.34F, bh_v * 0.30F, bl * 0.16F),
      QVector3D(0.0F, bh_v * 0.68F, bl * 0.22F));
  // Belly (rounded under the barrel, deeper and longer for a barrel-chest).
  ell(out[i++], "horse.full.belly", root,
      QVector3D(bw_v * 0.56F, bh_v * 0.40F, bl * 0.46F),
      QVector3D(0.0F, -bh_v * 0.30F, 0.0F));
  // Chest/prow (forward-facing mass between the front legs).
  ell(out[i++], "horse.full.chest", root,
      QVector3D(bw_v * 0.50F, bh_v * 0.46F, bl * 0.30F),
      QVector3D(0.0F, bh_v * 0.12F, bl * 0.38F));
  // Sternum (under-chest prow filler).
  ell(out[i++], "horse.full.sternum", root,
      QVector3D(bw_v * 0.36F, bh_v * 0.22F, bl * 0.20F),
      QVector3D(0.0F, -bh_v * 0.28F, bl * 0.34F));
  // Croup (top of rump, slightly lower than withers).
  ell(out[i++], "horse.full.croup", root,
      QVector3D(bw_v * 0.44F, bh_v * 0.32F, bl * 0.22F),
      QVector3D(0.0F, bh_v * 0.44F, -bl * 0.28F));
  // Hindquarter L/R (the powerful muscle that defines a horse's rear).
  ell(out[i++], "horse.full.hq.l", root,
      QVector3D(bw_v * 0.42F, bh_v * 0.60F, bl * 0.40F),
      QVector3D(bw_v * 0.30F, bh_v * 0.14F, -bl * 0.30F));
  ell(out[i++], "horse.full.hq.r", root,
      QVector3D(bw_v * 0.42F, bh_v * 0.60F, bl * 0.40F),
      QVector3D(-bw_v * 0.30F, bh_v * 0.14F, -bl * 0.30F));
  // Shoulder mass L/R (in front of withers, sloping into chest).
  ell(out[i++], "horse.full.shoulder.l", root,
      QVector3D(bw_v * 0.32F, bh_v * 0.44F, bl * 0.26F),
      QVector3D(bw_v * 0.28F, bh_v * 0.18F, bl * 0.28F));
  ell(out[i++], "horse.full.shoulder.r", root,
      QVector3D(bw_v * 0.32F, bh_v * 0.44F, bl * 0.26F),
      QVector3D(-bw_v * 0.28F, bh_v * 0.18F, bl * 0.28F));

  // ---- Neck + mane (8 primitives) ------------------------------------
  // Neck crest: thick at base, blends into withers; capsule for soft taper.
  cap(out[i++], "horse.full.neck.crest", root,
      pose.neck_base - pose.barrel_center, neck_top_bone,
      QVector3D(0.0F, 0.0F, 0.0F), pose.neck_radius * 1.72F, kRoleCoat);
  // Throat curve under the neck — bulked up so the neck base reads
  // noticeably thicker than the head end (trapezoidal silhouette).
  ell(out[i++], "horse.full.neck.throat", root,
      QVector3D(bw_v * 0.32F, bh_v * 0.26F, bl * 0.18F),
      (pose.neck_base - pose.barrel_center) +
          QVector3D(0.0F, -bh_v * 0.02F, bl * 0.10F));
  // Mane: row of small flattened ellipsoids along the crest, base→poll.
  for (int m = 0; m < 5; ++m) {
    float const t = static_cast<float>(m) / 4.0F;
    QVector3D const along =
        (pose.neck_base - pose.barrel_center) * (1.0F - t) +
        (pose.neck_top - pose.barrel_center) * t;
    ell(out[i++],
        m == 0   ? "horse.full.mane.0"
        : m == 1 ? "horse.full.mane.1"
        : m == 2 ? "horse.full.mane.2"
        : m == 3 ? "horse.full.mane.3"
                 : "horse.full.mane.4",
        root,
        QVector3D(bw_v * 0.08F, bh_v * 0.10F, bl * 0.05F),
        along + QVector3D(0.0F, bh_v * 0.18F, 0.0F),
        kRoleMane, 0);
  }
  // Forelock: tuft on the forehead between the ears.
  ell(out[i++], "horse.full.forelock", head_bone,
      QVector3D(dims.head_width * 0.20F, dims.head_height * 0.10F,
                hl * 0.10F),
      QVector3D(0.0F, dims.head_height * 0.32F, -hl * 0.12F), kRoleMane, 0);

  // ---- Head (9 primitives) -------------------------------------------
  // Cranium (oblong front-to-back).
  ell(out[i++], "horse.full.head.cranium", head_bone,
      QVector3D(dims.head_width * 0.44F, dims.head_height * 0.44F,
                hl * 0.44F),
      QVector3D(0.0F, dims.head_height * 0.05F, -hl * 0.10F));
  // Muzzle (tapered front of the head — narrower than the cheek/jaw).
  cap(out[i++], "horse.full.head.muzzle", head_bone,
      QVector3D(0.0F, -dims.head_height * 0.10F, hl * 0.06F), head_bone,
      QVector3D(0.0F, -dims.head_height * 0.18F, hl * 0.48F),
      dims.head_width * 0.20F, kRoleMuzzle);
  // Lower jaw (heavier mass under the cheek — defines the wedge shape).
  ell(out[i++], "horse.full.head.jaw", head_bone,
      QVector3D(dims.head_width * 0.28F, dims.head_height * 0.17F,
                hl * 0.26F),
      QVector3D(0.0F, -dims.head_height * 0.24F, hl * 0.10F), kRoleMuzzle);
  // Cheeks L/R — fuller so the head reads wedge-shaped (wide cheek, narrow muzzle).
  sph(out[i++], "horse.full.head.cheek.l", head_bone,
      QVector3D(dims.head_width * 0.32F, -dims.head_height * 0.05F,
                -hl * 0.02F),
      dims.head_width * 0.26F, kRoleCoat);
  sph(out[i++], "horse.full.head.cheek.r", head_bone,
      QVector3D(-dims.head_width * 0.32F, -dims.head_height * 0.05F,
                -hl * 0.02F),
      dims.head_width * 0.26F, kRoleCoat);
  // Ears L/R: small upward cones.
  cone_p(out[i++], "horse.full.head.ear.l", head_bone,
         QVector3D(dims.head_width * 0.22F, dims.head_height * 0.30F,
                   -hl * 0.18F),
         head_bone,
         QVector3D(dims.head_width * 0.24F, dims.head_height * 0.62F,
                   -hl * 0.22F),
         dims.head_width * 0.10F, kRoleCoat);
  cone_p(out[i++], "horse.full.head.ear.r", head_bone,
         QVector3D(-dims.head_width * 0.22F, dims.head_height * 0.30F,
                   -hl * 0.18F),
         head_bone,
         QVector3D(-dims.head_width * 0.24F, dims.head_height * 0.62F,
                   -hl * 0.22F),
         dims.head_width * 0.10F, kRoleCoat);
  // Eyes L/R: tiny dark spheres.
  sph(out[i++], "horse.full.head.eye.l", head_bone,
      QVector3D(dims.head_width * 0.36F, dims.head_height * 0.18F,
                hl * 0.02F),
      dims.head_width * 0.07F, kRoleEye, 0);
  sph(out[i++], "horse.full.head.eye.r", head_bone,
      QVector3D(-dims.head_width * 0.36F, dims.head_height * 0.18F,
                hl * 0.02F),
      dims.head_width * 0.07F, kRoleEye, 0);

  // ---- Tail (4 primitives) -------------------------------------------
  // Tail dock anchored on the rump.
  QVector3D const tail_root_off(0.0F, bh_v * 0.32F, -bl * 0.50F);
  ell(out[i++], "horse.full.tail.dock", root,
      QVector3D(bw_v * 0.10F, bh_v * 0.10F, bl * 0.09F), tail_root_off,
      kRoleCoat);
  // Tail switch: 3 drooping flattened ellipsoids cascading down.
  for (int t_i = 0; t_i < 3; ++t_i) {
    float const tt = static_cast<float>(t_i + 1) / 3.0F;
    QVector3D const off = tail_root_off +
                          QVector3D(0.0F, -bh_v * 0.18F * tt - bh_v * 0.05F,
                                    -bl * 0.10F * tt - bl * 0.04F);
    ell(out[i++],
        t_i == 0   ? "horse.full.tail.switch.0"
        : t_i == 1 ? "horse.full.tail.switch.1"
                   : "horse.full.tail.switch.2",
        root,
        QVector3D(bw_v * 0.13F - bw_v * 0.02F * tt,
                  bh_v * 0.20F + bh_v * 0.02F * tt,
                  bl * 0.07F),
        off, kRoleTail, 0);
  }

  // ---- Legs (4 × 3 = 12 primitives: thigh, cannon+joint, hoof) -------
  // For each leg we add: (a) a thicker upper limb capsule from the
  // shoulder/hindquarter down to a knee-height point on the foot bone,
  // (b) a narrower cannon cylinder co-located on the foot bone, and
  // (c) the hoof. Joints (knee/fetlock) are implied by the radius
  // change between the two segments.
  struct LegSpec {
    std::string_view upper_name;
    std::string_view cannon_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
    bool is_rear;
  };
  std::array<LegSpec, 4> const legs{{
      {"horse.full.leg.fl.upper", "horse.full.leg.fl.cannon",
       "horse.full.hoof.fl", HorseBone::FootFL, pose.shoulder_offset_reduced_fl,
       false},
      {"horse.full.leg.fr.upper", "horse.full.leg.fr.cannon",
       "horse.full.hoof.fr", HorseBone::FootFR, pose.shoulder_offset_reduced_fr,
       false},
      {"horse.full.leg.bl.upper", "horse.full.leg.bl.cannon",
       "horse.full.hoof.bl", HorseBone::FootBL, pose.shoulder_offset_reduced_bl,
       true},
      {"horse.full.leg.br.upper", "horse.full.leg.br.cannon",
       "horse.full.hoof.br", HorseBone::FootBR, pose.shoulder_offset_reduced_br,
       true},
  }};
  float const leg_len = dims.leg_length;
  // Stronger taper from upper limb (gaskin/forearm) down to cannon bone,
  // and the hoof is wider still — leg silhouette tapers visibly top→bottom.
  float const upper_r = pose.leg_radius_reduced * 2.35F;
  float const cannon_r = pose.leg_radius_reduced * 0.92F;
  for (auto const &leg : legs) {
    auto foot_b = static_cast<BoneIndex>(leg.foot_bone);
    // Upper leg: from shoulder/hindquarter down to ~mid-leg height,
    // tapered via a Capsule (rounded ends look like muscle).
    cap(out[i++], leg.upper_name, root, leg.shoulder_offset, foot_b,
        QVector3D(0.0F, leg_len * 0.42F, 0.0F), upper_r, kRoleCoat);
    // Cannon bone + pastern: thinner cylinder rigid to the foot bone,
    // from knee height down to just above the hoof.
    cyl(out[i++], leg.cannon_name, foot_b,
        QVector3D(0.0F, leg_len * 0.42F, 0.0F), foot_b,
        QVector3D(0.0F, dims.hoof_height * 0.5F, 0.0F), cannon_r,
        kRoleCoatDark);
    // Hoof (existing horizontal cylinder mesh).
    PrimitiveInstance &hoof = out[i++];
    hoof.debug_name = leg.hoof_name;
    hoof.shape = PrimitiveShape::Mesh;
    hoof.custom_mesh = Render::GL::get_unit_cylinder();
    hoof.params.anchor_bone = foot_b;
    hoof.params.half_extents = pose.hoof_scale;
    hoof.color_role = kRoleHoof;
    hoof.material_id = 8;
    hoof.lod_mask = kLodFull;
  }

  // Sanity: every slot must be filled.
  (void)i;

  return out;
}

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseFullPartCount> & {
  static const auto parts = build_static_full_parts();
  return parts;
}

auto build_horse_bind_palette() noexcept
    -> std::array<QMatrix4x4, kHorseBoneCount> {
  std::array<QMatrix4x4, kHorseBoneCount> out{};
  BonePalette tmp{};
  evaluate_horse_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out[i] = tmp[i];
  }
  return out;
}

auto resolve_renderer(Render::GL::ISubmitter &out) noexcept
    -> Render::GL::Renderer * {
  if (auto *renderer = dynamic_cast<Render::GL::Renderer *>(&out)) {
    return renderer;
  }
  if (auto *batch = dynamic_cast<Render::GL::BatchingSubmitter *>(&out)) {
    return dynamic_cast<Render::GL::Renderer *>(batch->fallback_submitter());
  }
  return nullptr;
}

void submit_horse_rigged_impl(const HorseSpecPose &pose,
                              const Render::GL::HorseVariant &variant,
                              Render::Creature::CreatureLOD lod,
                              const QMatrix4x4 &world_from_unit,
                              Render::GL::ISubmitter &out) noexcept {

  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);

  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    if (lod == Render::Creature::CreatureLOD::Billboard) {
      return;
    }
    std::array<QVector3D, 8> role_colors{};
    fill_horse_role_colors(variant, role_colors);
    Render::Creature::submit_creature(
        horse_creature_spec(),
        std::span<const QMatrix4x4>(tmp.data(), tmp.size()), lod,
        world_from_unit, out,
        std::span<const QVector3D>(role_colors.data(), role_colors.size()));
    return;
  }

  auto const &spec = horse_creature_spec();
  auto bind = horse_bind_palette();

  auto *entry = renderer->rigged_mesh_cache().get_or_bake(spec, lod, bind, 0);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  auto &arena = renderer->bone_palette_arena();
  Render::GL::BonePaletteSlot palette_slot_h = arena.allocate_palette();
  QMatrix4x4 *palette_slot = palette_slot_h.cpu;

  std::size_t const n =
      std::min<std::size_t>(entry->inverse_bind.size(), kHorseBoneCount);
  for (std::size_t i = 0; i < n; ++i) {
    palette_slot[i] = tmp[i] * entry->inverse_bind[i];
  }

  std::array<QVector3D, 8> role_colors{};
  fill_horse_role_colors(variant, role_colors);

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.material = nullptr;
  cmd.world = world_from_unit;
  cmd.bone_palette = palette_slot;
  cmd.palette_ubo = palette_slot_h.ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(palette_slot_h.offset);
  cmd.bone_count = static_cast<std::uint32_t>(n);
  cmd.role_color_count = static_cast<std::uint32_t>(role_colors.size());
  for (std::size_t i = 0; i < role_colors.size(); ++i) {
    cmd.role_colors[i] = role_colors[i];
  }
  cmd.color = variant.coat_color;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;

  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
}

} // namespace

auto horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "horse";
    s.topology = horse_topology();
    s.lod_minimal = PartGraph{
        std::span<const PrimitiveInstance>(static_minimal_parts().data(),
                                           static_minimal_parts().size()),
    };
    s.lod_reduced = PartGraph{
        std::span<const PrimitiveInstance>(static_reduced_parts().data(),
                                           static_reduced_parts().size()),
    };
    s.lod_full = PartGraph{
        std::span<const PrimitiveInstance>(static_full_parts().data(),
                                           static_full_parts().size()),
    };
    return s;
  }();
  return spec;
}

auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kHorseBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kHorseBoneCount);
}

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kHorseBoneCount> palette =
      build_horse_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

void submit_horse_reduced_rigged(const HorseSpecPose &pose,
                                 const Render::GL::HorseVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {
  submit_horse_rigged_impl(pose, variant,
                           Render::Creature::CreatureLOD::Reduced,
                           world_from_unit, out);
}

void submit_horse_full_rigged(const HorseSpecPose &pose,
                              const Render::GL::HorseVariant &variant,
                              const QMatrix4x4 &world_from_unit,
                              Render::GL::ISubmitter &out) noexcept {
  submit_horse_rigged_impl(pose, variant, Render::Creature::CreatureLOD::Full,
                           world_from_unit, out);
}

void submit_horse_minimal_rigged(const HorseSpecPose &pose,
                                 const Render::GL::HorseVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {
  submit_horse_rigged_impl(pose, variant,
                           Render::Creature::CreatureLOD::Minimal,
                           world_from_unit, out);
}

} // namespace Render::Horse
