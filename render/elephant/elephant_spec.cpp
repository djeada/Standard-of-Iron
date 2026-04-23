#include "elephant_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace Render::Elephant {

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

constexpr std::array<BoneDef, kElephantBoneCount> kElephantBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", 0},
    {"FootFL", 0},
    {"FootFR", 0},
    {"FootBL", 0},
    {"FootBR", 0},
    {"Head", 0},
    {"TrunkTip", 0},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kElephantSockets{};

constexpr SkeletonTopology kElephantTopology{
    std::span<const BoneDef>(kElephantBones),
    std::span<const Render::Creature::SocketDef>(kElephantSockets),
};

constexpr std::uint8_t kRoleSkin = 1;
constexpr std::uint8_t kRoleSkinShadow = 2;
constexpr std::uint8_t kRoleSkinReducedLeg = 3;
constexpr std::uint8_t kRoleSkinReducedFootPad = 4;
constexpr std::uint8_t kRoleSkinReducedTrunk = 5;
constexpr std::uint8_t kRoleEarInner = 6;
constexpr std::uint8_t kRoleTusk = 7;
constexpr std::uint8_t kRoleEye = 8;
constexpr std::uint8_t kRoleToenail = 9;
constexpr std::uint8_t kRoleTailTip = 10;

constexpr float k_pi = 3.14159265358979323846F;

[[nodiscard]] auto
translation_matrix(const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setToIdentity();
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

[[nodiscard]] auto darken(const QVector3D &c, float s) noexcept -> QVector3D {
  return c * s;
}

struct ReducedLegResult {
  QVector3D shoulder;
  QVector3D foot;
};

[[nodiscard]] auto compute_reduced_leg(
    const Render::GL::ElephantDimensions &d, const Render::GL::ElephantGait &g,
    const ElephantReducedMotion &motion, float lateral_sign, float forward_bias,
    float phase_offset) noexcept -> ReducedLegResult {
  float const leg_phase =
      motion.is_fighting
          ? std::fmod(motion.anim_time / 1.15F + phase_offset, 1.0F)
          : std::fmod(motion.phase + phase_offset, 1.0F);

  float stride = 0.0F;
  float lift = 0.0F;

  if (motion.is_fighting) {
    bool const is_front = (forward_bias > 0.0F);
    float const local = leg_phase;

    float intensity = 0.70F;
    switch (motion.combat_phase) {
    case Render::GL::CombatAnimPhase::WindUp:
      intensity = 0.85F;
      break;
    case Render::GL::CombatAnimPhase::Strike:
    case Render::GL::CombatAnimPhase::Impact:
      intensity = 1.0F;
      break;
    case Render::GL::CombatAnimPhase::Recover:
      intensity = 0.80F;
      break;
    default:
      break;
    }

    float const base_stomp = d.leg_length * 0.58F;
    float const stomp_height =
        base_stomp * (0.7F + 0.3F * intensity) * (is_front ? 1.0F : 0.80F);
    float const stomp_stride = g.stride_swing * 0.32F * intensity;
    float const impact_sink =
        d.foot_radius * (0.20F + 0.10F * intensity) * (is_front ? 1.0F : 0.85F);

    if (local < 0.45F) {
      float const u = local / 0.45F;
      float const ease = 1.0F - std::cos(u * k_pi * 0.5F);
      lift = ease * stomp_height;
      stride = stomp_stride * ease * 0.35F;
    } else if (local < 0.65F) {
      lift = stomp_height;
      stride = stomp_stride * 0.35F;
    } else if (local < 0.78F) {
      float const u = (local - 0.65F) / 0.13F;
      float const slam = (1.0F - u);
      float const slam_pow = slam * slam * slam * slam;
      lift = slam_pow * stomp_height - impact_sink * (u * u);
      stride = stomp_stride * (0.35F + u * 0.65F);
    } else {
      float const u = (local - 0.78F) / 0.22F;
      float const recover = 1.0F - (u * u);
      lift = -impact_sink * recover;
      stride = stomp_stride * (1.0F - u * 0.25F);
    }
  } else if (motion.is_moving) {
    float const angle = leg_phase * 2.0F * k_pi;
    stride = std::sin(angle) * g.stride_swing * 0.6F;
    float const lift_raw = std::sin(angle);
    lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * 0.8F : 0.0F;
  }

  QVector3D const shoulder(lateral_sign * d.body_width * 0.58F,
                           -d.body_height * 0.30F, forward_bias + stride);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -d.leg_length * 0.85F + lift, stride * 0.3F);

  return {shoulder, foot};
}

auto build_minimal_primitives(const ElephantSpecPose &pose,
                              std::array<PrimitiveInstance, 5> &out) noexcept
    -> std::size_t {
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "elephant.body";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = Render::GL::get_unit_sphere();
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Body);
    p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
    p.color_role = kRoleSkin;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }

  struct LegSpec {
    std::string_view name;
    ElephantBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<LegSpec, 4> const legs{{
      {"elephant.leg.fl", ElephantBone::FootFL, pose.shoulder_offset_fl},
      {"elephant.leg.fr", ElephantBone::FootFR, pose.shoulder_offset_fr},
      {"elephant.leg.bl", ElephantBone::FootBL, pose.shoulder_offset_bl},
      {"elephant.leg.br", ElephantBone::FootBR, pose.shoulder_offset_br},
  }};

  for (std::size_t i = 0; i < 4; ++i) {
    PrimitiveInstance &p = out[1 + i];
    p.debug_name = legs[i].name;
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Root);
    p.params.head_offset = legs[i].shoulder_offset;
    p.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    p.params.tail_offset = QVector3D();
    p.params.radius = pose.leg_radius;
    p.color_role = kRoleSkinShadow;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 5;
}

auto build_reduced_primitives(const ElephantSpecPose &pose,
                              std::array<PrimitiveInstance, 12> &out) noexcept
    -> std::size_t {

  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "elephant.body.reduced";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Root);
    p.params.half_extents = pose.reduced_body_half;
    p.color_role = kRoleSkin;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[1];
    p.debug_name = "elephant.neck";
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Root);
    p.params.head_offset = pose.neck_base_offset;
    p.params.tail_bone = static_cast<BoneIndex>(ElephantBone::Head);
    p.params.radius = pose.neck_radius;
    p.color_role = kRoleSkin;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[2];
    p.debug_name = "elephant.head";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Head);
    p.params.half_extents = pose.head_half;
    p.color_role = kRoleSkin;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[3];
    p.debug_name = "elephant.trunk";
    p.shape = PrimitiveShape::Cone;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::TrunkTip);
    p.params.tail_bone = static_cast<BoneIndex>(ElephantBone::Head);
    p.params.radius = pose.trunk_base_radius;
    p.color_role = kRoleSkinReducedTrunk;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  struct RLeg {
    std::string_view leg_name;
    std::string_view pad_name;
    ElephantBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<RLeg, 4> const legs{{
      {"elephant.leg.fl.r", "elephant.foot_pad.fl.r", ElephantBone::FootFL,
       pose.shoulder_offset_reduced_fl},
      {"elephant.leg.fr.r", "elephant.foot_pad.fr.r", ElephantBone::FootFR,
       pose.shoulder_offset_reduced_fr},
      {"elephant.leg.bl.r", "elephant.foot_pad.bl.r", ElephantBone::FootBL,
       pose.shoulder_offset_reduced_bl},
      {"elephant.leg.br.r", "elephant.foot_pad.br.r", ElephantBone::FootBR,
       pose.shoulder_offset_reduced_br},
  }};

  for (std::size_t i = 0; i < 4; ++i) {
    PrimitiveInstance &leg = out[4 + (i * 2)];
    leg.debug_name = legs[i].leg_name;
    leg.shape = PrimitiveShape::Cylinder;
    leg.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Root);
    leg.params.head_offset = legs[i].shoulder_offset;
    leg.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    leg.params.tail_offset = QVector3D();
    leg.params.radius = pose.leg_radius_reduced;
    leg.color_role = kRoleSkinReducedLeg;
    leg.material_id = 0;
    leg.lod_mask = kLodReduced;

    PrimitiveInstance &pad = out[5 + (i * 2)];
    pad.debug_name = legs[i].pad_name;
    pad.shape = PrimitiveShape::Mesh;
    pad.custom_mesh = Render::GL::get_unit_sphere();
    pad.params.anchor_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    pad.params.head_offset = QVector3D(0.0F, pose.foot_pad_offset_y, 0.0F);
    pad.params.half_extents = pose.foot_pad_half;
    pad.color_role = kRoleSkinReducedFootPad;
    pad.material_id = 8;
    pad.lod_mask = kLodReduced;
  }
  return 12;
}

} // namespace

auto elephant_topology() noexcept -> const SkeletonTopology & {
  return kElephantTopology;
}

void evaluate_elephant_skeleton(const ElephantSpecPose &pose,
                                BonePalette &out_palette) noexcept {
  out_palette[static_cast<std::size_t>(ElephantBone::Root)] =
      translation_matrix(pose.barrel_center);

  QMatrix4x4 body;
  body.setColumn(0, QVector4D(pose.body_ellipsoid_x, 0.0F, 0.0F, 0.0F));
  body.setColumn(1, QVector4D(0.0F, pose.body_ellipsoid_y, 0.0F, 0.0F));
  body.setColumn(2, QVector4D(0.0F, 0.0F, pose.body_ellipsoid_z, 0.0F));
  body.setColumn(3, QVector4D(pose.barrel_center, 1.0F));
  out_palette[static_cast<std::size_t>(ElephantBone::Body)] = body;

  out_palette[static_cast<std::size_t>(ElephantBone::FootFL)] =
      translation_matrix(pose.foot_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::FootFR)] =
      translation_matrix(pose.foot_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::FootBL)] =
      translation_matrix(pose.foot_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::FootBR)] =
      translation_matrix(pose.foot_br);

  out_palette[static_cast<std::size_t>(ElephantBone::Head)] =
      translation_matrix(pose.head_center);
  out_palette[static_cast<std::size_t>(ElephantBone::TrunkTip)] =
      translation_matrix(pose.trunk_end);
}

void fill_elephant_role_colors(
    const Render::GL::ElephantVariant &variant,
    std::array<QVector3D, kElephantRoleCount> &out_roles) noexcept {
  out_roles[0] = variant.skin_color;
  out_roles[1] = variant.skin_color;
  out_roles[2] = darken(variant.skin_color, 0.92F);
  out_roles[3] = darken(variant.skin_color, 0.88F);
  out_roles[4] = darken(variant.skin_color, 0.94F);
  out_roles[5] = variant.ear_inner_color;
  out_roles[6] = variant.tusk_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
  out_roles[8] = variant.toenail_color;
  out_roles[9] = QVector3D(0.05F, 0.04F, 0.04F);
}

void make_elephant_spec_pose(const Render::GL::ElephantDimensions &dims,
                             float bob, ElephantSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.34F;
  out_pose.body_ellipsoid_y = dims.body_height + dims.neck_length * 0.22F;
  out_pose.body_ellipsoid_z = dims.body_length + dims.head_length * 0.24F;

  float const shoulder_dx = dims.body_width * 0.60F;
  float const shoulder_dy = -dims.body_height * 0.34F;
  float const front_dz = dims.body_length * 0.33F;
  float const rear_dz = -dims.body_length * 0.31F;

  out_pose.shoulder_offset_fl = QVector3D(shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr = QVector3D(-shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(shoulder_dx, shoulder_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-shoulder_dx, shoulder_dy, rear_dz);

  float const drop = -dims.leg_length * 0.88F;
  out_pose.foot_fl =
      center + out_pose.shoulder_offset_fl + QVector3D(0, drop, 0);
  out_pose.foot_fr =
      center + out_pose.shoulder_offset_fr + QVector3D(0, drop, 0);
  out_pose.foot_bl =
      center + out_pose.shoulder_offset_bl + QVector3D(0, drop, 0);
  out_pose.foot_br =
      center + out_pose.shoulder_offset_br + QVector3D(0, drop, 0);

  out_pose.leg_radius = dims.leg_radius * 0.95F;
}

void make_elephant_spec_pose_reduced(const Render::GL::ElephantDimensions &dims,
                                     const Render::GL::ElephantGait &gait,
                                     const ElephantReducedMotion &motion,
                                     ElephantSpecPose &out_pose) noexcept {

  make_elephant_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  out_pose.reduced_body_half =
      QVector3D(dims.body_width * 0.58F, dims.body_height * 0.46F,
                dims.body_length * 0.40F);

  QVector3D const neck_base_world =
      center +
      QVector3D(0.0F, dims.body_height * 0.22F, dims.body_length * 0.42F);
  out_pose.neck_base_offset = neck_base_world - center;
  out_pose.neck_radius = dims.neck_width * 0.95F;

  out_pose.head_center =
      neck_base_world +
      QVector3D(0.0F, dims.neck_length * 0.46F, dims.head_length * 0.58F);

  out_pose.head_half =
      QVector3D(dims.head_width * 0.46F, dims.head_height * 0.42F,
                dims.head_length * 0.40F);

  out_pose.trunk_end =
      out_pose.head_center +
      QVector3D(0.0F, -dims.trunk_length * 0.58F, dims.trunk_length * 0.28F);
  out_pose.trunk_base_radius = dims.trunk_base_radius * 0.8F;

  float const front_forward = dims.body_length * 0.35F;
  float const rear_forward = -dims.body_length * 0.35F;

  auto apply_leg = [&](float lat, float fwd, float phase_offset,
                       QVector3D &shoulder_out, QVector3D &foot_out) noexcept {
    auto r = compute_reduced_leg(dims, gait, motion, lat, fwd, phase_offset);
    shoulder_out = r.shoulder;
    foot_out = center + r.foot;
  };

  apply_leg(1.0F, front_forward, gait.front_leg_phase,
            out_pose.shoulder_offset_reduced_fl, out_pose.foot_reduced_fl);
  apply_leg(-1.0F, front_forward, gait.front_leg_phase + 0.50F,
            out_pose.shoulder_offset_reduced_fr, out_pose.foot_reduced_fr);
  apply_leg(1.0F, rear_forward, gait.rear_leg_phase,
            out_pose.shoulder_offset_reduced_bl, out_pose.foot_reduced_bl);
  apply_leg(-1.0F, rear_forward, gait.rear_leg_phase + 0.50F,
            out_pose.shoulder_offset_reduced_br, out_pose.foot_reduced_br);

  out_pose.foot_fl = out_pose.foot_reduced_fl;
  out_pose.foot_fr = out_pose.foot_reduced_fr;
  out_pose.foot_bl = out_pose.foot_reduced_bl;
  out_pose.foot_br = out_pose.foot_reduced_br;

  out_pose.leg_radius_reduced = dims.leg_radius * 0.95F;

  out_pose.foot_pad_offset_y = -dims.foot_radius * 0.18F;
  out_pose.foot_pad_half = QVector3D(dims.foot_radius * 1.15F,
                                     dims.foot_radius * 0.70F,
                                     dims.foot_radius * 1.20F);
}

} // namespace Render::Elephant

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Elephant {

namespace {

auto build_baseline_pose() noexcept -> ElephantSpecPose {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  Render::GL::ElephantGait gait{};
  ElephantSpecPose pose{};
  make_elephant_spec_pose_reduced(dims, gait, ElephantReducedMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const ElephantSpecPose & {
  static const ElephantSpecPose pose = build_baseline_pose();
  return pose;
}

constexpr std::size_t kElephantFullPartCount = 50;

auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, kElephantFullPartCount> {
  ElephantSpecPose const &pose = baseline_pose();
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  std::array<PrimitiveInstance, kElephantFullPartCount> out{};

  using Render::Creature::kLodFull;

  auto root = static_cast<BoneIndex>(ElephantBone::Root);
  auto head_bone = static_cast<BoneIndex>(ElephantBone::Head);

  auto ell = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role = kRoleSkin, int material_id = 6) {
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

  auto box = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role, int material_id = 6) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Box;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.half_extents = half_extents;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  float const bw = dims.body_width;
  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const hl = dims.head_length;
  float const hw = dims.head_width;
  float const hh = dims.head_height;

  std::size_t i = 0;

  float const bw_v = bw * 1.35F;
  float const bh_v = bh * 0.88F;

  ell(out[i++], "elephant.full.body.barrel", root,
      QVector3D(bw_v * 0.78F, bh_v * 0.58F, bl * 0.46F),
      QVector3D(0.0F, bh_v * 0.05F, 0.0F));

  ell(out[i++], "elephant.full.body.shoulder_hump", root,
      QVector3D(bw_v * 0.58F, bh_v * 0.26F, bl * 0.20F),
      QVector3D(0.0F, bh_v * 0.52F, bl * 0.20F));

  ell(out[i++], "elephant.full.body.midback", root,
      QVector3D(bw_v * 0.55F, bh_v * 0.12F, bl * 0.20F),
      QVector3D(0.0F, bh_v * 0.42F, -bl * 0.02F));

  ell(out[i++], "elephant.full.body.rump", root,
      QVector3D(bw_v * 0.66F, bh_v * 0.26F, bl * 0.22F),
      QVector3D(0.0F, bh_v * 0.45F, -bl * 0.28F));

  ell(out[i++], "elephant.full.body.belly", root,
      QVector3D(bw_v * 0.72F, bh_v * 0.40F, bl * 0.40F),
      QVector3D(0.0F, -bh_v * 0.26F, 0.0F));

  ell(out[i++], "elephant.full.body.chest", root,
      QVector3D(bw_v * 0.66F, bh_v * 0.48F, bl * 0.20F),
      QVector3D(0.0F, bh_v * 0.05F, bl * 0.36F));

  ell(out[i++], "elephant.full.body.brisket", root,
      QVector3D(bw_v * 0.46F, bh_v * 0.24F, bl * 0.18F),
      QVector3D(0.0F, -bh_v * 0.28F, bl * 0.34F));

  sph(out[i++], "elephant.full.body.flank.l", root,
      QVector3D(bw_v * 0.55F, -bh_v * 0.02F, -bl * 0.10F), bw_v * 0.32F,
      kRoleSkin, 6);
  sph(out[i++], "elephant.full.body.flank.r", root,
      QVector3D(-bw_v * 0.55F, -bh_v * 0.02F, -bl * 0.10F), bw_v * 0.32F,
      kRoleSkin, 6);

  ell(out[i++], "elephant.full.head.skull", head_bone,
      QVector3D(hw * 0.45F, hh * 0.48F, hl * 0.42F),
      QVector3D(0.0F, hh * 0.05F, 0.0F));

  ell(out[i++], "elephant.full.head.forehead.l", head_bone,
      QVector3D(hw * 0.20F, hh * 0.22F, hl * 0.18F),
      QVector3D(hw * 0.18F, hh * 0.32F, -hl * 0.05F));
  ell(out[i++], "elephant.full.head.forehead.r", head_bone,
      QVector3D(hw * 0.20F, hh * 0.22F, hl * 0.18F),
      QVector3D(-hw * 0.18F, hh * 0.32F, -hl * 0.05F));

  ell(out[i++], "elephant.full.head.cheek.l", head_bone,
      QVector3D(hw * 0.18F, hh * 0.24F, hl * 0.26F),
      QVector3D(hw * 0.32F, -hh * 0.05F, hl * 0.05F));
  ell(out[i++], "elephant.full.head.cheek.r", head_bone,
      QVector3D(hw * 0.18F, hh * 0.24F, hl * 0.26F),
      QVector3D(-hw * 0.32F, -hh * 0.05F, hl * 0.05F));

  ell(out[i++], "elephant.full.head.jaw", head_bone,
      QVector3D(hw * 0.30F, hh * 0.16F, hl * 0.28F),
      QVector3D(0.0F, -hh * 0.30F, hl * 0.18F));

  sph(out[i++], "elephant.full.head.eye.l", head_bone,
      QVector3D(hw * 0.36F, hh * 0.10F, hl * 0.20F), hw * 0.05F, kRoleEye, 0);
  sph(out[i++], "elephant.full.head.eye.r", head_bone,
      QVector3D(-hw * 0.36F, hh * 0.10F, hl * 0.20F), hw * 0.05F, kRoleEye, 0);

  float const tl = dims.trunk_length;

  float const tbr = dims.trunk_base_radius * 1.45F;
  float const ttr = std::max(dims.trunk_tip_radius, tbr * 0.30F);

  QVector3D const trunk_base(0.0F, -hh * 0.20F, hl * 0.45F);
  constexpr int kTrunkSegments = 7;
  std::array<QVector3D, kTrunkSegments + 1> trunk_pts{};
  for (int s = 0; s <= kTrunkSegments; ++s) {
    float const t = static_cast<float>(s) / static_cast<float>(kTrunkSegments);

    float drop = tl * 0.85F * t;
    float fwd = tl * 0.05F * t;

    if (t > 0.65F) {
      float const u = (t - 0.65F) / 0.35F;
      float const curl = u * u;
      fwd += tl * 0.45F * curl;
      drop -= tl * 0.10F * curl;
    }
    trunk_pts[s] = trunk_base + QVector3D(0.0F, -drop, fwd);
  }
  std::array<std::string_view, kTrunkSegments> const trunk_names{{
      "elephant.full.trunk.seg.0",
      "elephant.full.trunk.seg.1",
      "elephant.full.trunk.seg.2",
      "elephant.full.trunk.seg.3",
      "elephant.full.trunk.seg.4",
      "elephant.full.trunk.seg.5",
      "elephant.full.trunk.seg.6",
  }};
  for (int s = 0; s < kTrunkSegments; ++s) {
    float const t0 = static_cast<float>(s) / static_cast<float>(kTrunkSegments);

    float const r = tbr + (ttr - tbr) * (t0 + 0.5F / kTrunkSegments);
    cap(out[i++], trunk_names[s], head_bone, trunk_pts[s], head_bone,
        trunk_pts[s + 1], r, kRoleSkin, 6);
  }

  sph(out[i++], "elephant.full.trunk.tip", head_bone, trunk_pts[kTrunkSegments],
      ttr * 1.05F, kRoleSkin, 6);

  float const ear_w = dims.ear_width;
  float const ear_h = dims.ear_height;
  float const ear_t = dims.ear_thickness;

  QVector3D const ear_half(ear_w * 0.70F, ear_h * 0.62F, ear_w * 0.08F);
  QVector3D const ear_inner_half(ear_w * 0.50F, ear_h * 0.46F, ear_w * 0.06F);

  ell(out[i++], "elephant.full.ear.outer.l", head_bone, ear_half,
      QVector3D(hw * 0.55F + ear_w * 0.55F, hh * 0.05F, -hl * 0.05F), kRoleSkin,
      6);
  ell(out[i++], "elephant.full.ear.outer.r", head_bone, ear_half,
      QVector3D(-(hw * 0.55F + ear_w * 0.55F), hh * 0.05F, -hl * 0.05F),
      kRoleSkin, 6);

  ell(out[i++], "elephant.full.ear.inner.l", head_bone, ear_inner_half,
      QVector3D(hw * 0.55F + ear_w * 0.50F, hh * 0.05F,
                -hl * 0.05F + ear_w * 0.10F),
      kRoleEarInner, 6);
  ell(out[i++], "elephant.full.ear.inner.r", head_bone, ear_inner_half,
      QVector3D(-(hw * 0.55F + ear_w * 0.50F), hh * 0.05F,
                -hl * 0.05F + ear_w * 0.10F),
      kRoleEarInner, 6);

  float const tk_l = dims.tusk_length;
  float const tk_r = dims.tusk_radius * 1.6F;
  QVector3D const tusk_root_l(hw * 0.22F, -hh * 0.18F, hl * 0.42F);
  QVector3D const tusk_root_r(-hw * 0.22F, -hh * 0.18F, hl * 0.42F);
  QVector3D const tusk_tip_l(hw * 0.55F, -hh * 0.18F + tk_l * 0.10F,
                             hl * 0.42F + tk_l * 0.95F);
  QVector3D const tusk_tip_r(-hw * 0.55F, -hh * 0.18F + tk_l * 0.10F,
                             hl * 0.42F + tk_l * 0.95F);
  cone_p(out[i++], "elephant.full.tusk.l", head_bone, tusk_root_l, head_bone,
         tusk_tip_l, tk_r, kRoleTusk, 0);
  cone_p(out[i++], "elephant.full.tusk.r", head_bone, tusk_root_r, head_bone,
         tusk_tip_r, tk_r, kRoleTusk, 0);

  float const tail_l = dims.tail_length;
  QVector3D const tail_root(0.0F, bh * 0.18F, -bl * 0.50F);
  QVector3D const tail_mid =
      tail_root + QVector3D(0.0F, -tail_l * 0.30F, -tail_l * 0.30F);
  QVector3D const tail_end =
      tail_root + QVector3D(0.0F, -tail_l * 0.85F, -tail_l * 0.45F);
  cap(out[i++], "elephant.full.tail.dock", root, tail_root, root, tail_mid,
      bw * 0.06F, kRoleSkin, 6);
  cap(out[i++], "elephant.full.tail.mid", root, tail_mid, root, tail_end,
      bw * 0.04F, kRoleSkinShadow, 6);
  sph(out[i++], "elephant.full.tail.switch", root,
      tail_end + QVector3D(0.0F, -bw * 0.04F, -bw * 0.02F), bw * 0.07F,
      kRoleTailTip, 0);

  struct LegSpec {
    std::string_view upper_name;
    std::string_view knee_name;
    std::string_view lower_name;
    std::string_view foot_name;
    ElephantBone foot_bone;
    QVector3D shoulder_offset;
    bool is_front;
  };
  std::array<LegSpec, 4> const legs{{
      {"elephant.full.leg.fl.upper", "elephant.full.leg.fl.knee",
       "elephant.full.leg.fl.lower", "elephant.full.foot.fl",
       ElephantBone::FootFL, pose.shoulder_offset_reduced_fl, true},
      {"elephant.full.leg.fr.upper", "elephant.full.leg.fr.knee",
       "elephant.full.leg.fr.lower", "elephant.full.foot.fr",
       ElephantBone::FootFR, pose.shoulder_offset_reduced_fr, true},
      {"elephant.full.leg.bl.upper", "elephant.full.leg.bl.knee",
       "elephant.full.leg.bl.lower", "elephant.full.foot.bl",
       ElephantBone::FootBL, pose.shoulder_offset_reduced_bl, false},
      {"elephant.full.leg.br.upper", "elephant.full.leg.br.knee",
       "elephant.full.leg.br.lower", "elephant.full.foot.br",
       ElephantBone::FootBR, pose.shoulder_offset_reduced_br, false},
  }};
  float const leg_len = dims.leg_length;

  float const upper_r_front = dims.leg_radius * 1.85F;
  float const upper_r_rear = dims.leg_radius * 1.78F;
  float const lower_r_front = dims.leg_radius * 1.72F;
  float const lower_r_rear = dims.leg_radius * 1.65F;
  float const knee_r = dims.leg_radius * 1.75F;
  float const foot_pad_y = -dims.foot_radius * 0.18F;

  QVector3D const foot_half(dims.foot_radius * 1.90F, dims.foot_radius * 0.65F,
                            dims.foot_radius * 1.90F);
  for (auto const &leg : legs) {
    auto foot_b = static_cast<BoneIndex>(leg.foot_bone);
    float const upper_r = leg.is_front ? upper_r_front : upper_r_rear;
    float const lower_r = leg.is_front ? lower_r_front : lower_r_rear;

    cap(out[i++], leg.upper_name, root, leg.shoulder_offset, foot_b,
        QVector3D(0.0F, leg_len * 0.50F, 0.0F), upper_r, kRoleSkin, 6);

    sph(out[i++], leg.knee_name, foot_b, QVector3D(0.0F, leg_len * 0.50F, 0.0F),
        knee_r, kRoleSkinShadow, 0);

    cyl(out[i++], leg.lower_name, foot_b,
        QVector3D(0.0F, leg_len * 0.50F, 0.0F), foot_b,
        QVector3D(0.0F, dims.foot_radius * 0.4F, 0.0F), lower_r, kRoleSkin, 6);

    PrimitiveInstance &foot = out[i++];
    foot.debug_name = leg.foot_name;
    foot.shape = PrimitiveShape::Mesh;
    foot.custom_mesh = Render::GL::get_unit_sphere();
    foot.params.anchor_bone = foot_b;
    foot.params.head_offset = QVector3D(0.0F, foot_pad_y, 0.0F);
    foot.params.half_extents = foot_half;
    foot.color_role = kRoleSkinShadow;
    foot.material_id = 8;
    foot.lod_mask = kLodFull;
  }

  (void)i;

  return out;
}

auto build_static_reduced_parts() noexcept
    -> std::array<PrimitiveInstance, 12> {
  std::array<PrimitiveInstance, 12> out{};
  build_reduced_primitives(baseline_pose(), out);
  return out;
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

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, kElephantFullPartCount> & {
  static const auto parts = build_static_full_parts();
  return parts;
}

auto static_reduced_parts() noexcept
    -> const std::array<PrimitiveInstance, 12> & {
  static const auto parts = build_static_reduced_parts();
  return parts;
}

auto build_elephant_bind_palette() noexcept
    -> std::array<QMatrix4x4, kElephantBoneCount> {
  std::array<QMatrix4x4, kElephantBoneCount> out{};
  BonePalette tmp{};
  evaluate_elephant_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < kElephantBoneCount; ++i) {
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

void submit_elephant_rigged_impl(const ElephantSpecPose &pose,
                                 const Render::GL::ElephantVariant &variant,
                                 Render::Creature::CreatureLOD lod,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {

  BonePalette tmp{};
  evaluate_elephant_skeleton(pose, tmp);

  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    if (lod == Render::Creature::CreatureLOD::Billboard) {
      return;
    }
    std::array<QVector3D, kElephantRoleCount> role_colors{};
    fill_elephant_role_colors(variant, role_colors);
    Render::Creature::submit_creature(
        elephant_creature_spec(),
        std::span<const QMatrix4x4>(tmp.data(), tmp.size()), lod,
        world_from_unit, out,
        std::span<const QVector3D>(role_colors.data(), role_colors.size()));
    return;
  }

  auto const &spec = elephant_creature_spec();
  auto bind = elephant_bind_palette();

  auto *entry = renderer->rigged_mesh_cache().get_or_bake(spec, lod, bind, 0);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  auto &arena = renderer->bone_palette_arena();
  Render::GL::BonePaletteSlot palette_slot_h = arena.allocate_palette();
  QMatrix4x4 *palette_slot = palette_slot_h.cpu;

  std::size_t const n =
      std::min<std::size_t>(entry->inverse_bind.size(), kElephantBoneCount);
  for (std::size_t i = 0; i < n; ++i) {
    palette_slot[i] = tmp[i] * entry->inverse_bind[i];
  }

  std::array<QVector3D, kElephantRoleCount> role_colors{};
  fill_elephant_role_colors(variant, role_colors);

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
  cmd.color = variant.skin_color;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
}

} // namespace

auto elephant_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "elephant";
    s.topology = elephant_topology();
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

auto compute_elephant_bone_palette(const ElephantSpecPose &pose,
                                   std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kElephantBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_elephant_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kElephantBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kElephantBoneCount);
}

auto elephant_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kElephantBoneCount> palette =
      build_elephant_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

void submit_elephant_reduced_rigged(const ElephantSpecPose &pose,
                                    const Render::GL::ElephantVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept {
  submit_elephant_rigged_impl(pose, variant,
                              Render::Creature::CreatureLOD::Reduced,
                              world_from_unit, out);
}

void submit_elephant_full_rigged(const ElephantSpecPose &pose,
                                 const Render::GL::ElephantVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {
  submit_elephant_rigged_impl(
      pose, variant, Render::Creature::CreatureLOD::Full, world_from_unit, out);
}

void submit_elephant_minimal_rigged(const ElephantSpecPose &pose,
                                    const Render::GL::ElephantVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept {
  submit_elephant_rigged_impl(pose, variant,
                              Render::Creature::CreatureLOD::Minimal,
                              world_from_unit, out);
}

} // namespace Render::Elephant
