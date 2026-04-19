#include "elephant_spec.h"

#include "../creature/part_graph.h"
#include "../creature/pipeline/creature_frame.h"
#include "../creature/pipeline/creature_pipeline.h"
#include "../creature/pipeline/unit_visual_spec.h"
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

  QVector3D const shoulder(lateral_sign * d.body_width * 0.40F,
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

void fill_elephant_role_colors(const Render::GL::ElephantVariant &variant,
                               std::array<QVector3D, 5> &out_roles) noexcept {
  out_roles[0] = variant.skin_color;
  out_roles[1] = darken(variant.skin_color, 0.80F);
  out_roles[2] = darken(variant.skin_color, 0.88F);
  out_roles[3] = darken(variant.skin_color, 0.75F);
  out_roles[4] = darken(variant.skin_color, 0.90F);
}

void make_elephant_spec_pose(const Render::GL::ElephantDimensions &dims,
                             float bob, ElephantSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.2F;
  out_pose.body_ellipsoid_y = dims.body_height + dims.neck_length * 0.3F;
  out_pose.body_ellipsoid_z = dims.body_length + dims.head_length * 0.3F;

  float const shoulder_dx = dims.body_width * 0.38F;
  float const shoulder_dy = -dims.body_height * 0.25F;
  float const front_dz = dims.body_length * 0.30F;
  float const rear_dz = -dims.body_length * 0.30F;

  out_pose.shoulder_offset_fl = QVector3D(shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr = QVector3D(-shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(shoulder_dx, shoulder_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-shoulder_dx, shoulder_dy, rear_dz);

  float const drop = -dims.leg_length * 0.70F;
  out_pose.foot_fl =
      center + out_pose.shoulder_offset_fl + QVector3D(0, drop, 0);
  out_pose.foot_fr =
      center + out_pose.shoulder_offset_fr + QVector3D(0, drop, 0);
  out_pose.foot_bl =
      center + out_pose.shoulder_offset_bl + QVector3D(0, drop, 0);
  out_pose.foot_br =
      center + out_pose.shoulder_offset_br + QVector3D(0, drop, 0);

  out_pose.leg_radius = dims.leg_radius * 0.70F;
}

void make_elephant_spec_pose_reduced(const Render::GL::ElephantDimensions &dims,
                                     const Render::GL::ElephantGait &gait,
                                     const ElephantReducedMotion &motion,
                                     ElephantSpecPose &out_pose) noexcept {

  make_elephant_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  out_pose.reduced_body_half =
      QVector3D(dims.body_width * 0.5F, dims.body_height * 0.45F,
                dims.body_length * 0.375F);

  QVector3D const neck_base_world =
      center +
      QVector3D(0.0F, dims.body_height * 0.20F, dims.body_length * 0.45F);
  out_pose.neck_base_offset = neck_base_world - center;
  out_pose.neck_radius = dims.neck_width * 0.85F;

  out_pose.head_center =
      neck_base_world +
      QVector3D(0.0F, dims.neck_length * 0.50F, dims.head_length * 0.60F);

  out_pose.head_half =
      QVector3D(dims.head_width * 0.425F, dims.head_height * 0.40F,
                dims.head_length * 0.35F);

  out_pose.trunk_end =
      out_pose.head_center +
      QVector3D(0.0F, -dims.trunk_length * 0.50F, dims.trunk_length * 0.40F);
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

  out_pose.leg_radius_reduced = dims.leg_radius * 0.85F;

  out_pose.foot_pad_offset_y = -dims.foot_radius * 0.18F;
  out_pose.foot_pad_half = QVector3D(dims.foot_radius, dims.foot_radius * 0.65F,
                                     dims.foot_radius * 1.10F);
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

auto build_static_full_parts() noexcept -> std::array<PrimitiveInstance, 15> {
  ElephantSpecPose const &pose = baseline_pose();
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  std::array<PrimitiveInstance, 15> out{};

  std::array<PrimitiveInstance, 12> reduced_buf{};
  build_reduced_primitives(pose, reduced_buf);
  for (std::size_t i = 0; i < 12; ++i) {
    out[i] = reduced_buf[i];
    out[i].lod_mask = Render::Creature::kLodFull;
  }

  auto ell = [](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                const QVector3D &half_extents,
                const QVector3D &offset_from_anchor) {
    p.debug_name = name;
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset_from_anchor;
    p.params.half_extents = half_extents;
    p.color_role = kRoleSkin;
    p.material_id = 6;
    p.lod_mask = Render::Creature::kLodFull;
  };

  QVector3D const chest_off(0.0F, dims.body_height * 0.10F,
                            dims.body_length * 0.30F);
  QVector3D const rump_off(0.0F, dims.body_height * 0.02F,
                           -dims.body_length * 0.32F);
  QVector3D const belly_off(0.0F, -dims.body_height * 0.22F,
                            dims.body_length * 0.05F);

  ell(out[12], "elephant.full.chest",
      static_cast<BoneIndex>(ElephantBone::Root),
      QVector3D(dims.body_width * 0.59F, dims.body_height * 0.50F,
                dims.body_length * 0.18F),
      chest_off);
  ell(out[13], "elephant.full.rump", static_cast<BoneIndex>(ElephantBone::Root),
      QVector3D(dims.body_width * 0.55F, dims.body_height * 0.49F,
                dims.body_length * 0.17F),
      rump_off);
  ell(out[14], "elephant.full.belly",
      static_cast<BoneIndex>(ElephantBone::Root),
      QVector3D(dims.body_width * 0.50F, dims.body_height * 0.35F,
                dims.body_length * 0.275F),
      belly_off);

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

auto static_full_parts() noexcept -> const std::array<PrimitiveInstance, 15> & {
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
    std::array<QVector3D, 5> role_colors{};
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

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.material = nullptr;
  cmd.world = world_from_unit;
  cmd.bone_palette = palette_slot;
  cmd.palette_ubo = palette_slot_h.ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(palette_slot_h.offset);
  cmd.bone_count = static_cast<std::uint32_t>(n);
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

void submit_elephant_via_pipeline(const Render::GL::ElephantRendererBase &owner,
                                  const ElephantSpecPose &pose,
                                  const Render::GL::ElephantVariant &variant,
                                  const QMatrix4x4 &world_from_unit,
                                  std::uint32_t inst_seed,
                                  Render::Creature::CreatureLOD lod,
                                  Render::GL::ISubmitter &out) noexcept {
  thread_local Render::Creature::Pipeline::CreaturePipeline pipeline;
  thread_local Render::Creature::Pipeline::CreatureFrame frame;
  thread_local std::array<Render::Creature::Pipeline::UnitVisualSpec, 1> specs;

  frame.clear();
  specs[0] = owner.visual_spec();
  specs[0].kind = Render::Creature::Pipeline::CreatureKind::Elephant;

  frame.push_elephant(0, world_from_unit, 0, inst_seed, lod, pose, variant);

  Render::Creature::Pipeline::FrameContext fctx{};
  pipeline.submit(fctx,
                  std::span<const Render::Creature::Pipeline::UnitVisualSpec>{
                      specs.data(), specs.size()},
                  frame, out);
}

} // namespace Render::Elephant
