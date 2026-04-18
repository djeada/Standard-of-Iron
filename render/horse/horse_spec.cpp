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

// Color roles resolved by submit_horse_lod:
constexpr std::uint8_t kRoleCoat = 1;           // solid coat
constexpr std::uint8_t kRoleCoatDark = 2;       // Minimal legs
constexpr std::uint8_t kRoleCoatReducedLeg = 3; // Reduced legs
constexpr std::uint8_t kRoleHoof = 4;           // hoof_color

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

  // Minimal body — basis columns encode the Minimal ellipsoid scale.
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
                            std::array<QVector3D, 4> &out_roles) noexcept {
  QVector3D const coat = variant.coat_color;
  out_roles[0] = coat;
  out_roles[1] = coat * 0.75F;
  out_roles[2] = coat * 0.85F;
  out_roles[3] = variant.hoof_color;
}

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.2F;
  out_pose.body_ellipsoid_y = dims.body_height + dims.neck_rise * 0.5F;
  out_pose.body_ellipsoid_z = dims.body_length + dims.head_length * 0.5F;

  float const shoulder_dx = dims.body_width * 0.40F;
  float const shoulder_dy = -dims.body_height * 0.3F;
  float const front_dz = dims.body_length * 0.25F;
  float const rear_dz = -dims.body_length * 0.25F;

  out_pose.shoulder_offset_fl = QVector3D(shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr = QVector3D(-shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(shoulder_dx, shoulder_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-shoulder_dx, shoulder_dy, rear_dz);

  float const drop = -dims.leg_length * 0.60F;
  out_pose.foot_fl = center + out_pose.shoulder_offset_fl + QVector3D(0, drop, 0);
  out_pose.foot_fr = center + out_pose.shoulder_offset_fr + QVector3D(0, drop, 0);
  out_pose.foot_bl = center + out_pose.shoulder_offset_bl + QVector3D(0, drop, 0);
  out_pose.foot_br = center + out_pose.shoulder_offset_br + QVector3D(0, drop, 0);

  out_pose.leg_radius = dims.body_width * 0.15F;
}

namespace {

// Legacy `render_simplified` leg math, factored out so the Reduced
// pose builder and the parity tests can share the exact same
// expression tree.
struct ReducedLegSample {
  QVector3D shoulder; // local to barrel_center (relative offset)
  QVector3D foot;     // local to barrel_center (relative offset)
};

auto compute_reduced_leg(const Render::GL::HorseDimensions &dims,
                         const Render::GL::HorseGait &gait,
                         float phase_base, float phase_offset,
                         float lateral_sign, float forward_bias,
                         bool is_moving,
                         const QVector3D &anchor_offset) noexcept
    -> ReducedLegSample {
  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  float const leg_phase = std::fmod(phase_base + phase_offset, 1.0F);
  float stride = 0.0F;
  float lift = 0.0F;
  if (is_moving) {
    float const angle = leg_phase * k_two_pi;
    stride = std::sin(angle) * gait.stride_swing * 0.6F + forward_bias;
    float const lift_raw = std::sin(angle);
    lift = lift_raw > 0.0F ? lift_raw * gait.stride_lift * 0.8F : 0.0F;
  }

  float const shoulder_out = dims.body_width * 0.45F;
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out, lift * 0.05F, stride);

  float const leg_length = dims.leg_length * 0.85F;
  QVector3D const foot = shoulder + QVector3D(0.0F, -leg_length + lift, 0.0F);

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept {
  // Start from the Minimal pose to keep Body basis + Minimal leg
  // fields valid in case a downstream consumer ever needs them.
  make_horse_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  // --- Reduced body ellipsoid ---------------------------------------
  // Legacy: body.scale(body_width, body_height*0.85, body_length*0.80)
  // on a unit sphere. half_extents = that * 0.5.
  out_pose.reduced_body_half =
      QVector3D(dims.body_width * 0.5F, dims.body_height * 0.425F,
                dims.body_length * 0.40F);

  // --- Neck ---------------------------------------------------------
  out_pose.neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.35F,
                         dims.body_length * 0.35F);
  out_pose.neck_top =
      out_pose.neck_base + QVector3D(0.0F, dims.neck_rise, dims.neck_length);
  out_pose.neck_radius = dims.body_width * 0.40F;

  // --- Head ---------------------------------------------------------
  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(0.0F, dims.head_height * 0.10F, dims.head_length * 0.40F);
  out_pose.head_half = QVector3D(dims.head_width * 0.45F,
                                 dims.head_height * 0.425F,
                                 dims.head_length * 0.375F);

  // --- Legs (motion-biased) ----------------------------------------
  QVector3D const front_anchor =
      QVector3D(0.0F, dims.body_height * 0.05F, dims.body_length * 0.30F);
  QVector3D const rear_anchor =
      QVector3D(0.0F, dims.body_height * 0.02F, -dims.body_length * 0.28F);

  float const front_bias = dims.body_length * 0.15F;
  float const rear_bias = -dims.body_length * 0.15F;

  auto fl = compute_reduced_leg(dims, gait, motion.phase, gait.front_leg_phase,
                                1.0F, front_bias, motion.is_moving,
                                front_anchor);
  auto fr = compute_reduced_leg(dims, gait, motion.phase,
                                gait.front_leg_phase + 0.48F, -1.0F,
                                front_bias, motion.is_moving, front_anchor);
  auto bl = compute_reduced_leg(dims, gait, motion.phase, gait.rear_leg_phase,
                                1.0F, rear_bias, motion.is_moving,
                                rear_anchor);
  auto br = compute_reduced_leg(dims, gait, motion.phase,
                                gait.rear_leg_phase + 0.52F, -1.0F, rear_bias,
                                motion.is_moving, rear_anchor);

  out_pose.shoulder_offset_reduced_fl = fl.shoulder;
  out_pose.shoulder_offset_reduced_fr = fr.shoulder;
  out_pose.shoulder_offset_reduced_bl = bl.shoulder;
  out_pose.shoulder_offset_reduced_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl.foot;
  out_pose.foot_br = center + br.foot;

  out_pose.leg_radius_reduced = dims.body_width * 0.22F;

  // --- Hooves -------------------------------------------------------
  // Legacy: hoof.scale(body_width*0.28, hoof_height, body_width*0.30)
  // on a unit cylinder (which has radius 1 and height 1 along Y).
  out_pose.hoof_scale =
      QVector3D(dims.body_width * 0.28F, dims.hoof_height,
                dims.body_width * 0.30F);
}

namespace {

// Build the Minimal LOD PartGraph into the caller-owned array.
// Returns the number of primitives written.
auto build_minimal_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 5> &out) noexcept
    -> std::size_t {
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.body";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = Render::GL::get_unit_sphere();
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Body);
    p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
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
    p.params.radius = pose.leg_radius;
    p.color_role = kRoleCoatDark;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 5;
}

// Build the Reduced LOD PartGraph. Layout:
//   [0]  body ellipsoid        (OrientedSphere @ Root)
//   [1]  neck cylinder         (Cylinder Root+neck_base → NeckTop)
//   [2]  head ellipsoid        (OrientedSphere @ Head)
//   [3]  leg FL                (Cylinder Root+shoulder → FootFL)
//   [4]  hoof FL               (Mesh unit_cylinder @ FootFL)
//   [5]  leg FR
//   [6]  hoof FR
//   [7]  leg BL
//   [8]  hoof BL
//   [9]  leg BR
//   [10] hoof BR
auto build_reduced_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 11> &out) noexcept
    -> std::size_t {
  // Body.
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
  // Neck.
  {
    PrimitiveInstance &p = out[1];
    p.debug_name = "horse.neck";
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset = pose.neck_base - pose.barrel_center;
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::NeckTop);
    p.params.radius = pose.neck_radius;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }
  // Head.
  {
    PrimitiveInstance &p = out[2];
    p.debug_name = "horse.head";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Head);
    p.params.half_extents = pose.head_half;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  struct RLeg {
    std::string_view leg_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<RLeg, 4> const legs{{
      {"horse.leg.fl.r", "horse.hoof.fl.r", HorseBone::FootFL,
       pose.shoulder_offset_reduced_fl},
      {"horse.leg.fr.r", "horse.hoof.fr.r", HorseBone::FootFR,
       pose.shoulder_offset_reduced_fr},
      {"horse.leg.bl.r", "horse.hoof.bl.r", HorseBone::FootBL,
       pose.shoulder_offset_reduced_bl},
      {"horse.leg.br.r", "horse.hoof.br.r", HorseBone::FootBR,
       pose.shoulder_offset_reduced_br},
  }};
  for (std::size_t i = 0; i < 4; ++i) {
    // Leg cylinder (Root + shoulder_offset_reduced) → Foot.
    PrimitiveInstance &leg = out[3 + (i * 2)];
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

    // Hoof Mesh(unit_cylinder) axis-aligned at foot.
    PrimitiveInstance &hoof = out[4 + (i * 2)];
    hoof.debug_name = legs[i].hoof_name;
    hoof.shape = PrimitiveShape::Mesh;
    hoof.custom_mesh = Render::GL::get_unit_cylinder();
    hoof.params.anchor_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    hoof.params.half_extents = pose.hoof_scale;
    hoof.color_role = kRoleHoof;
    hoof.material_id = 8;
    hoof.lod_mask = kLodReduced;
  }
  return 11;
}

} // namespace

void submit_horse_lod(const HorseSpecPose &pose,
                      const Render::GL::HorseVariant &variant,
                      CreatureLOD lod,
                      const QMatrix4x4 &world_from_unit,
                      Render::GL::ISubmitter &out) noexcept {
  BonePalette palette;
  evaluate_horse_skeleton(pose, palette);

  std::array<QVector3D, 4> role_colors{};
  fill_horse_role_colors(variant, role_colors);

  switch (lod) {
  case CreatureLOD::Minimal: {
    std::array<PrimitiveInstance, 5> prims{};
    std::size_t const n = build_minimal_primitives(pose, prims);
    PartGraph graph{std::span<const PrimitiveInstance>(prims.data(), n)};
    submit_part_graph(kHorseTopology, graph,
                      std::span<const QMatrix4x4>(palette), lod,
                      world_from_unit, out,
                      std::span<const QVector3D>(role_colors));
    break;
  }
  case CreatureLOD::Reduced: {
    std::array<PrimitiveInstance, 11> prims{};
    std::size_t const n = build_reduced_primitives(pose, prims);
    PartGraph graph{std::span<const PrimitiveInstance>(prims.data(), n)};
    submit_part_graph(kHorseTopology, graph,
                      std::span<const QMatrix4x4>(palette), lod,
                      world_from_unit, out,
                      std::span<const QVector3D>(role_colors));
    break;
  }
  case CreatureLOD::Full:
  {
    Render::Creature::submit_creature(
        horse_creature_spec(), std::span<const QMatrix4x4>(palette), lod,
        world_from_unit, out, std::span<const QVector3D>(role_colors));
    break;
  }
  case CreatureLOD::Billboard:
    break;
  }
}

} // namespace Render::Horse

// ---------------------------------------------------------------------
// Legacy rigged-mesh helpers retained for non-production tests.
// ---------------------------------------------------------------------

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Horse {

namespace {

// Pose used as the bake's bind reference. Built from
// `make_horse_dimensions(0)` so per-entity dimension drift collapses
// to a single baseline silhouette.
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

// ---------------- Static Reduced LOD PartGraph ----------------------
// 11 primitives mirroring `build_reduced_primitives` layout, but with
// half_extents / radii frozen from the baseline pose so the bake is
// keyable purely by (spec, lod).
auto build_static_reduced_parts() noexcept
    -> std::array<PrimitiveInstance, 11> {
  std::array<PrimitiveInstance, 11> out{};
  build_reduced_primitives(baseline_pose(), out);
  return out;
}

auto static_reduced_parts() noexcept
    -> const std::array<PrimitiveInstance, 11> & {
  static const auto parts = build_static_reduced_parts();
  return parts;
}

// ---------------- Static Full LOD PartGraph -------------------------
// 16 primitives forming a recognisable horse silhouette:
//   [0..5]  body shell (chest, withers, belly, rump, spine, sternum)
//   [6]     neck cylinder (Root + neck_base_offset → NeckTop)
//   [7]     head ellipsoid (OrientedSphere @ Head)
//   [8..11] leg cylinders (Root + shoulder → FootXX)
//   [12..15] hoof cylinders (Mesh unit_cylinder @ FootXX)
//
// All silhouette features beyond the shell — mane, forelock, tail,
// ears, eyes, blaze, fetlock joints, knee bumps, bridle/reins — are
// deferred to the v3-s15-5g-mount-fidelity follow-up. Equipment
// (saddle/armor/rider) continues to render as DrawPartCmd via the
// existing equipment dispatch.
auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, 16> {
  HorseSpecPose const &pose = baseline_pose();
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  std::array<PrimitiveInstance, 16> out{};

  auto ell = [](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                const QVector3D &half_extents,
                const QVector3D &offset_from_anchor) {
    p.debug_name = name;
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset_from_anchor;
    p.params.half_extents = half_extents;
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = Render::Creature::kLodFull;
  };

  // Body shell anchored at Root with offset_from_anchor =
  // (legacy_world_center − barrel_center). Under baseline these
  // offsets are constant; at runtime the per-entity Root bone moves
  // them with the unit.
  QVector3D const chest_off(0.0F, dims.body_height * 0.12F,
                            dims.body_length * 0.34F);
  QVector3D const withers_off =
      chest_off + QVector3D(0.0F, dims.body_height * 0.55F,
                            -dims.body_length * 0.03F);
  QVector3D const belly_off(0.0F, -dims.body_height * 0.35F,
                            -dims.body_length * 0.05F);
  QVector3D const rump_off(0.0F, dims.body_height * 0.08F,
                           -dims.body_length * 0.36F);
  QVector3D const wp = chest_off +
      QVector3D(0.0F, dims.body_height * 0.65F, -dims.body_length * 0.04F);
  QVector3D const cp = rump_off +
      QVector3D(0.0F, dims.body_height * 0.50F, -dims.body_length * 0.16F);
  QVector3D const spine_off = wp + (cp - wp) * 0.42F;
  QVector3D const sternum_off(0.0F, -dims.body_height * 0.42F,
                              dims.body_length * 0.30F);

  ell(out[0], "horse.full.chest", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.56F, dims.body_height * 0.475F,
                dims.body_length * 0.18F),
      chest_off);
  ell(out[1], "horse.full.withers", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.375F, dims.body_height * 0.175F,
                dims.body_length * 0.09F),
      withers_off);
  ell(out[2], "horse.full.belly", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.49F, dims.body_height * 0.32F,
                dims.body_length * 0.20F),
      belly_off);
  ell(out[3], "horse.full.rump", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.61F, dims.body_height * 0.525F,
                dims.body_length * 0.19F),
      rump_off);
  ell(out[4], "horse.full.spine", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.275F, dims.body_height * 0.08F,
                dims.body_length * 0.29F),
      spine_off);
  ell(out[5], "horse.full.sternum", static_cast<BoneIndex>(HorseBone::Root),
      QVector3D(dims.body_width * 0.275F, dims.body_height * 0.09F,
                dims.body_length * 0.07F),
      sternum_off);

  // Neck cylinder: Root + neck_base_offset → NeckTop.
  {
    PrimitiveInstance &p = out[6];
    p.debug_name = "horse.full.neck";
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset = pose.neck_base - pose.barrel_center;
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::NeckTop);
    p.params.radius = pose.neck_radius;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = Render::Creature::kLodFull;
  }

  // Head ellipsoid.
  {
    PrimitiveInstance &p = out[7];
    p.debug_name = "horse.full.head";
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Head);
    p.params.half_extents = pose.head_half;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = Render::Creature::kLodFull;
  }

  // Legs + hooves (4 each).
  struct Leg {
    std::string_view leg_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<Leg, 4> const legs{{
      {"horse.full.leg.fl", "horse.full.hoof.fl", HorseBone::FootFL,
       pose.shoulder_offset_reduced_fl},
      {"horse.full.leg.fr", "horse.full.hoof.fr", HorseBone::FootFR,
       pose.shoulder_offset_reduced_fr},
      {"horse.full.leg.bl", "horse.full.hoof.bl", HorseBone::FootBL,
       pose.shoulder_offset_reduced_bl},
      {"horse.full.leg.br", "horse.full.hoof.br", HorseBone::FootBR,
       pose.shoulder_offset_reduced_br},
  }};
  for (std::size_t i = 0; i < 4; ++i) {
    PrimitiveInstance &leg = out[8 + i];
    leg.debug_name = legs[i].leg_name;
    leg.shape = PrimitiveShape::Cylinder;
    leg.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    leg.params.head_offset = legs[i].shoulder_offset;
    leg.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    leg.params.radius = pose.leg_radius_reduced;
    leg.color_role = kRoleCoat;
    leg.material_id = 0;
    leg.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &hoof = out[12 + i];
    hoof.debug_name = legs[i].hoof_name;
    hoof.shape = PrimitiveShape::Mesh;
    hoof.custom_mesh = Render::GL::get_unit_cylinder();
    hoof.params.anchor_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    hoof.params.half_extents = pose.hoof_scale;
    hoof.color_role = kRoleHoof;
    hoof.material_id = 8;
    hoof.lod_mask = Render::Creature::kLodFull;
  }

  return out;
}

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, 16> & {
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
  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    return;
  }

  auto const &spec = horse_creature_spec();
  auto bind = horse_bind_palette();

  auto *entry = renderer->rigged_mesh_cache().get_or_bake(spec, lod, bind,
                                                          /*variant_bucket=*/0);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  auto &arena = renderer->bone_palette_arena();
  Render::GL::BonePaletteSlot palette_slot_h = arena.allocate_palette();
  QMatrix4x4 *palette_slot = palette_slot_h.cpu;

  std::array<QMatrix4x4, kHorseBoneCount> current{};
  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    current[i] = tmp[i];
  }

  std::size_t const n =
      std::min<std::size_t>(entry->inverse_bind.size(), kHorseBoneCount);
  for (std::size_t i = 0; i < n; ++i) {
    palette_slot[i] = current[i] * entry->inverse_bind[i];
  }

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.material = nullptr;
  cmd.world = world_from_unit;
  cmd.bone_palette = palette_slot;
  cmd.palette_ubo = palette_slot_h.ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(palette_slot_h.offset);
  cmd.bone_count = static_cast<std::uint32_t>(n);
  cmd.color = variant.coat_color;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;
  // Per-entity proportion drift collapses to baseline in the bake; a
  // future stage may bucket dominant axes (e.g. body_width) into
  // variant_bucket. variation_scale stays uniform here.
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
}

} // namespace

auto horse_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "horse";
    s.topology = horse_topology();
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
  submit_horse_rigged_impl(pose, variant,
                           Render::Creature::CreatureLOD::Full,
                           world_from_unit, out);
}

} // namespace Render::Horse
