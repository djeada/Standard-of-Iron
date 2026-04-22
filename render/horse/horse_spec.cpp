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

  out_pose.body_ellipsoid_x = dims.body_width * 1.30F;
  out_pose.body_ellipsoid_y =
      dims.body_height * 0.56F + dims.neck_rise * 0.06F;
  out_pose.body_ellipsoid_z =
      dims.body_length * 0.86F + dims.head_length * 0.14F;

  float const front_shoulder_dx = dims.body_width * 0.80F;
  float const rear_hip_dx = dims.body_width * 0.58F;
  float const front_shoulder_dy = -dims.body_height * 0.11F;
  float const rear_hip_dy = -dims.body_height * 0.19F;
  float const front_dz = dims.body_length * 0.34F;
  float const rear_dz = -dims.body_length * 0.25F;

  out_pose.shoulder_offset_fl =
      QVector3D(front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr =
      QVector3D(-front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl =
      QVector3D(rear_hip_dx, rear_hip_dy, rear_dz);
  out_pose.shoulder_offset_br =
      QVector3D(-rear_hip_dx, rear_hip_dy, rear_dz);

  float const front_drop = -dims.leg_length * 0.86F;
  float const rear_drop = -dims.leg_length * 0.82F;
  out_pose.foot_fl = center + out_pose.shoulder_offset_fl +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.04F);
  out_pose.foot_fr = center + out_pose.shoulder_offset_fr +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.04F);
  out_pose.foot_bl = center + out_pose.shoulder_offset_bl +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.08F);
  out_pose.foot_br = center + out_pose.shoulder_offset_br +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.08F);

  out_pose.leg_radius = dims.body_width * 0.076F;
}

namespace {

struct ReducedLegSample {
  QVector3D shoulder;
  QVector3D foot;
};

auto compute_reduced_leg(
    const Render::GL::HorseDimensions &dims, const Render::GL::HorseGait &gait,
    float phase_base, float phase_offset, float lateral_sign, bool is_rear,
    float forward_bias, bool is_moving,
    const QVector3D &anchor_offset) noexcept -> ReducedLegSample {
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
                                      ? 0.60F
                                      : (gait.cycle_time > 0.5F ? 0.44F : 0.34F);
    float const stride_extent = gait.stride_swing * 0.56F;
    if (leg_phase < stance_fraction) {
      float const t = ease_in_out(leg_phase / stance_fraction);
      stride = forward_bias + stride_extent * (0.40F - 1.00F * t);
      shoulder_compress =
          std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.08F;
    } else {
      float const t = ease_in_out((leg_phase - stance_fraction) /
                                  (1.0F - stance_fraction));
      stride = forward_bias - stride_extent * 0.60F + stride_extent * 1.00F * t;
      lift = std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.84F;
    }
  }

  float const shoulder_out =
      dims.body_width * (is_rear ? 0.64F : 0.72F);
  float const vertical_bias =
      -dims.body_height * (is_rear ? 0.08F : 0.03F);
  float const longitudinal_bias =
      dims.body_length * (is_rear ? -0.03F : 0.02F);
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out,
                vertical_bias + lift * 0.05F - shoulder_compress,
                stride + longitudinal_bias);

  float const leg_length = dims.leg_length * (is_rear ? 0.93F : 0.98F);
  QVector3D const foot =
      shoulder +
      QVector3D(0.0F, -leg_length + lift,
                dims.body_length * (is_rear ? -0.02F : 0.01F));

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept {

  make_horse_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  // x: lateral barrel semi-extent — close to the full body half-width so the
  //    barrel box/capsule actually spans the horse width rather than being a
  //    narrow central stripe.
  // y: used as a scale reference for vertical offsets.
  // z: used as a scale reference for fore-aft offsets.
  out_pose.reduced_body_half =
      QVector3D(dims.body_width * 0.88F, dims.body_height * 0.52F,
                dims.body_length * 0.38F);

  // Raise the neck base higher on the chest and steepen the neck angle so
  // it arches forward rather than lying nearly flat.
  float const head_height_scale = 1.0F + gait.head_height_jitter;
  out_pose.neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.72F * head_height_scale,
                         dims.body_length * 0.26F);
  out_pose.neck_top =
      out_pose.neck_base +
      QVector3D(0.0F, dims.neck_rise * 1.60F * head_height_scale,
                dims.neck_length * 0.78F);
  // Thicker neck reads as more substantial from the game camera.
  out_pose.neck_radius = dims.body_width * 0.28F;

  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(0.0F, dims.head_height * 0.10F, dims.head_length * 0.52F);
  out_pose.head_half =
      QVector3D(dims.head_width * 0.28F, dims.head_height * 0.26F,
                dims.head_length * 0.48F);

  QVector3D const front_anchor =
      QVector3D(0.0F, dims.body_height * 0.18F, dims.body_length * 0.28F);
  QVector3D const rear_anchor =
      QVector3D(0.0F, dims.body_height * 0.16F, -dims.body_length * 0.24F);

  float const front_bias = dims.body_length * 0.10F;
  float const rear_bias = -dims.body_length * 0.14F;

  Render::GL::HorseGait jittered = gait;
  jittered.stride_swing *= (1.0F + gait.stride_jitter);
  float const front_lat =
      gait.lateral_lead_front == 0.0F ? 0.44F : gait.lateral_lead_front;
  float const rear_lat =
      gait.lateral_lead_rear == 0.0F ? 0.50F : gait.lateral_lead_rear;

  auto fl = compute_reduced_leg(
      dims, jittered, motion.phase, gait.front_leg_phase, 1.0F, false,
      front_bias,
      motion.is_moving,
      front_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));
  auto fr = compute_reduced_leg(
      dims, jittered, motion.phase, gait.front_leg_phase + front_lat, -1.0F,
      false,
      front_bias, motion.is_moving,
      front_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto bl = compute_reduced_leg(
      dims, jittered, motion.phase, gait.rear_leg_phase, 1.0F, true,
      rear_bias, motion.is_moving,
      rear_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto br = compute_reduced_leg(
      dims, jittered, motion.phase, gait.rear_leg_phase + rear_lat, -1.0F,
      true,
      rear_bias, motion.is_moving,
      rear_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));

  out_pose.shoulder_offset_reduced_fl = fl.shoulder;
  out_pose.shoulder_offset_reduced_fr = fr.shoulder;
  out_pose.shoulder_offset_reduced_bl = bl.shoulder;
  out_pose.shoulder_offset_reduced_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl.foot;
  out_pose.foot_br = center + br.foot;

  // Larger base so the forearm / gaskin reads as muscular at game distance.
  // The cannon is scaled down 3×, so the taper is dramatic and clearly visible.
  out_pose.leg_radius_reduced = dims.body_width * 0.30F;

  out_pose.hoof_scale = QVector3D(dims.body_width * 0.20F,
                                  dims.hoof_height * 0.74F,
                                  dims.body_width * 0.30F);
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
    p.params.half_extents = QVector3D(0.82F, 1.08F, 0.92F);
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
    p.color_role = kRoleCoatReducedLeg;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 5;
}

constexpr std::size_t kHorseReducedPartCount = 17;

auto build_reduced_primitives(
    const HorseSpecPose &pose, const Render::GL::HorseDimensions &dims,
    std::array<PrimitiveInstance, kHorseReducedPartCount> &out) noexcept
    -> std::size_t {

  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const bw = dims.body_width;

  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.body.reduced";
    // Long, slender horizontal capsule forming the barrel — the head (chest
    // end) and tail (rump end) are well separated in Z so the silhouette is
    // clearly an elongated tube and not a sphere.
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, bh * 0.02F, bl * 0.34F);
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.tail_offset =
        QVector3D(0.0F, -bh * 0.02F, -bl * 0.32F);
    // Cross-section keyed off body_width (lateral) — keeps the barrel
    // proportional to the horse's actual width, not its height.
    p.params.radius = bw * 0.78F;
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[1];
    p.debug_name = "horse.chest.reduced";
    // Short forward-and-down extension giving the chest a projecting prow;
    // capsule lies along Z (mostly horizontal) instead of standing vertically.
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, -bh * 0.06F, bl * 0.50F);  // forward sternum
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.tail_offset =
        QVector3D(0.0F, bh * 0.04F, bl * 0.30F);   // blends back into barrel
    p.params.radius = bw * 0.62F;
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[2];
    p.debug_name = "horse.rump.reduced";
    // Short rearward extension forming the croup; rises higher than the
    // barrel topline so the hindquarters read as the highest point of the
    // back, then tapers down to the tailhead.
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        QVector3D(0.0F, bh * 0.06F, -bl * 0.50F);  // tailhead (rear apex)
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.tail_offset =
        QVector3D(0.0F, -bh * 0.04F, -bl * 0.30F); // joins barrel
    p.params.radius = bw * 0.74F;  // hindquarters bulkier than chest at base
    p.color_role = kRoleCoatDark;
    p.material_id = 6;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[3];
    p.debug_name = "horse.neck_head.reduced";
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset =
        pose.neck_base - pose.barrel_center +
        QVector3D(0.0F, -pose.reduced_body_half.y() * 0.08F,
                  -pose.reduced_body_half.z() * 0.03F);
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Head);
    p.params.tail_offset =
        QVector3D(0.0F, -pose.head_half.y() * 0.06F,
                  pose.head_half.z() * 0.68F);
    p.params.radius = pose.neck_radius * 0.52F;
    p.color_role = kRoleCoat;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  {
    PrimitiveInstance &p = out[4];
    p.debug_name = "horse.tail.reduced";
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    // Tail attaches at the top of the rump and hangs backward-down.
    p.params.head_offset =
        QVector3D(0.0F, bh * 0.16F, -bl * 0.44F);  // dock (rump attachment)
    p.params.tail_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.tail_offset =
        QVector3D(0.0F, -bh * 0.36F, -bl * 0.52F); // switch (tip, hangs down)
    p.params.radius = pose.leg_radius_reduced * 0.28F;
    p.color_role = kRoleTail;
    p.material_id = 0;
    p.lod_mask = kLodReduced;
  }

  struct RLeg {
    std::string_view upper_name;
    std::string_view lower_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
    bool is_rear;
  };
  std::array<RLeg, 4> const legs{{
      {"horse.leg.fl.upper.r", "horse.leg.fl.lower.r", "horse.hoof.fl.r",
       HorseBone::FootFL, pose.shoulder_offset_reduced_fl, false},
      {"horse.leg.fr.upper.r", "horse.leg.fr.lower.r", "horse.hoof.fr.r",
       HorseBone::FootFR, pose.shoulder_offset_reduced_fr, false},
      {"horse.leg.bl.upper.r", "horse.leg.bl.lower.r", "horse.hoof.bl.r",
       HorseBone::FootBL, pose.shoulder_offset_reduced_bl, true},
      {"horse.leg.br.upper.r", "horse.leg.br.lower.r", "horse.hoof.br.r",
       HorseBone::FootBR, pose.shoulder_offset_reduced_br, true},
  }};
  for (std::size_t i = 0; i < 4; ++i) {
    BoneIndex const foot_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    // Heavy upper arm / gaskin tapering to a slender cannon; the ratio is
    // more extreme than before so it reads as muscular even at reduced LOD.
    float const upper_radius =
        pose.leg_radius_reduced * (legs[i].is_rear ? 1.42F : 1.68F);
    float const lower_radius =
        pose.leg_radius_reduced * (legs[i].is_rear ? 0.38F : 0.34F);
    // Joint (knee / hock) sits at ~34 % of leg_length above the foot for
    // fronts (long forearm, short cannon) and ~28 % for rears.  The rear
    // hock is angled backward in Z, creating the characteristic Z-profile.
    QVector3D const upper_tail =
        legs[i].is_rear
            ? QVector3D(0.0F, dims.leg_length * 0.28F,
                        -pose.reduced_body_half.z() * 0.12F)
            : QVector3D(0.0F, dims.leg_length * 0.34F,
                        pose.reduced_body_half.z() * 0.05F);
    QVector3D const lower_head =
        legs[i].is_rear
            ? QVector3D(0.0F, dims.leg_length * 0.28F,
                        -pose.reduced_body_half.z() * 0.10F)
            : QVector3D(0.0F, dims.leg_length * 0.34F,
                        pose.reduced_body_half.z() * 0.04F);
    QVector3D const lower_tail =
        QVector3D(0.0F, pose.hoof_scale.y() * 0.80F,
                  legs[i].is_rear ? -pose.reduced_body_half.z() * 0.02F
                                  : pose.reduced_body_half.z() * 0.02F);

    PrimitiveInstance &upper = out[5 + (i * 3)];
    upper.debug_name = legs[i].upper_name;
    upper.shape = PrimitiveShape::Capsule;
    upper.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    upper.params.head_offset = legs[i].shoulder_offset;
    upper.params.tail_bone = foot_bone;
    upper.params.tail_offset = upper_tail;
    upper.params.radius = upper_radius;
    upper.color_role = kRoleCoatReducedLeg;
    upper.material_id = 0;
    upper.lod_mask = kLodReduced;

    PrimitiveInstance &lower = out[6 + (i * 3)];
    lower.debug_name = legs[i].lower_name;
    lower.shape = PrimitiveShape::Cylinder;
    lower.params.anchor_bone = foot_bone;
    lower.params.head_offset = lower_head;
    lower.params.tail_bone = foot_bone;
    lower.params.tail_offset = lower_tail;
    lower.params.radius = lower_radius;
    lower.color_role = kRoleCoatDark;
    lower.material_id = 0;
    lower.lod_mask = kLodReduced;

    PrimitiveInstance &hoof = out[7 + (i * 3)];
    hoof.debug_name = legs[i].hoof_name;
    hoof.shape = PrimitiveShape::Mesh;
    hoof.custom_mesh = Render::GL::get_unit_cylinder();
    hoof.params.anchor_bone = foot_bone;
    hoof.params.half_extents =
        QVector3D(pose.hoof_scale.x() * 0.90F, pose.hoof_scale.y() * 0.90F,
                  pose.hoof_scale.z() * 0.88F);
    hoof.color_role = kRoleHoof;
    hoof.material_id = 8;
    hoof.lod_mask = kLodReduced;
  }

  return kHorseReducedPartCount;
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
    -> std::array<PrimitiveInstance, kHorseReducedPartCount> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  std::array<PrimitiveInstance, kHorseReducedPartCount> out{};
  build_reduced_primitives(baseline_pose(), dims, out);
  return out;
}

auto static_reduced_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseReducedPartCount> & {
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

  auto box = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role = kRoleCoat, int material_id = 6) {
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
  // Convenience aliases for neck/mane/tail sections that use a slightly
  // reduced scale to keep those secondary shapes from overwhelming the barrel.
  float const bw_v = bw * 0.96F;
  float const bh_v = bh * 0.86F;

  std::size_t i = 0;

  // ---- Body / barrel (10 primitives) ----------------------------------
  // Redesigned from the ground up for anatomical realism:
  //
  //  • Ribcage: OrientedSphere (ellipsoid) so the barrel reads as rounded.
  //    X (lateral) is now ~72 % of body_width — wide enough to contain the
  //    shoulder ellipsoids and match the real heartgirth roundness.
  //  • Withers: tall, narrow spike.  Half-height = 40 % of body_height so
  //    they project clearly above the back and croup — the classic high-
  //    point silhouette of a horse.
  //  • Hindquarters: dominant ellipsoids.  Half-height = 48 % of bh so
  //    the gluteal mass is unmistakable from side and three-quarter views.
  //  • Croup: wide and elevated (Y-offset = 46 % bh); lower than withers.
  //  • Belly: wide ellipsoid hanging well below centre (Y = -32 % bh).
  //  • Chest: tall box projecting forward to bl * 0.46 — clear prow.
  //  • Shoulder masses: tall (40 % bh) so the scapula reads as a distinct
  //    sloping slab rather than a shallow button.

  // Ribcage — the dominant barrel mass.
  ell(out[i++], "horse.full.ribcage", root,
      QVector3D(bw * 0.72F, bh * 0.54F, bl * 0.30F),
      QVector3D(0.0F, bh * 0.06F, 0.0F));
  // Withers — bony spine processes: a narrow spike that is the topline high
  // point, clearly above the croup.
  box(out[i++], "horse.full.withers", root,
      QVector3D(bw * 0.07F, bh * 0.40F, bl * 0.13F),
      QVector3D(0.0F, bh * 0.84F, bl * 0.16F));
  // Belly — hangs low as an abdominal mass separate from the ribcage.
  ell(out[i++], "horse.full.belly", root,
      QVector3D(bw * 0.64F, bh * 0.20F, bl * 0.28F),
      QVector3D(0.0F, -bh * 0.32F, -bl * 0.05F));
  // Chest / pectoral — tall box that projects forward to form the forehorse
  // prow; noticeably narrower than the ribcage so the front tapers.
  box(out[i++], "horse.full.chest", root,
      QVector3D(bw * 0.36F, bh * 0.44F, bl * 0.09F),
      QVector3D(0.0F, bh * 0.04F, bl * 0.46F));
  // Sternum — lower chest point well below the chest centre.
  ell(out[i++], "horse.full.sternum", root,
      QVector3D(bw * 0.22F, bh * 0.10F, bl * 0.09F),
      QVector3D(0.0F, -bh * 0.38F, bl * 0.40F));
  // Croup — wide, elevated, clearly below the withers.
  box(out[i++], "horse.full.croup", root,
      QVector3D(bw * 0.54F, bh * 0.26F, bl * 0.18F),
      QVector3D(0.0F, bh * 0.46F, -bl * 0.21F));
  // Hindquarters L / R — the dominant gluteal mass; large enough to define
  // the rump silhouette even from the side.
  ell(out[i++], "horse.full.hq.l", root,
      QVector3D(bw * 0.20F, bh * 0.48F, bl * 0.24F),
      QVector3D(bw * 0.24F, bh * 0.10F, -bl * 0.24F));
  ell(out[i++], "horse.full.hq.r", root,
      QVector3D(bw * 0.20F, bh * 0.48F, bl * 0.24F),
      QVector3D(-bw * 0.24F, bh * 0.10F, -bl * 0.24F));
  // Shoulder blades L / R — tall sloping ellipsoids representing the
  // scapula; height 40 % of bh so they read as a distinct mass.
  ell(out[i++], "horse.full.shoulder.l", root,
      QVector3D(bw * 0.10F, bh * 0.40F, bl * 0.16F),
      QVector3D(bw * 0.22F, bh * 0.16F, bl * 0.28F));
  ell(out[i++], "horse.full.shoulder.r", root,
      QVector3D(bw * 0.10F, bh * 0.40F, bl * 0.16F),
      QVector3D(-bw * 0.22F, bh * 0.16F, bl * 0.28F));

  // ---- Neck + mane (8 primitives) ------------------------------------
  // Neck crest: thicker at the base, then tapering toward the poll.
  cap(out[i++], "horse.full.neck.crest", root,
      pose.neck_base - pose.barrel_center, neck_top_bone,
      QVector3D(0.0F, 0.0F, 0.0F), pose.neck_radius * 1.08F, kRoleCoat);
  // Throatlatch adds underside mass near the base while keeping the head end
  // visibly narrower.
  ell(out[i++], "horse.full.neck.throat", root,
      QVector3D(bw_v * 0.10F, bh_v * 0.11F, bl * 0.11F),
      (pose.neck_base - pose.barrel_center) +
          QVector3D(0.0F, -bh_v * 0.12F, bl * 0.06F));
  // Mane: a narrow ridge that falls slightly to one side instead of ballooning
  // the whole neck.
  for (int m = 0; m < 5; ++m) {
    float const t = static_cast<float>(m) / 4.0F;
    QVector3D const along =
        (pose.neck_base - pose.barrel_center) * (1.0F - t) +
        (pose.neck_top - pose.barrel_center) * t;
    float const size = 1.0F - 0.16F * t;
    ell(out[i++],
        m == 0   ? "horse.full.mane.0"
        : m == 1 ? "horse.full.mane.1"
        : m == 2 ? "horse.full.mane.2"
        : m == 3 ? "horse.full.mane.3"
                 : "horse.full.mane.4",
        root,
        QVector3D(bw_v * 0.04F * size, bh_v * 0.06F * size,
                  bl * 0.045F * size),
        along + QVector3D(bw_v * 0.025F, bh_v * (0.17F - 0.06F * t),
                          -bl * 0.02F * t),
        kRoleMane, 0);
  }
  // Forelock: small tuft between the ears.
  ell(out[i++], "horse.full.forelock", head_bone,
      QVector3D(dims.head_width * 0.14F, dims.head_height * 0.08F,
                hl * 0.07F),
      QVector3D(0.0F, dims.head_height * 0.25F, -hl * 0.20F), kRoleMane, 0);

  // ---- Head (9 primitives) -------------------------------------------
  // Cranium: lean and slightly long, leaving room for a narrower muzzle.
  ell(out[i++], "horse.full.head.cranium", head_bone,
      QVector3D(dims.head_width * 0.30F, dims.head_height * 0.30F,
                hl * 0.36F),
      QVector3D(0.0F, dims.head_height * 0.08F, -hl * 0.16F));
  // Muzzle stays clearly narrower than the cheek/jaw plane.
  cap(out[i++], "horse.full.head.muzzle", head_bone,
      QVector3D(0.0F, -dims.head_height * 0.04F, hl * 0.04F), head_bone,
      QVector3D(0.0F, -dims.head_height * 0.15F, hl * 0.60F),
      dims.head_width * 0.13F, kRoleMuzzle);
  // Lower jaw defines the wedge transition from broad cheek to narrow muzzle.
  ell(out[i++], "horse.full.head.jaw", head_bone,
      QVector3D(dims.head_width * 0.17F, dims.head_height * 0.09F,
                hl * 0.24F),
      QVector3D(0.0F, -dims.head_height * 0.20F, hl * 0.14F), kRoleMuzzle);
  // Cheeks stay fuller than the muzzle, but no longer turn the whole head
  // into a rounded block.
  sph(out[i++], "horse.full.head.cheek.l", head_bone,
      QVector3D(dims.head_width * 0.22F, -dims.head_height * 0.02F,
                -hl * 0.01F),
      dims.head_width * 0.17F, kRoleCoat);
  sph(out[i++], "horse.full.head.cheek.r", head_bone,
      QVector3D(-dims.head_width * 0.22F, -dims.head_height * 0.02F,
                -hl * 0.01F),
      dims.head_width * 0.17F, kRoleCoat);
  // Ears L/R: smaller and slightly set back.
  cone_p(out[i++], "horse.full.head.ear.l", head_bone,
         QVector3D(dims.head_width * 0.15F, dims.head_height * 0.27F,
                   -hl * 0.24F),
         head_bone,
         QVector3D(dims.head_width * 0.16F, dims.head_height * 0.54F,
                   -hl * 0.33F),
         dims.head_width * 0.07F, kRoleCoat);
  cone_p(out[i++], "horse.full.head.ear.r", head_bone,
         QVector3D(-dims.head_width * 0.15F, dims.head_height * 0.27F,
                   -hl * 0.24F),
         head_bone,
         QVector3D(-dims.head_width * 0.16F, dims.head_height * 0.54F,
                   -hl * 0.33F),
         dims.head_width * 0.07F, kRoleCoat);
  // Eyes L/R: slightly larger and farther back, which reads better from the
  // game camera and matches the longer cranium.
  sph(out[i++], "horse.full.head.eye.l", head_bone,
      QVector3D(dims.head_width * 0.28F, dims.head_height * 0.15F,
                -hl * 0.06F),
      dims.head_width * 0.08F, kRoleEye, 0);
  sph(out[i++], "horse.full.head.eye.r", head_bone,
      QVector3D(-dims.head_width * 0.28F, dims.head_height * 0.15F,
                -hl * 0.06F),
      dims.head_width * 0.08F, kRoleEye, 0);

  // ---- Tail (4 primitives) -------------------------------------------
  // Tail dock anchored high on the croup.
  QVector3D const tail_root_off(0.0F, bh_v * 0.28F, -bl * 0.46F);
  ell(out[i++], "horse.full.tail.dock", root,
      QVector3D(bw_v * 0.05F, bh_v * 0.06F, bl * 0.07F), tail_root_off,
      kRoleCoat);
  // Tail switch: longer, narrower masses so the tail falls cleanly instead of
  // reading like a rounded stump.
  for (int t_i = 0; t_i < 3; ++t_i) {
    float const tt = static_cast<float>(t_i + 1) / 3.0F;
    QVector3D const off = tail_root_off +
                          QVector3D(0.0F, -bh_v * 0.22F * tt - bh_v * 0.04F,
                                    -bl * 0.08F * tt - bl * 0.02F);
    ell(out[i++],
        t_i == 0   ? "horse.full.tail.switch.0"
        : t_i == 1 ? "horse.full.tail.switch.1"
                   : "horse.full.tail.switch.2",
        root,
        QVector3D(bw_v * (0.08F - 0.02F * tt),
                  bh_v * (0.14F + 0.04F * tt), bl * 0.08F),
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
  for (auto const &leg : legs) {
    auto foot_b = static_cast<BoneIndex>(leg.foot_bone);
    // Dramatic radius taper — upper arm (forearm/gaskin) is visibly muscular
    // while the cannon is a slender column.
    float const upper_r =
        pose.leg_radius_reduced * (leg.is_rear ? 1.58F : 1.88F);
    float const cannon_r =
        pose.leg_radius_reduced * (leg.is_rear ? 0.46F : 0.44F);
    // Joint sits at 28-34 % of leg_length above the foot — long forearm /
    // gaskin, short cannon.  The rear hock is angled backward to create the
    // characteristic Z-profile of the hind leg.
    QVector3D const upper_tail =
        leg.is_rear ? QVector3D(0.0F, leg_len * 0.28F, -bl * 0.10F)
                    : QVector3D(0.0F, leg_len * 0.34F, bl * 0.03F);
    QVector3D const cannon_head =
        leg.is_rear ? QVector3D(0.0F, leg_len * 0.28F, -bl * 0.08F)
                    : QVector3D(0.0F, leg_len * 0.34F, bl * 0.02F);
    QVector3D const cannon_tail =
        QVector3D(0.0F, dims.hoof_height * 0.64F,
                  leg.is_rear ? -bl * 0.02F : bl * 0.02F);
    // Upper leg: from shoulder/hindquarter down to ~mid-leg height,
    // tapered via a Capsule (rounded ends look like muscle).
    cap(out[i++], leg.upper_name, root, leg.shoulder_offset, foot_b, upper_tail,
        upper_r, kRoleCoat);
    // Cannon bone + pastern: thinner cylinder rigid to the foot bone,
    // from knee height down to just above the hoof.
    cyl(out[i++], leg.cannon_name, foot_b, cannon_head, foot_b, cannon_tail,
        cannon_r, kRoleCoatDark);
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
