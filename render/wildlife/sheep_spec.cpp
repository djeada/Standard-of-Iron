#include "sheep_spec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "render/creature/species_manifest.h"
#include "sheep_manifest.h"
#include "wildlife_gait.h"

namespace Render::Wildlife {

namespace {

using Render::Creature::Quadruped::EllipsoidNode;
using Render::Creature::Quadruped::MeshNode;
using Render::Creature::Quadruped::SnoutNode;
using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;

constexpr float k_hip_y = 0.300F;
constexpr float k_knee_y = 0.164F;
constexpr float k_fetlock_y = 0.052F;
constexpr float k_fore_hip_z = 0.180F;
constexpr float k_hind_hip_z = -0.212F;

constexpr QVector3D k_withers(0.0F, 0.452F, 0.250F);
constexpr QVector3D k_poll_up(0.0F, 0.616F, 0.366F);
constexpr QVector3D k_graze_dir(0.0F, -0.974F, 0.226F);
constexpr float k_head_length = 0.180F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

constexpr std::array<BodyRing, 19> k_body_rings{{
    {-0.343F, 0.395F, 0.067F, 0.095F, 0.095F},
    {-0.318F, 0.398F, 0.141F, 0.144F, 0.144F},
    {-0.288F, 0.413F, 0.165F, 0.156F, 0.156F},
    {-0.257F, 0.424F, 0.164F, 0.160F, 0.160F},
    {-0.226F, 0.433F, 0.158F, 0.161F, 0.161F},
    {-0.190F, 0.442F, 0.152F, 0.160F, 0.160F},
    {-0.153F, 0.452F, 0.144F, 0.157F, 0.157F},
    {-0.113F, 0.456F, 0.138F, 0.156F, 0.156F},
    {-0.073F, 0.446F, 0.135F, 0.165F, 0.165F},
    {-0.034F, 0.437F, 0.133F, 0.173F, 0.173F},
    {0.006F, 0.432F, 0.133F, 0.177F, 0.177F},
    {0.046F, 0.425F, 0.135F, 0.176F, 0.176F},
    {0.086F, 0.422F, 0.140F, 0.168F, 0.168F},
    {0.126F, 0.427F, 0.147F, 0.158F, 0.158F},
    {0.165F, 0.438F, 0.151F, 0.155F, 0.155F},
    {0.205F, 0.450F, 0.141F, 0.156F, 0.156F},
    {0.245F, 0.460F, 0.113F, 0.152F, 0.152F},
    {0.275F, 0.470F, 0.080F, 0.139F, 0.139F},
    {0.300F, 0.480F, 0.040F, 0.120F, 0.120F},
}};

struct LegPlan {
  float x;
  float z;
  float phase_offset;
  float knee_bias;
  float foot_bias;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-0.104F, k_fore_hip_z, 0.0F, 0.034F, -0.008F},
    {0.104F, k_fore_hip_z, 0.5F, 0.034F, -0.008F},
    {-0.114F, k_hind_hip_z, 0.5F, -0.056F, 0.006F},
    {0.114F, k_hind_hip_z, 0.0F, -0.056F, 0.006F},
}};

constexpr float k_bob_base = 0.011F;
constexpr float k_bob_gain = 0.030F;
constexpr float k_pitch_lag = 0.85F;
constexpr float k_pitch_gain = 0.020F;

constexpr GaitPlan k_gait_walk{0.320F, 0.64F, 0.042F, 0.092F};
constexpr GaitPlan k_gait_run{0.530F, 0.44F, 0.115F, 0.145F};

auto gait_plan(SheepGait gait) noexcept -> GaitPlan {
  switch (gait) {
  case SheepGait::Walk:
    return k_gait_walk;
  case SheepGait::Run:
    return k_gait_run;
  case SheepGait::Stand:
    break;
  }
  return GaitPlan{};
}

auto leg_rests() noexcept -> const std::array<LegRest, k_leg_count>& {
  static const std::array<LegRest, k_leg_count> rests = [] {
    std::array<LegRest, k_leg_count> out{};
    for (std::size_t i = 0; i < k_leg_count; ++i) {
      const LegPlan& plan = k_leg_plans[i];
      out[i] = make_leg_rest({plan.x, k_hip_y, plan.z},
                             {plan.x, k_knee_y, plan.z + plan.knee_bias},
                             {plan.x, k_fetlock_y, plan.z + plan.foot_bias},
                             {plan.x, 0.0F, plan.z + plan.foot_bias},
                             plan.phase_offset);
    }
    return out;
  }();
  return rests;
}

auto lerp(const QVector3D& a, const QVector3D& b, float t) -> QVector3D {
  return a + ((b - a) * std::clamp(t, 0.0F, 1.0F));
}

auto bezier(const QVector3D& p0,
            const QVector3D& p1,
            const QVector3D& p2,
            float t) -> QVector3D {
  float const inv = 1.0F - t;
  return (p0 * (inv * inv)) + (p1 * (2.0F * inv * t)) + (p2 * (t * t));
}

void fill_legs(RigPose& pose,
               const SheepDrive& drive,
               float fore_lift,
               float hind_lift) {
  const GaitPlan plan = gait_plan(drive.gait);
  float const weight = drive.gait == SheepGait::Stand ? 0.0F : 1.0F;
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    LegRest rest = leg_rests()[i];
    bool const hind = k_leg_plans[i].z < 0.0F;
    rest.hip.setY(rest.hip.y() + (hind ? hind_lift : fore_lift));
    solve_leg(rest, plan, drive.stride_phase + rest.phase_offset, weight, pose.legs[i]);
  }
}

void fill_head(RigPose& pose, const SheepDrive& drive) {
  float const nibble = std::sin(drive.stride_phase * k_two_pi * 6.0F) * 0.010F *
                       std::clamp(drive.graze, 0.0F, 1.0F);
  float const neck_length = (k_poll_up - k_withers).length();
  QVector3D const poll_graze = k_withers + (k_graze_dir.normalized() * neck_length);
  QVector3D const control_up(0.0F, 0.590F, 0.292F);
  QVector3D const control_graze =
      k_withers + ((poll_graze - k_withers) * 0.5F) + QVector3D(0.0F, 0.0F, 0.045F);
  QVector3D const poll = lerp(k_poll_up, poll_graze, drive.graze);
  QVector3D const control = lerp(control_up, control_graze, drive.graze);

  pose.withers = k_withers;
  pose.poll = poll;

  QVector3D const facing =
      (poll - bezier(k_withers, control, poll, 0.70F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  QVector3D const muzzle_dir =
      (facing - (head_up * (1.58F - (drive.graze * 1.38F)))).normalized();

  pose.muzzle = poll + (muzzle_dir * k_head_length);
  pose.jaw_hinge = poll + (facing * 0.060F) - (head_up * 0.018F);
  QVector3D const jaw_closed = pose.muzzle - (head_up * 0.014F);
  float const jaw_length = (jaw_closed - pose.jaw_hinge).length();
  pose.jaw_tip = jaw_closed + QVector3D(0.0F, nibble, 0.0F);
  hold_bone(pose.jaw_hinge, pose.jaw_tip, jaw_length);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll - (facing * 0.014F) + (side * (sign * 0.050F)) + (head_up * 0.016F);
    QVector3D const relaxed =
        ((side * (sign * 0.90F)) - (facing * 0.22F) - (head_up * 0.32F)).normalized();
    QVector3D const alert =
        ((side * (sign * 0.78F)) + (facing * 0.20F) + (head_up * 0.24F)).normalized();
    QVector3D const dir = lerp(relaxed, alert, drive.alert).normalized();
    QVector3D const tip = base + (dir * 0.100F);
    if (sign_index == 0) {
      pose.ear_base_l = base;
      pose.ear_tip_l = tip;
    } else {
      pose.ear_base_r = base;
      pose.ear_tip_r = tip;
    }
  }
}

struct HeadAttachment {
  float neck_length{0.0F};
  QVector3D muzzle{};
  QVector3D jaw_hinge{};
  QVector3D jaw_tip{};
  QVector3D ear_base_l{};
  QVector3D ear_tip_l{};
  QVector3D ear_base_r{};
  QVector3D ear_tip_r{};
};

auto capture_head_attachment(const RigPose& pose) -> HeadAttachment {
  HeadAttachment held;
  held.neck_length = (pose.poll - pose.withers).length();
  held.muzzle = pose.muzzle - pose.poll;
  held.jaw_hinge = pose.jaw_hinge - pose.poll;
  held.jaw_tip = pose.jaw_tip - pose.poll;
  held.ear_base_l = pose.ear_base_l - pose.poll;
  held.ear_tip_l = pose.ear_tip_l - pose.poll;
  held.ear_base_r = pose.ear_base_r - pose.poll;
  held.ear_tip_r = pose.ear_tip_r - pose.poll;
  return held;
}

void reattach_head(RigPose& pose, const HeadAttachment& held) {
  QVector3D const neck = pose.poll - pose.withers;
  float const length = neck.length();
  if (length > 1.0e-5F && held.neck_length > 1.0e-5F) {
    pose.poll = pose.withers + ((neck / length) * held.neck_length);
  }
  pose.muzzle = pose.poll + held.muzzle;
  pose.jaw_hinge = pose.poll + held.jaw_hinge;
  pose.jaw_tip = pose.poll + held.jaw_tip;
  pose.ear_base_l = pose.poll + held.ear_base_l;
  pose.ear_tip_l = pose.poll + held.ear_tip_l;
  pose.ear_base_r = pose.poll + held.ear_base_r;
  pose.ear_tip_r = pose.poll + held.ear_tip_r;
}

auto rotate_about_x(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D {
  QVector3D const d = point - pivot;
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return pivot + QVector3D(d.x(), (d.y() * c) - (d.z() * s), (d.y() * s) + (d.z() * c));
}

auto rotate_about_y(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D {
  QVector3D const d = point - pivot;
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return pivot + QVector3D((d.x() * c) + (d.z() * s), d.y(), (d.z() * c) - (d.x() * s));
}

auto head_chain(RigPose& pose) -> std::array<QVector3D*, 8> {
  return {&pose.poll,
          &pose.muzzle,
          &pose.jaw_hinge,
          &pose.jaw_tip,
          &pose.ear_base_l,
          &pose.ear_base_r,
          &pose.ear_tip_l,
          &pose.ear_tip_r};
}

void apply_idle_motion(RigPose& pose, const SheepDrive& drive) {
  if (drive.breath == 0.0F && drive.head_turn == 0.0F && drive.head_dip == 0.0F &&
      drive.ear_flick == 0.0F && drive.tail_flick == 0.0F &&
      drive.weight_shift == 0.0F) {
    return;
  }

  QVector3D const swell(0.0F, 0.011F * drive.breath, 0.0F);
  float const shift = 0.016F * drive.weight_shift;

  auto ride = [&](QVector3D& point, float swell_weight, float shift_weight) {
    point += swell * swell_weight;
    point.setX(point.x() + (shift * shift_weight));
  };

  ride(pose.root, 0.35F, 0.55F);
  ride(pose.body_front, 1.0F, 0.72F);
  ride(pose.body_rear, 0.70F, 1.0F);
  ride(pose.withers, 0.90F, 0.72F);
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    bool const hind = k_leg_plans[i].z < 0.0F;
    ride(pose.legs[i].shoulder, hind ? 0.50F : 0.85F, hind ? 1.0F : 0.68F);
    ride(pose.legs[i].knee, hind ? 0.20F : 0.34F, hind ? 0.44F : 0.28F);
  }

  QVector3D const neck = pose.withers;
  float const yaw = 0.30F * drive.head_turn;
  float const pitch = 0.17F * drive.head_dip;
  float const flick = 0.55F * drive.ear_flick;
  pose.ear_tip_l = rotate_about_y(pose.ear_tip_l, pose.ear_base_l, flick);
  pose.ear_tip_r = rotate_about_y(pose.ear_tip_r, pose.ear_base_r, -flick);
  for (auto* point : head_chain(pose)) {
    *point += swell * 0.85F;
    point->setX(point->x() + (shift * 0.74F));
    *point = rotate_about_y(*point, neck, yaw);
    *point = rotate_about_x(*point, neck, pitch);
  }

  ride(pose.tail_base, 0.60F, 1.0F);
  pose.tail_mid += swell * 0.4F;
  pose.tail_tip += swell * 0.3F;
  pose.tail_mid.setX(pose.tail_mid.x() + (0.020F * drive.tail_flick) + shift);
  pose.tail_tip.setX(pose.tail_tip.x() + (0.048F * drive.tail_flick) + shift);
}

void apply_startle(RigPose& pose, const SheepDrive& drive) {
  float const phase = std::clamp(drive.startle, 0.0F, 1.0F);
  if (phase <= 0.0F) {
    return;
  }

  auto const smoother = [](float value) {
    float const t = std::clamp(value, 0.0F, 1.0F);
    return t * t * t * ((t * ((t * 6.0F) - 15.0F)) + 10.0F);
  };
  auto const window = [&](float from, float to) {
    if (phase <= from) {
      return 0.0F;
    }
    if (phase >= to) {
      return 1.0F;
    }
    return smoother((phase - from) / (to - from));
  };

  float const flinch = window(0.0F, 0.14F) - window(0.30F, 0.62F);

  float const hop = window(0.06F, 0.30F) - window(0.30F, 0.72F);

  float const settle = 1.0F - window(0.62F, 1.0F);

  float const away = 0.150F * flinch;
  float const rise = 0.075F * hop;
  float const twist = -0.42F * flinch;

  float const throw_up = -0.46F * flinch;

  QVector3D const pivot(0.0F, 0.240F, -0.120F);

  auto shove = [&](QVector3D& point, float lateral, float lift) {
    point.setX(point.x() + (away * lateral));
    point.setY(point.y() + (rise * lift));
    point = rotate_about_y(point, pivot, twist * lateral);
  };

  shove(pose.root, 0.80F, 0.85F);
  shove(pose.body_rear, 1.00F, 1.00F);
  shove(pose.body_front, 0.70F, 0.80F);
  shove(pose.withers, 0.65F, 0.75F);

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    bool const hind = k_leg_plans[i].z < 0.0F;

    float const tuck = hind ? -0.070F : 0.048F;
    float const kick = hind ? 0.090F : 0.030F;
    pose.legs[i].knee += QVector3D(0.0F, 0.052F * hop, tuck * hop);
    pose.legs[i].foot += QVector3D(0.0F, 0.086F * hop, (tuck - kick) * hop);
    pose.legs[i].toe += QVector3D(0.0F, 0.094F * hop, (tuck - kick) * hop);
    shove(pose.legs[i].shoulder, hind ? 1.0F : 0.7F, hind ? 1.0F : 0.8F);
    shove(pose.legs[i].knee, hind ? 0.8F : 0.6F, hind ? 0.85F : 0.7F);
    shove(pose.legs[i].foot, hind ? 0.5F : 0.4F, hind ? 0.6F : 0.5F);
    shove(pose.legs[i].toe, hind ? 0.4F : 0.3F, hind ? 0.5F : 0.4F);
  }

  QVector3D const neck = pose.withers;
  for (auto* point : head_chain(pose)) {
    point->setY(point->y() + (rise * 0.9F));
    *point = rotate_about_x(*point, neck, throw_up);
    *point = rotate_about_y(*point, pivot, twist * 0.9F);
  }

  float const lash = 0.055F * flinch * settle;
  pose.tail_base.setX(pose.tail_base.x() + (away * 0.9F));
  pose.tail_base.setY(pose.tail_base.y() + (rise * 0.9F));
  pose.tail_mid.setX(pose.tail_mid.x() + (away * 0.9F) - lash);
  pose.tail_tip.setX(pose.tail_tip.x() + (away * 0.9F) - (lash * 2.2F));
  pose.tail_mid.setY(pose.tail_mid.y() + (rise * 0.8F) + (0.030F * flinch));
  pose.tail_tip.setY(pose.tail_tip.y() + (rise * 0.7F) + (0.048F * flinch));
}

void apply_collapse(RigPose& pose, const SheepDrive& drive) {
  float const phase = std::clamp(drive.collapse, 0.0F, 1.0F);
  if (phase <= 0.0F) {
    return;
  }

  DeathMotion const m = death_motion(phase);

  constexpr float k_lying_y = 0.175F;
  constexpr float k_rest_body_y = 0.439F;
  constexpr float k_roll_radians = -1.62F;

  float const descent = (k_rest_body_y - k_lying_y) * m.fall;
  QVector3D const spine(0.0F, k_lying_y, 0.0F);
  float const roll = k_roll_radians * m.roll;
  float const head_descent = (k_rest_body_y - k_lying_y) * m.head;

  auto place = [&](QVector3D& p, float descent_amount, float bounce_weight) {
    p.setY(p.y() - descent_amount + (m.settle * 0.019F * bounce_weight));
    p = roll_about_spine(p, spine, roll);
    p.setY(std::max(p.y(), 0.020F));
  };

  place(pose.root, descent * 0.92F, 0.5F);
  place(pose.body_rear, descent, 1.0F);
  place(pose.body_front, descent, 0.9F);
  place(pose.withers, descent, 0.8F);

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    float const out = (i % 2U == 0U) ? -1.0F : 1.0F;

    float const fold = m.buckle * 0.62F;
    float const kick = m.thrash * 0.042F * out;

    pose.legs[i].knee += QVector3D(0.0F, -0.030F * fold, 0.018F * fold * out);
    pose.legs[i].foot += QVector3D(0.0F, -0.048F * fold, 0.034F * fold * out + kick);
    pose.legs[i].toe += QVector3D(0.0F, -0.054F * fold, 0.042F * fold * out + kick);

    place(pose.legs[i].shoulder, descent, 0.9F);
    place(pose.legs[i].knee, descent, 0.7F);
    place(pose.legs[i].foot, descent, 0.5F);
    place(pose.legs[i].toe, descent, 0.4F);
  }

  pose.poll += QVector3D(0.0F, 0.0F, -0.030F * m.head);
  pose.muzzle += QVector3D(0.0F, 0.0F, -0.052F * m.head);

  place(pose.poll, head_descent, 0.7F);
  place(pose.muzzle, head_descent, 0.6F);
  place(pose.jaw_hinge, head_descent, 0.6F);
  place(pose.jaw_tip, head_descent, 0.5F);
  pose.jaw_tip += QVector3D(0.0F, -0.016F * m.head, 0.010F * m.head);

  place(pose.ear_base_l, head_descent, 0.5F);
  place(pose.ear_base_r, head_descent, 0.5F);
  place(pose.ear_tip_l, head_descent, 0.4F);
  place(pose.ear_tip_r, head_descent, 0.4F);

  float const tail_lag = std::clamp(m.head * 1.05F, 0.0F, 1.0F);
  place(pose.tail_base, descent, 0.8F);
  place(pose.tail_mid, (k_rest_body_y - k_lying_y) * tail_lag, 0.6F);
  place(pose.tail_tip, (k_rest_body_y - k_lying_y) * tail_lag, 0.5F);
}

auto make_pose(const SheepDrive& drive) -> RigPose {
  RigPose pose;
  float const cadence = drive.stride_phase * k_two_pi * 2.0F;
  float const gait = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
  float const bob =
      (std::sin(cadence) * (k_bob_base + (k_bob_gain * gait))) - (drive.graze * 0.030F);
  float const pitch = std::sin(cadence - k_pitch_lag) * k_pitch_gain * gait;

  pose.root = QVector3D(0.0F, bob, 0.0F);
  pose.body_rear = QVector3D(0.0F, 0.430F + bob + pitch, -0.300F);
  pose.body_front = QVector3D(0.0F, 0.448F + bob - pitch, 0.280F);

  fill_legs(pose, drive, bob - pitch, bob + pitch);
  fill_head(pose, drive);
  pose.tail_base = QVector3D(0.0F, 0.520F, -0.300F);
  pose.tail_mid = QVector3D(0.0F, 0.446F, -0.330F);
  pose.tail_tip = QVector3D(0.0F, 0.382F, -0.336F);
  HeadAttachment const head_attachment = capture_head_attachment(pose);
  SkeletonLengths const skeleton = capture_skeleton_lengths(pose);

  float const nod =
      std::sin(cadence - 1.35F) * (k_bob_base + (k_bob_gain * gait)) * 0.62F;
  QVector3D const head_ride(0.0F, (bob * 0.55F) + nod, 0.0F);
  pose.withers += QVector3D(0.0F, bob * 0.85F - pitch * 0.6F, 0.0F);
  pose.poll += head_ride;
  pose.muzzle += head_ride;
  pose.jaw_hinge += head_ride;
  pose.jaw_tip += head_ride;
  pose.ear_base_l += head_ride;
  pose.ear_base_r += head_ride;
  pose.ear_tip_l += head_ride;
  pose.ear_tip_r += head_ride;

  float const wag =
      std::sin(drive.stride_phase * k_two_pi) * 0.022F * drive.speed_ratio;
  pose.tail_mid.setX(wag * 0.5F);
  pose.tail_tip.setX(wag);

  apply_idle_motion(pose, drive);
  apply_startle(pose, drive);
  apply_collapse(pose, drive);
  reattach_head(pose, head_attachment);
  enforce_skeleton_lengths(pose, skeleton);
  return pose;
}

bool g_minimal_tessellation = false;

auto ellipsoid(std::string_view name,
               Bone bone,
               std::uint8_t role,
               const QVector3D& center,
               const QVector3D& radii,
               std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  EllipsoidNode data;
  data.center = center;
  data.radii = radii;
  if (g_minimal_tessellation) {
    data.ring_count = 3U;
    data.ring_vertices = 6U;
  }
  node.data = data;
  return node;
}

auto barrel(std::string_view name,
            Bone bone,
            std::uint8_t role,
            std::vector<Render::Creature::Quadruped::BarrelRing> rings) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  Render::Creature::Quadruped::BarrelNode data;
  data.rings = std::move(rings);
  node.data = data;
  return node;
}

auto tube(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& start,
          const QVector3D& end,
          float start_radius,
          float end_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  TubeNode data;
  data.start = start;
  data.end = end;
  data.start_radius = start_radius;
  data.end_radius = end_radius;
  if (g_minimal_tessellation) {
    data.segment_count = 2U;
    data.ring_vertices = 5U;
  }
  node.data = data;
  return node;
}

auto ear_flap(std::string_view name,
              Bone bone,
              std::uint8_t role,
              const QVector3D& base,
              const QVector3D& tip,
              const QVector3D& face_normal,
              float half_width) -> MeshNode {
  QVector3D const along = (tip - base).normalized();
  QVector3D normal = face_normal - (along * QVector3D::dotProduct(face_normal, along));
  if (normal.lengthSquared() <= 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  }
  normal.normalize();
  QVector3D const across = QVector3D::crossProduct(normal, along).normalized();

  constexpr std::array<std::pair<float, float>, 5> k_profile{{
      {0.00F, 0.38F},
      {0.20F, 0.86F},
      {0.46F, 1.00F},
      {0.76F, 0.70F},
      {1.00F, 0.06F},
  }};

  float const length = (tip - base).length();
  std::vector<QVector3D> outline;
  outline.reserve(k_profile.size() * 2U);
  for (const auto& [t, w] : k_profile) {
    outline.push_back(base + (along * (t * length)) - (across * (w * half_width)));
  }
  for (auto it = k_profile.rbegin(); it != k_profile.rend(); ++it) {
    outline.push_back(base + (along * (it->first * length)) +
                      (across * (it->second * half_width)));
  }

  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  Render::Creature::Quadruped::FlatFanNode data;
  data.outline = std::move(outline);
  data.thickness_axis = normal;
  data.thickness = 0.014F;
  node.data = data;
  return node;
}

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode> {
  g_minimal_tessellation = wanted_lod == Render::Creature::k_lod_minimal;
  const RigPose& bind = sheep_bind_pose();
  std::vector<MeshNode> nodes;
  nodes.reserve(64U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  std::vector<Render::Creature::Quadruped::BarrelRing> body_rings;
  body_rings.reserve(k_body_rings.size());
  for (std::size_t i = 0; i < k_body_rings.size(); ++i) {
    const BodyRing& ring = k_body_rings[i];
    float const fi = static_cast<float>(i);

    float const clump_w =
        1.0F + (0.040F * std::sin(fi * 2.1F)) + (0.024F * std::sin((fi * 3.7F) + 1.3F));
    float const clump_t = 1.0F + (0.045F * std::sin((fi * 2.6F) + 0.7F)) +
                          (0.022F * std::sin((fi * 4.3F) + 2.1F));
    body_rings.push_back({ring.z,
                          ring.y,
                          ring.half_width * clump_w,
                          ring.top * clump_t,
                          ring.bottom * clump_w});
  }
  nodes.push_back(
      barrel("sheep.fleece", Bone::Body, k_sheep_role_wool, std::move(body_rings)));

  nodes.push_back(ellipsoid("sheep.belly",
                            Bone::Body,
                            k_sheep_role_wool_grubby,
                            {0.0F, 0.268F, -0.010F},
                            {0.112F, 0.042F, 0.210F}));
  nodes.push_back(ellipsoid("sheep.brisket",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.352F, 0.220F},
                            {0.076F, 0.068F, 0.062F}));
  nodes.push_back(ellipsoid("sheep.ruff",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.462F, 0.240F},
                            {0.098F, 0.096F, 0.084F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("sheep.neck.lower",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       bind.withers,
                       neck_mid,
                       0.094F,
                       0.078F));
  nodes.push_back(tube("sheep.neck.upper",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       neck_mid,
                       bind.poll,
                       0.078F,
                       0.064F));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("sheep.cranium",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.036F) + (head_up * 0.008F),
                            {0.070F, 0.066F, 0.080F}));

  nodes.push_back(ellipsoid("sheep.poll_wool",
                            Bone::Head,
                            k_sheep_role_wool_light,
                            bind.poll - (facing * 0.048F) + (head_up * 0.020F),
                            {0.072F, 0.058F, 0.064F}));
  nodes.push_back(ellipsoid("sheep.cheek_wool",
                            Bone::NeckTop,
                            k_sheep_role_wool,
                            bind.poll - ((bind.poll - bind.withers) * 0.30F),
                            {0.076F, 0.072F, 0.068F}));

  {
    MeshNode node;
    node.debug_name = "sheep.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_sheep_role_face;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.062F);
    data.end = bind.muzzle;
    data.base_radius = 0.048F;
    data.tip_radius = 0.030F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(ellipsoid("sheep.nose",
                            Bone::Head,
                            k_sheep_role_nose,
                            bind.muzzle + (facing * 0.006F),
                            {0.030F, 0.026F, 0.024F}));
  nodes.push_back(tube("sheep.jaw",
                       Bone::Jaw,
                       k_sheep_role_face,
                       bind.jaw_hinge,
                       bind.jaw_tip,
                       0.030F,
                       0.022F));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("sheep.eye",
                              Bone::Head,
                              k_sheep_role_eye,
                              bind.poll + (facing * 0.032F) + (head_up * 0.016F) +
                                  (side * (sign * 0.050F)),
                              {0.014F, 0.014F, 0.014F},
                              k_full));
  }

  nodes.push_back(ear_flap("sheep.ear_l",
                           Bone::EarL,
                           k_sheep_role_face,
                           bind.ear_base_l,
                           bind.ear_tip_l,
                           head_up,
                           0.036F));
  nodes.push_back(ear_flap("sheep.ear_r",
                           Bone::EarR,
                           k_sheep_role_face,
                           bind.ear_base_r,
                           bind.ear_tip_r,
                           head_up,
                           0.036F));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    bool const hind = i >= 2U;

    nodes.push_back(ellipsoid(hind ? "sheep.thigh" : "sheep.shoulder",
                              k_shoulders[i],
                              k_sheep_role_wool,
                              {joints.shoulder.x() * (hind ? 1.02F : 1.04F),
                               k_hip_y + (hind ? 0.068F : 0.074F),
                               joints.shoulder.z() + (hind ? 0.004F : 0.006F)},
                              hind ? QVector3D(0.062F, 0.094F, 0.086F)
                                   : QVector3D(0.056F, 0.084F, 0.078F)));
    nodes.push_back(ellipsoid(
        "sheep.skirt",
        Bone::Body,
        k_sheep_role_wool,
        {joints.shoulder.x() * 0.94F, k_hip_y + 0.016F, joints.shoulder.z() * 0.96F},
        {0.056F, 0.042F, 0.070F}));

    nodes.push_back(tube("sheep.leg.upper",
                         k_shoulders[i],
                         k_sheep_role_wool_shade,
                         joints.shoulder,
                         joints.knee,
                         0.048F,
                         0.026F));
    nodes.push_back(tube("sheep.leg.lower",
                         k_knees[i],
                         k_sheep_role_face,
                         joints.knee,
                         joints.foot,
                         0.024F,
                         0.019F));
    nodes.push_back(tube("sheep.leg.hoof",
                         k_feet[i],
                         k_sheep_role_hoof,
                         joints.foot,
                         joints.toe,
                         0.021F,
                         0.026F));
  }

  nodes.push_back(tube("sheep.tail.base",
                       Bone::TailBase,
                       k_sheep_role_wool,
                       bind.tail_base,
                       bind.tail_mid,
                       0.038F,
                       0.030F));
  nodes.push_back(tube("sheep.tail.tip",
                       Bone::TailTip,
                       k_sheep_role_wool_grubby,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.030F,
                       0.014F));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  g_minimal_tessellation = false;
  return nodes;
}

auto static_full_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(sheep_manifest().lod_full);
  return compiled;
}

auto static_minimal_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(sheep_manifest().lod_minimal);
  return compiled;
}

} // namespace

auto sheep_gait_advance(SheepGait gait) noexcept -> float {
  return gait_advance(gait_plan(gait));
}

auto sheep_bind_pose() noexcept -> const RigPose& {
  static const RigPose pose = make_pose(SheepDrive{});
  return pose;
}

auto sheep_pose(const SheepDrive& drive) noexcept -> RigPose {
  return make_pose(drive);
}

auto sheep_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const BonePalette palette = [] {
    BonePalette out{};
    evaluate_wildlife_skeleton(sheep_bind_pose(), out);
    return out;
  }();
  return {palette.data(), palette.size()};
}

auto sheep_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_full);
  return {nodes.data(), nodes.size()};
}

auto sheep_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_minimal);
  return {nodes.data(), nodes.size()};
}

auto sheep_creature_spec() noexcept -> const Render::Creature::CreatureSpec& {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "sheep";
    s.topology = wildlife_topology();
    s.lod_full = static_full_parts().part_graph();
    s.lod_minimal = static_minimal_parts().part_graph();
    return s;
  }();
  return spec;
}

} // namespace Render::Wildlife
