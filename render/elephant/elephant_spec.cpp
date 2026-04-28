#include "elephant_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../submitter.h"
#include "elephant_manifest.h"

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
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::Creature::SkeletonTopology;

constexpr std::array<BoneDef, kElephantBoneCount> kElephantBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", static_cast<BoneIndex>(ElephantBone::Root)},
    {"ShoulderFL", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeFL", static_cast<BoneIndex>(ElephantBone::ShoulderFL)},
    {"FootFL", static_cast<BoneIndex>(ElephantBone::KneeFL)},
    {"ShoulderFR", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeFR", static_cast<BoneIndex>(ElephantBone::ShoulderFR)},
    {"FootFR", static_cast<BoneIndex>(ElephantBone::KneeFR)},
    {"ShoulderBL", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeBL", static_cast<BoneIndex>(ElephantBone::ShoulderBL)},
    {"FootBL", static_cast<BoneIndex>(ElephantBone::KneeBL)},
    {"ShoulderBR", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeBR", static_cast<BoneIndex>(ElephantBone::ShoulderBR)},
    {"FootBR", static_cast<BoneIndex>(ElephantBone::KneeBR)},
    {"Head", static_cast<BoneIndex>(ElephantBone::Root)},
    {"TrunkTip", static_cast<BoneIndex>(ElephantBone::Head)},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kElephantSockets{};
constexpr std::uint8_t kRoleSkin = 1;
constexpr int kElephantMaterialId = 6;

constexpr SkeletonTopology kElephantTopology{
    std::span<const BoneDef>(kElephantBones),
    std::span<const Render::Creature::SocketDef>(kElephantSockets),
};

constexpr float k_pi = 3.14159265358979323846F;

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
  QVector3D shoulder_to_foot = foot - shoulder;
  float const dist_sq =
      QVector3D::dotProduct(shoulder_to_foot, shoulder_to_foot);
  if (dist_sq <= 1.0e-6F) {
    return shoulder + QVector3D(0.0F, -upper_len, 0.0F);
  }

  float const total_len = std::max(upper_len + lower_len, 1.0e-4F);
  float const min_len =
      std::max(std::abs(upper_len - lower_len) + 1.0e-4F, total_len * 0.10F);
  float const dist =
      std::clamp(std::sqrt(dist_sq), min_len, total_len - 1.0e-4F);
  QVector3D const dir = shoulder_to_foot / std::sqrt(dist_sq);

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

[[nodiscard]] auto darken(const QVector3D &c, float s) noexcept -> QVector3D {
  return c * s;
}

struct LegResult {
  QVector3D shoulder;
  QVector3D foot;
};

[[nodiscard]] auto compute_pose_leg(const Render::GL::ElephantDimensions &d,
                                    const Render::GL::ElephantGait &g,
                                    const ElephantPoseMotion &motion,
                                    float lateral_sign, float forward_bias,
                                    float phase_offset) noexcept -> LegResult {
  float const leg_phase =
      motion.is_fighting
          ? std::fmod(motion.anim_time / 1.15F + phase_offset, 1.0F)
          : std::fmod(motion.phase + phase_offset, 1.0F);
  bool const is_front = (forward_bias > 0.0F);

  float stride = 0.0F;
  float lift = 0.0F;

  if (motion.is_fighting) {
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
    // Front legs lift higher for clearance and use shorter strides for balance;
    // rear legs push with longer strides to provide propulsion.
    float const stride_scale = is_front ? 0.52F : 0.68F;
    float const lift_scale = is_front ? 1.05F : 0.85F;
    stride = std::sin(angle) * g.stride_swing * stride_scale;
    float const lift_raw = std::sin(angle);
    lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * lift_scale : 0.0F;
  }

  QVector3D const shoulder(lateral_sign * d.body_width * 0.40F,
                           -d.body_height * 0.30F, forward_bias + stride);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -d.leg_length * 0.85F + lift, stride * 0.45F);

  return {shoulder, foot};
}

} // namespace

auto elephant_topology() noexcept -> const SkeletonTopology & {
  return *elephant_manifest().topology;
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

  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderFL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderFR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderBL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderBR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_br);

  out_palette[static_cast<std::size_t>(ElephantBone::KneeFL)] =
      translation_matrix(pose.knee_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeFR)] =
      translation_matrix(pose.knee_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeBL)] =
      translation_matrix(pose.knee_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeBR)] =
      translation_matrix(pose.knee_br);

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
  out_roles.fill(QVector3D(0.0F, 0.0F, 0.0F));
  out_roles[0] = variant.skin_color;
  out_roles[1] = variant.skin_color;
  out_roles[2] = darken(variant.skin_color, 0.92F);
  out_roles[3] = darken(variant.skin_color, 0.88F);
  out_roles[4] = darken(variant.skin_color, 0.94F);
  out_roles[5] = variant.tusk_color;
  out_roles[6] = QVector3D(0.0F, 0.0F, 0.0F);
  out_roles[7] = variant.toenail_color;
  out_roles[8] = QVector3D(0.05F, 0.04F, 0.04F);
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

  out_pose.shoulder_offset_pose_fl = out_pose.shoulder_offset_fl;
  out_pose.shoulder_offset_pose_fr = out_pose.shoulder_offset_fr;
  out_pose.shoulder_offset_pose_bl = out_pose.shoulder_offset_bl;
  out_pose.shoulder_offset_pose_br = out_pose.shoulder_offset_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const upper_len = dims.leg_length * 0.46F;
  float const lower_len = dims.leg_length * 0.44F;
  QVector3D const front_bend_hint(0.0F, 0.0F, dims.body_length * 0.12F);
  QVector3D const rear_bend_hint(0.0F, 0.0F, -dims.body_length * 0.10F);
  out_pose.knee_fl = solve_bent_leg_joint(
      shoulder_fl, out_pose.foot_fl, upper_len, lower_len, front_bend_hint);
  out_pose.knee_fr = solve_bent_leg_joint(
      shoulder_fr, out_pose.foot_fr, upper_len, lower_len, front_bend_hint);
  out_pose.knee_bl = solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl,
                                          upper_len, lower_len, rear_bend_hint);
  out_pose.knee_br = solve_bent_leg_joint(shoulder_br, out_pose.foot_br,
                                          upper_len, lower_len, rear_bend_hint);

  out_pose.leg_radius = dims.leg_radius * 0.70F;
}

void make_elephant_spec_pose_animated(
    const Render::GL::ElephantDimensions &dims,
    const Render::GL::ElephantGait &gait, const ElephantPoseMotion &motion,
    ElephantSpecPose &out_pose) noexcept {

  make_elephant_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  out_pose.body_half =
      QVector3D(dims.body_width * 0.50F, dims.body_height * 0.45F,
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

  if (motion.is_fighting) {
    // Raised trunk in threat/attack display; head angles down for a charge.
    float const phase_wave =
        std::sin(motion.anim_time * k_pi * 2.0F / 1.15F);
    float const trunk_raise =
        dims.trunk_length * (0.55F + 0.08F * phase_wave);
    float const trunk_retract_z = dims.trunk_length * 0.35F;
    out_pose.trunk_end +=
        QVector3D(0.0F, trunk_raise, -trunk_retract_z);
    float const head_lower = dims.head_height * 0.07F;
    out_pose.head_center += QVector3D(0.0F, -head_lower, 0.0F);
  }

  float const front_forward = dims.body_length * 0.35F;
  float const rear_forward = -dims.body_length * 0.35F;

  auto apply_leg = [&](float lat, float fwd, float phase_offset,
                       QVector3D &shoulder_out, QVector3D &foot_out) noexcept {
    auto r = compute_pose_leg(dims, gait, motion, lat, fwd, phase_offset);
    shoulder_out = r.shoulder;
    foot_out = center + r.foot;
  };

  apply_leg(1.0F, front_forward, gait.front_leg_phase,
            out_pose.shoulder_offset_pose_fl, out_pose.foot_pose_fl);
  apply_leg(-1.0F, front_forward, gait.front_leg_phase + 0.50F,
            out_pose.shoulder_offset_pose_fr, out_pose.foot_pose_fr);
  apply_leg(1.0F, rear_forward, gait.rear_leg_phase,
            out_pose.shoulder_offset_pose_bl, out_pose.foot_pose_bl);
  apply_leg(-1.0F, rear_forward, gait.rear_leg_phase + 0.50F,
            out_pose.shoulder_offset_pose_br, out_pose.foot_pose_br);

  out_pose.foot_fl = out_pose.foot_pose_fl;
  out_pose.foot_fr = out_pose.foot_pose_fr;
  out_pose.foot_bl = out_pose.foot_pose_bl;
  out_pose.foot_br = out_pose.foot_pose_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const upper_len = dims.leg_length * 0.46F;
  float const lower_len = dims.leg_length * 0.44F;
  float const bend_scale = motion.is_moving || motion.is_fighting
                               ? 1.0F + std::min(gait.stride_lift * 5.0F, 0.45F)
                               : 1.15F;
  QVector3D const front_bend_hint(0.0F, 0.0F,
                                  dims.body_length * 0.11F * bend_scale);
  QVector3D const rear_bend_hint(0.0F, 0.0F,
                                 -dims.body_length * 0.09F * bend_scale);
  out_pose.knee_fl = solve_bent_leg_joint(
      shoulder_fl, out_pose.foot_fl, upper_len, lower_len, front_bend_hint);
  out_pose.knee_fr = solve_bent_leg_joint(
      shoulder_fr, out_pose.foot_fr, upper_len, lower_len, front_bend_hint);
  out_pose.knee_bl = solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl,
                                          upper_len, lower_len, rear_bend_hint);
  out_pose.knee_br = solve_bent_leg_joint(shoulder_br, out_pose.foot_br,
                                          upper_len, lower_len, rear_bend_hint);

  out_pose.pose_leg_radius = dims.leg_radius * 0.85F;

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
  make_elephant_spec_pose_animated(dims, gait, ElephantPoseMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const ElephantSpecPose & {
  static const ElephantSpecPose pose = build_baseline_pose();
  return pose;
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

auto static_minimal_parts() noexcept
    -> const Render::Creature::CompiledWholeMeshLod & {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(elephant_manifest().lod_minimal);
  return compiled;
}

auto static_full_parts() noexcept
    -> const Render::Creature::CompiledWholeMeshLod & {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(elephant_manifest().lod_full);
  return compiled;
}

} // namespace

auto elephant_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "elephant";
    s.topology = elephant_topology();
    s.lod_minimal = static_minimal_parts().part_graph();
    s.lod_full = static_full_parts().part_graph();
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

} // namespace Render::Elephant
