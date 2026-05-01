#include "horse_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../submitter.h"
#include "horse_layout.h"
#include "horse_manifest.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

namespace Render::Horse {

namespace {

using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::Creature::SkeletonTopology;

constexpr std::array<BoneDef, k_horse_bone_count> k_horse_bones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", static_cast<BoneIndex>(HorseBone::Root)},
    {"ShoulderFL", static_cast<BoneIndex>(HorseBone::Body)},
    {"KneeFL", static_cast<BoneIndex>(HorseBone::ShoulderFL)},
    {"FootFL", static_cast<BoneIndex>(HorseBone::KneeFL)},
    {"ShoulderFR", static_cast<BoneIndex>(HorseBone::Body)},
    {"KneeFR", static_cast<BoneIndex>(HorseBone::ShoulderFR)},
    {"FootFR", static_cast<BoneIndex>(HorseBone::KneeFR)},
    {"ShoulderBL", static_cast<BoneIndex>(HorseBone::Body)},
    {"KneeBL", static_cast<BoneIndex>(HorseBone::ShoulderBL)},
    {"FootBL", static_cast<BoneIndex>(HorseBone::KneeBL)},
    {"ShoulderBR", static_cast<BoneIndex>(HorseBone::Body)},
    {"KneeBR", static_cast<BoneIndex>(HorseBone::ShoulderBR)},
    {"FootBR", static_cast<BoneIndex>(HorseBone::KneeBR)},
    {"NeckTop", static_cast<BoneIndex>(HorseBone::Body)},
    {"Head", static_cast<BoneIndex>(HorseBone::NeckTop)},
}};

constexpr std::array<Render::Creature::SocketDef, 0> k_horse_sockets{};
constexpr std::uint8_t k_role_coat = 1;
constexpr std::uint8_t k_role_hoof = 4;
constexpr int k_horse_material_id = 6;

constexpr SkeletonTopology k_horse_topology{
    std::span<const BoneDef>(k_horse_bones),
    std::span<const Render::Creature::SocketDef>(k_horse_sockets),
};

[[nodiscard]] auto
translation_matrix(const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setToIdentity();
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

[[nodiscard]] auto
solve_bent_leg_joint(const QVector3D &shoulder, const QVector3D &foot,
                     float upper_len, float lower_len,
                     const QVector3D &bend_hint) noexcept -> QVector3D {
  QVector3D hip_to_foot = foot - shoulder;
  float const dist_sq = QVector3D::dotProduct(hip_to_foot, hip_to_foot);
  if (dist_sq <= 1.0e-6F) {
    return shoulder + QVector3D(0.0F, -upper_len, 0.0F);
  }

  float const total_len = std::max(upper_len + lower_len, 1.0e-4F);
  float const min_len =
      std::max(std::abs(upper_len - lower_len) + 1.0e-4F, total_len * 0.10F);
  float const dist =
      std::clamp(std::sqrt(dist_sq), min_len, total_len - 1.0e-4F);
  QVector3D const dir = hip_to_foot / std::sqrt(dist_sq);

  QVector3D bend_axis = bend_hint - dir * QVector3D::dotProduct(bend_hint, dir);
  if (bend_axis.lengthSquared() <= 1.0e-6F) {
    bend_axis = QVector3D(0.0F, 0.0F, 1.0F) -
                dir * QVector3D::dotProduct(QVector3D(0.0F, 0.0F, 1.0F), dir);
  }
  if (bend_axis.lengthSquared() <= 1.0e-6F) {
    bend_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  bend_axis.normalize();

  float const along =
      (upper_len * upper_len - lower_len * lower_len + dist * dist) /
      (2.0F * dist);
  float const height =
      std::sqrt(std::max(upper_len * upper_len - along * along, 0.0F));
  return shoulder + dir * along + bend_axis * height;
}

struct horse_pose_profile {
  QVector3D body_half_scale{0.72F, 0.70F, 0.64F};
  QVector3D neck_base_scale{0.0F, 0.36F, 0.20F};
  float neck_rise_scale{2.85F};
  float neck_length_scale{2.03F};
  float neck_radius_scale{0.23F};
  QVector3D head_center_scale{0.0F, 0.08F, 0.30F};
  QVector3D head_half_scale{0.34F, 0.26F, 0.48F};
  QVector3D front_anchor_scale{0.0F, 0.18F, 0.46F};
  QVector3D rear_anchor_scale{0.0F, 0.16F, -0.18F};
  float front_bias_scale{0.10F};
  float rear_bias_scale{-0.15F};
  float front_shoulder_out_scale{0.42F};
  float rear_shoulder_out_scale{0.92F};
  float front_vertical_bias_scale{-0.04F};
  float rear_vertical_bias_scale{-0.06F};
  float front_longitudinal_bias_scale{0.14F};
  float rear_longitudinal_bias_scale{0.00F};
  float front_leg_length_scale{1.10F};
  float rear_leg_length_scale{1.13F};
  float leg_radius_scale{0.38F};
  float front_upper_radius_scale{2.05F};
  float rear_upper_radius_scale{2.25F};
  float front_lower_radius_scale{0.92F};
  float rear_lower_radius_scale{0.90F};
  float front_upper_joint_height_scale{0.66F};
  float rear_upper_joint_height_scale{0.60F};
  float front_joint_z_scale{0.12F};
  float rear_joint_z_scale{-0.48F};

  QVector3D body_offset_scale{0.0F, 0.03F, -0.02F};
  QVector3D body_half_extents_scale{0.50F, 0.22F, 0.44F};
  QVector3D chest_offset_scale{0.0F, 0.03F, 0.50F};
  QVector3D chest_half_extents_scale{0.36F, 0.32F, 0.14F};
  QVector3D belly_offset_scale{0.0F, -0.18F, -0.04F};
  QVector3D belly_half_extents_scale{0.36F, 0.08F, 0.34F};
  QVector3D rump_offset_scale{0.0F, 0.07F, -0.42F};
  QVector3D rump_half_extents_scale{0.46F, 0.28F, 0.24F};

  QVector3D withers_offset_scale{0.0F, 0.58F, 0.18F};
  QVector3D withers_tail_offset_scale{0.0F, 0.34F, 0.18F};
  float withers_radius_scale{0.12F};
  QVector3D cranium_offset_scale{0.0F, 0.05F, -0.10F};
  QVector3D cranium_half_extents_scale{0.24F, 0.18F, 0.33F};
  QVector3D muzzle_head_scale{0.0F, -0.03F, 0.04F};
  QVector3D muzzle_tail_scale{0.0F, -0.20F, 0.88F};
  float muzzle_radius_scale{0.09F};
  QVector3D jaw_offset_scale{0.0F, -0.16F, 0.15F};
  QVector3D jaw_half_extents_scale{0.08F, 0.04F, 0.17F};
  QVector3D ear_head_scale{0.10F, 0.12F, -0.28F};
  QVector3D ear_tail_scale{0.11F, 0.34F, -0.36F};
  float ear_radius_scale{0.05F};
  QVector3D tail_head_scale{0.0F, 0.16F, -0.42F};
  QVector3D tail_tail_scale{0.0F, -0.36F, -0.56F};
  float tail_radius_scale{0.20F};
  QVector3D hoof_scale{0.30F, 0.88F, 0.50F};
};

[[nodiscard]] auto make_horse_pose_profile() noexcept -> horse_pose_profile {
  return {};
}

} // namespace

auto horse_topology() noexcept -> const SkeletonTopology & {
  return k_horse_topology;
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

  out_palette[static_cast<std::size_t>(HorseBone::ShoulderFL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fl);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderFR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fr);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderBL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_bl);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderBR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_br);

  out_palette[static_cast<std::size_t>(HorseBone::KneeFL)] =
      translation_matrix(pose.knee_fl);
  out_palette[static_cast<std::size_t>(HorseBone::KneeFR)] =
      translation_matrix(pose.knee_fr);
  out_palette[static_cast<std::size_t>(HorseBone::KneeBL)] =
      translation_matrix(pose.knee_bl);
  out_palette[static_cast<std::size_t>(HorseBone::KneeBR)] =
      translation_matrix(pose.knee_br);

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
  out_roles[1] = coat * 0.62F + QVector3D(0.080F, 0.068F, 0.050F);
  out_roles[2] = coat * 0.70F + QVector3D(0.060F, 0.050F, 0.040F);
  out_roles[3] = variant.hoof_color;
  out_roles[4] = variant.mane_color;
  out_roles[5] = variant.tail_color;
  out_roles[6] = variant.muzzle_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
}

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  float const body_width_vis = horse_body_visual_width(dims);
  float const body_height_vis = horse_body_visual_height(dims);
  float const body_length_vis = horse_body_visual_length(dims);
  float const head_width_vis = horse_head_visual_width(dims);
  float const head_height_vis = horse_head_visual_height(dims);
  float const head_length_vis = horse_head_visual_length(dims);
  QVector3D const front_attach = horse_front_leg_attach_local(dims);
  QVector3D const rear_attach = horse_rear_leg_attach_local(dims);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = body_width_vis * 0.76F;
  out_pose.body_ellipsoid_y =
      body_height_vis * 0.86F + dims.neck_rise * k_horse_visual_scale * 0.10F;
  out_pose.body_ellipsoid_z = body_length_vis * 0.88F + head_length_vis * 0.10F;

  float const front_shoulder_dx = body_width_vis * 0.36F;
  float const rear_hip_dx = body_width_vis * 0.50F;

  out_pose.shoulder_offset_fl =
      QVector3D(front_shoulder_dx, front_attach.y(), front_attach.z());
  out_pose.shoulder_offset_fr =
      QVector3D(-front_shoulder_dx, front_attach.y(), front_attach.z());
  out_pose.shoulder_offset_bl =
      QVector3D(rear_hip_dx, rear_attach.y(), rear_attach.z());
  out_pose.shoulder_offset_br =
      QVector3D(-rear_hip_dx, rear_attach.y(), rear_attach.z());

  float const front_drop = -dims.leg_length * 0.96F;
  float const rear_drop = -dims.leg_length * 1.00F;
  out_pose.foot_fl = center + out_pose.shoulder_offset_fl +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.16F);
  out_pose.foot_fr = center + out_pose.shoulder_offset_fr +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.16F);
  out_pose.foot_bl = center + out_pose.shoulder_offset_bl +
                     QVector3D(0.0F, rear_drop, dims.body_length * 0.28F);
  out_pose.foot_br = center + out_pose.shoulder_offset_br +
                     QVector3D(0.0F, rear_drop, dims.body_length * 0.28F);

  out_pose.shoulder_offset_pose_fl = out_pose.shoulder_offset_fl;
  out_pose.shoulder_offset_pose_fr = out_pose.shoulder_offset_fr;
  out_pose.shoulder_offset_pose_bl = out_pose.shoulder_offset_bl;
  out_pose.shoulder_offset_pose_br = out_pose.shoulder_offset_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const front_upper_len = dims.leg_length * 0.36F;
  float const front_lower_len = dims.leg_length * 0.66F;
  float const rear_upper_len = dims.leg_length * 0.38F;
  float const rear_lower_len = dims.leg_length * 0.70F;
  QVector3D const front_bend_hint(0.0F, -dims.leg_length * 0.22F,
                                  dims.body_length * 0.10F);
  QVector3D const rear_bend_hint(0.0F, -dims.leg_length * 0.20F,
                                 dims.body_length * 0.06F);
  out_pose.knee_fl =
      solve_bent_leg_joint(shoulder_fl, out_pose.foot_fl, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_fr =
      solve_bent_leg_joint(shoulder_fr, out_pose.foot_fr, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_bl =
      solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.knee_br =
      solve_bent_leg_joint(shoulder_br, out_pose.foot_br, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.leg_radius = body_width_vis * 0.125F;

  QVector3D const neck_top = horse_neck_top_local(dims);
  out_pose.neck_base = center + horse_neck_base_local(dims);
  out_pose.neck_top = center + neck_top;
  out_pose.neck_radius = body_width_vis * 0.16F;
  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(0.0F, head_height_vis * 0.08F, head_length_vis * 0.28F);
  out_pose.head_half = QVector3D(
      head_width_vis * 0.40F, head_height_vis * 0.30F, head_length_vis * 0.34F);
}

namespace {

struct pose_leg_sample {
  QVector3D shoulder;
  QVector3D foot;
};

auto compute_pose_leg(
    const Render::GL::HorseDimensions &dims, const Render::GL::HorseGait &gait,
    const horse_pose_profile &profile, float phase_base, float phase_offset,
    float lateral_sign, bool is_rear, float forward_bias, bool is_moving,
    bool is_fighting,
    const QVector3D &anchor_offset) noexcept -> pose_leg_sample {
  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  float const leg_phase = std::fmod(phase_base + phase_offset, 1.0F);
  float stride = 0.0F;
  float lift = 0.0F;
  float shoulder_compress = 0.0F;
  if (is_fighting) {
    if (!is_rear) {

      float const paw_height = dims.leg_length * 0.42F;
      float const strike_reach = dims.body_length * 0.08F;
      float const impact_sink = dims.hoof_height * 0.12F;
      if (leg_phase < 0.45F) {
        float const u = ease_in_out(leg_phase / 0.45F);
        lift = u * paw_height;
        stride = forward_bias + u * strike_reach;
      } else if (leg_phase < 0.65F) {
        lift = paw_height;
        stride = forward_bias + strike_reach;
      } else if (leg_phase < 0.78F) {
        float const u = (leg_phase - 0.65F) / 0.13F;
        float const slam = 1.0F - u;
        lift = slam * slam * slam * slam * paw_height - impact_sink * u * u;
        stride = forward_bias + strike_reach * (1.0F - u * 0.5F);
      } else {
        float const u = (leg_phase - 0.78F) / 0.22F;
        lift = -impact_sink * (1.0F - u * u);
        stride = forward_bias + strike_reach * 0.5F * (1.0F - u);
      }
    } else {

      float const brace_sway =
          std::sin(leg_phase * 2.0F * std::numbers::pi_v<float>) *
          dims.body_length * 0.015F;
      stride = forward_bias - dims.body_length * 0.04F + brace_sway;
      lift = 0.0F;
    }
  } else if (is_moving) {
    float const stance_fraction =
        gait.cycle_time > 0.9F ? 0.60F
                               : (gait.cycle_time > 0.5F ? 0.44F : 0.34F);
    float const stride_extent = gait.stride_swing * 0.56F;
    if (leg_phase < stance_fraction) {
      float const t = ease_in_out(leg_phase / stance_fraction);
      stride = forward_bias + stride_extent * (0.40F - 1.00F * t);
      shoulder_compress =
          std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.08F;
    } else {
      float const t =
          ease_in_out((leg_phase - stance_fraction) / (1.0F - stance_fraction));
      float const swing_sin = std::sin(t * std::numbers::pi_v<float>);
      float const swing_tuck =
          swing_sin * gait.stride_lift * (is_rear ? 0.14F : 0.06F);
      stride = forward_bias - stride_extent * (is_rear ? 0.56F : 0.60F) +
               stride_extent * (is_rear ? 0.94F : 0.98F) * t - swing_tuck;
      lift = swing_sin * gait.stride_lift * (is_rear ? 1.65F : 1.35F);
    }
  }

  float const shoulder_out =
      dims.body_width * (is_rear ? profile.rear_shoulder_out_scale
                                 : profile.front_shoulder_out_scale);

  float const vertical_bias =
      dims.body_height * (is_rear ? profile.rear_vertical_bias_scale
                                  : profile.front_vertical_bias_scale);
  float const longitudinal_bias =
      dims.body_length * (is_rear ? profile.rear_longitudinal_bias_scale
                                  : profile.front_longitudinal_bias_scale);
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out,
                vertical_bias + lift * 0.05F - shoulder_compress,
                stride + longitudinal_bias);

  float const leg_length =
      dims.leg_length * (is_rear ? profile.rear_leg_length_scale
                                 : profile.front_leg_length_scale);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -leg_length + lift,
                           dims.body_length * (is_rear ? 0.24F : 0.12F));

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_animated(const Render::GL::HorseDimensions &dims,
                                   const Render::GL::HorseGait &gait,
                                   HorsePoseMotion motion,
                                   HorseSpecPose &out_pose) noexcept {

  make_horse_spec_pose(dims, motion.bob, out_pose);
  horse_pose_profile const profile = make_horse_pose_profile();

  QVector3D const center = out_pose.barrel_center;
  float const body_width_vis = horse_body_visual_width(dims);
  float const body_height_vis = horse_body_visual_height(dims);
  float const body_length_vis = horse_body_visual_length(dims);
  float const head_width_vis = horse_head_visual_width(dims);
  float const head_height_vis = horse_head_visual_height(dims);
  float const head_length_vis = horse_head_visual_length(dims);

  out_pose.body_half = QVector3D(body_width_vis * profile.body_half_scale.x(),
                                 body_height_vis * profile.body_half_scale.y(),
                                 body_length_vis * profile.body_half_scale.z());

  float const head_height_scale = 1.0F + gait.head_height_jitter;
  QVector3D const neck_base_local = horse_neck_base_local(dims);
  QVector3D const neck_top_local = horse_neck_top_local(dims);
  out_pose.neck_base =
      center + QVector3D(neck_base_local.x(),
                         neck_base_local.y() * head_height_scale,
                         neck_base_local.z());
  out_pose.neck_top = center + QVector3D(neck_top_local.x(),
                                         neck_top_local.y() * head_height_scale,
                                         neck_top_local.z());
  out_pose.neck_radius = body_width_vis * 0.16F;

  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(head_width_vis * profile.head_center_scale.x(),
                head_height_vis * profile.head_center_scale.y(),
                head_length_vis * profile.head_center_scale.z());
  out_pose.head_half = QVector3D(head_width_vis * profile.head_half_scale.x(),
                                 head_height_vis * profile.head_half_scale.y(),
                                 head_length_vis * profile.head_half_scale.z());

  if (motion.is_fighting) {

    float const neck_arch_y = dims.neck_rise * 0.18F;
    float const neck_pull_z = dims.neck_length * 0.04F;
    float const head_drop_y = dims.head_height * 0.10F;
    float const head_push_z = dims.head_length * 0.07F;
    out_pose.neck_top += QVector3D(0.0F, neck_arch_y, -neck_pull_z);

    out_pose.head_center +=
        QVector3D(0.0F, neck_arch_y - head_drop_y, -neck_pull_z + head_push_z);
  }

  QVector3D const front_anchor = horse_front_leg_attach_local(dims);
  QVector3D const rear_anchor = horse_rear_leg_attach_local(dims);

  float const front_bias = 0.0F;
  float const rear_bias = 0.0F;

  Render::GL::HorseGait jittered = gait;
  jittered.stride_swing *= (1.0F + gait.stride_jitter);
  float const front_lat =
      gait.lateral_lead_front == 0.0F ? 0.44F : gait.lateral_lead_front;
  float const rear_lat =
      gait.lateral_lead_rear == 0.0F ? 0.50F : gait.lateral_lead_rear;

  auto fl = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase, 1.0F, false,
      front_bias, motion.is_moving, motion.is_fighting,
      front_anchor + QVector3D(dims.body_width * 0.12F, 0.0F, 0.0F));
  auto fr = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase + front_lat,
      -1.0F, false, front_bias, motion.is_moving, motion.is_fighting,
      front_anchor + QVector3D(-dims.body_width * 0.12F, 0.0F, 0.0F));
  auto bl_leg = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase, 1.0F, true,
      rear_bias, motion.is_moving, motion.is_fighting,
      rear_anchor + QVector3D(-dims.body_width * 0.30F, 0.0F, 0.0F));
  auto br = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase + rear_lat,
      -1.0F, true, rear_bias, motion.is_moving, motion.is_fighting,
      rear_anchor + QVector3D(dims.body_width * 0.30F, 0.0F, 0.0F));

  out_pose.shoulder_offset_pose_fl = fl.shoulder;
  out_pose.shoulder_offset_pose_fr = fr.shoulder;
  out_pose.shoulder_offset_pose_bl = bl_leg.shoulder;
  out_pose.shoulder_offset_pose_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl_leg.foot;
  out_pose.foot_br = center + br.foot;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const front_upper_len = dims.leg_length * 0.38F;
  float const front_lower_len = dims.leg_length * 0.72F;
  float const rear_upper_len = dims.leg_length * 0.40F;
  float const rear_lower_len = dims.leg_length * 0.76F;
  float const gait_bend =
      motion.is_moving ? 1.0F + std::min(gait.stride_lift * 3.0F, 0.40F) : 1.0F;
  QVector3D const front_bend_hint(0.0F, -dims.leg_length * 0.28F,
                                  dims.body_length * 0.14F * gait_bend);
  QVector3D const rear_bend_hint(0.0F, -dims.leg_length * 0.24F,
                                 dims.body_length * 0.08F * gait_bend);
  out_pose.knee_fl =
      solve_bent_leg_joint(shoulder_fl, out_pose.foot_fl, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_fr =
      solve_bent_leg_joint(shoulder_fr, out_pose.foot_fr, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_bl =
      solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.knee_br =
      solve_bent_leg_joint(shoulder_br, out_pose.foot_br, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.pose_leg_radius = dims.body_width * profile.leg_radius_scale;

  out_pose.hoof_scale = QVector3D(dims.body_width * profile.hoof_scale.x(),
                                  dims.hoof_height * profile.hoof_scale.y(),
                                  dims.body_width * profile.hoof_scale.z());
}

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
  make_horse_spec_pose_animated(dims, gait, HorsePoseMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const HorseSpecPose & {
  static const HorseSpecPose pose = build_baseline_pose();
  return pose;
}

auto static_minimal_parts() noexcept
    -> const Render::Creature::CompiledWholeMeshLod & {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(horse_manifest().lod_minimal);
  return compiled;
}

auto static_full_parts() noexcept
    -> const Render::Creature::CompiledWholeMeshLod & {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(horse_manifest().lod_full);
  return compiled;
}

auto build_horse_bind_palette() noexcept
    -> std::array<QMatrix4x4, k_horse_bone_count> {
  std::array<QMatrix4x4, k_horse_bone_count> out{};
  BonePalette tmp{};
  evaluate_horse_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < k_horse_bone_count; ++i) {
    out[i] = tmp[i];
  }
  return out;
}

} // namespace

auto horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "horse";
    s.topology = horse_topology();
    s.lod_minimal = static_minimal_parts().part_graph();
    s.lod_full = static_full_parts().part_graph();
    return s;
  }();
  return spec;
}

auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < k_horse_bone_count) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);
  for (std::size_t i = 0; i < k_horse_bone_count; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(k_horse_bone_count);
}

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, k_horse_bone_count> palette =
      build_horse_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

} // namespace Render::Horse
