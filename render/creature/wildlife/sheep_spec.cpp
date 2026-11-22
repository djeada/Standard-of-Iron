#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "render/creature/wildlife/sheep_spec_internal.h"

namespace Render::Wildlife {

namespace sheep_spec_detail {

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

auto body_motion(const SheepDrive& drive) -> BodyMotion {
  BodyMotion m;
  if (drive.gait == SheepGait::Stand) {
    return m;
  }
  float const duty = gait_plan(drive.gait).stance_duty;
  float const phase = drive.stride_phase;

  if (drive.gait == SheepGait::Run) {
    float const effort = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
    m.fore_mid = (duty * 0.5F) - ((k_run_leg_phase[0] + k_run_leg_phase[1]) * 0.5F);
    m.hind_mid = (duty * 0.5F) - ((k_run_leg_phase[2] + k_run_leg_phase[3]) * 0.5F);

    float const flight = std::cos((phase - m.fore_mid - 0.25F) * k_two_pi * 2.0F) *
                         k_run_flight * effort;
    m.fore = flight - (std::cos((phase - m.fore_mid) * k_two_pi) * k_run_rock * effort);
    m.hind = flight - (std::cos((phase - m.hind_mid) * k_two_pi) * k_run_rock * effort);
    return m;
  }

  m.fore_mid = (duty * 0.5F) - k_walk_leg_phase[0];
  m.hind_mid = (duty * 0.5F) - k_walk_leg_phase[2];
  m.fore = std::cos((phase - m.fore_mid) * k_two_pi * 2.0F) * k_walk_bob;
  m.hind = std::cos((phase - m.hind_mid) * k_two_pi * 2.0F) * k_walk_bob;

  float const left_support = (m.fore_mid + m.hind_mid) * 0.5F;
  m.sway = -std::cos((phase - left_support) * k_two_pi) * k_walk_sway;
  return m;
}

void fill_legs(RigPose& pose, const SheepDrive& drive, const BodyMotion& motion) {
  const GaitPlan plan = gait_plan(drive.gait);
  float const weight = drive.gait == SheepGait::Stand ? 0.0F : 1.0F;
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    LegRest rest = leg_rests()[i];
    bool const hind = k_leg_plans[i].z < 0.0F;
    rest.hip.setY(rest.hip.y() + (hind ? motion.hind : motion.fore));
    rest.hip.setX(rest.hip.x() + motion.sway);
    float const offset =
        drive.gait == SheepGait::Run ? k_run_leg_phase[i] : k_walk_leg_phase[i];
    solve_leg(rest, plan, drive.stride_phase + offset, weight, pose.legs[i]);
  }
}

void fill_head(RigPose& pose, const SheepDrive& drive) {
  float const nibble = (std::sin(drive.stride_phase * k_two_pi * 6.0F) * 0.010F *
                        std::clamp(drive.graze, 0.0F, 1.0F)) -
                       (std::clamp(drive.chew, 0.0F, 1.0F) * 0.012F);
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
        ((side * (sign * 0.92F)) - (facing * 0.20F) - (head_up * 0.16F)).normalized();
    QVector3D const alert =
        ((side * (sign * 0.78F)) + (facing * 0.20F) + (head_up * 0.24F)).normalized();
    QVector3D const dir = lerp(relaxed, alert, drive.alert).normalized();
    QVector3D const tip = base + (dir * 0.116F);
    if (sign_index == 0) {
      pose.ear_base_l = base;
      pose.ear_tip_l = tip;
    } else {
      pose.ear_base_r = base;
      pose.ear_tip_r = tip;
    }
  }
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

auto cartoon_keel(float phase) -> CartoonKeel {
  auto const saturate = [](float t) {
    return std::clamp(t, 0.0F, 1.0F);
  };
  auto const smooth = [&](float t) {
    float const x = saturate(t);
    return x * x * (3.0F - (2.0F * x));
  };
  constexpr float k_pi = 3.14159265F;
  constexpr float k_landing = 0.56F;

  float const p = saturate(phase);
  CartoonKeel keel;

  keel.hop = std::sin(k_pi * saturate(p / 0.30F)) * (1.0F - smooth((p - 0.20F) / 0.2F));
  keel.shiver = std::sin(p * 90.0F) * (1.0F - smooth(p / 0.16F)) * saturate(p / 0.03F);

  float const tip = saturate((p - 0.14F) / (k_landing - 0.14F));
  keel.roll = tip * tip * tip;
  keel.splay = smooth((p - 0.40F) / 0.30F);
  if (p > k_landing) {
    float const since = (p - k_landing) / (1.0F - k_landing);
    float const fade = std::exp(-2.2F * since) * (1.0F - smooth(since));
    keel.boing = std::sin(since * 18.0F) * fade;
    keel.bounce = std::abs(std::sin(since * 9.0F)) * std::exp(-6.0F * since);
  }
  return keel;
}

auto apply_collapse(RigPose& pose, const SheepDrive& drive) -> float {
  float const phase = std::clamp(drive.collapse, 0.0F, 1.0F);
  if (phase <= 0.0F) {
    return 0.0F;
  }

  CartoonKeel const keel = cartoon_keel(phase);

  constexpr float k_spine_pivot_y = 0.318F;
  constexpr float k_roll_radians = -3.02F;
  constexpr float k_hop_height = 0.085F;
  constexpr float k_bounce_height = 0.040F;
  constexpr float k_splay_radians = 0.24F;
  constexpr float k_boing_radians = 0.45F;

  QVector3D const spine(0.0F, k_spine_pivot_y, 0.0F);
  float const roll = k_roll_radians * keel.roll;
  float const lift = (keel.hop * k_hop_height) + (keel.bounce * k_bounce_height);

  auto place = [&](QVector3D& p) {
    p.setX(p.x() + (keel.shiver * 0.006F));
    p = roll_about_spine(p, spine, roll);
    p.setY(std::max(p.y() + lift, 0.020F));
  };

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    auto& leg = pose.legs[i];

    float const out = leg.shoulder.x() < 0.0F ? -1.0F : 1.0F;
    float const front = k_leg_plans[i].z < 0.0F ? -1.0F : 1.0F;
    float const spring = keel.boing * (i % 2U == 0U ? 1.0F : -0.8F) * front;
    float const splay =
        out * ((keel.splay * k_splay_radians) + (spring * k_boing_radians));
    leg.knee = roll_about_spine(leg.knee, leg.shoulder, splay);
    leg.foot = roll_about_spine(leg.foot, leg.shoulder, splay);
    leg.toe = roll_about_spine(leg.toe, leg.shoulder, splay);
    float const reach = keel.splay * 0.030F * front;
    leg.foot += QVector3D(0.0F, 0.0F, reach);
    leg.toe += QVector3D(0.0F, 0.0F, reach * 1.2F);
  }

  for (QVector3D* p : {&pose.root,
                       &pose.body_rear,
                       &pose.body_front,
                       &pose.withers,
                       &pose.poll,
                       &pose.muzzle,
                       &pose.jaw_hinge,
                       &pose.jaw_tip,
                       &pose.ear_base_l,
                       &pose.ear_tip_l,
                       &pose.ear_base_r,
                       &pose.ear_tip_r,
                       &pose.tail_base,
                       &pose.tail_mid,
                       &pose.tail_tip}) {
    place(*p);
  }
  for (auto& leg : pose.legs) {
    place(leg.shoulder);
    place(leg.knee);
    place(leg.foot);
    place(leg.toe);
  }
  return roll;
}

auto rolled_head(const HeadAttachment& held, float roll) -> HeadAttachment {
  if (roll == 0.0F) {
    return held;
  }
  HeadAttachment turned = held;
  QVector3D const origin;
  for (QVector3D* offset : {&turned.muzzle,
                            &turned.jaw_hinge,
                            &turned.jaw_tip,
                            &turned.ear_base_l,
                            &turned.ear_tip_l,
                            &turned.ear_base_r,
                            &turned.ear_tip_r}) {
    *offset = roll_about_spine(*offset, origin, roll);
  }
  return turned;
}

auto make_pose(const SheepDrive& drive) -> RigPose {
  RigPose pose;
  float const effort = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
  bool const running = drive.gait == SheepGait::Run;
  bool const moving = drive.gait != SheepGait::Stand;

  BodyMotion motion = body_motion(drive);
  float const settle = drive.graze * 0.030F;
  motion.fore -= settle;
  motion.hind -= settle;
  float const bob = (motion.fore + motion.hind) * 0.5F;
  QVector3D const shift(motion.sway, 0.0F, 0.0F);

  pose.root = QVector3D(motion.sway * 0.5F, bob, 0.0F);
  pose.body_rear = QVector3D(motion.sway, 0.430F + motion.hind, -0.300F);
  pose.body_front = QVector3D(motion.sway, 0.448F + motion.fore, 0.280F);

  fill_legs(pose, drive, motion);
  fill_head(pose, drive);
  pose.tail_base = k_tail_base;
  pose.tail_mid = k_tail_mid;
  pose.tail_tip = k_tail_tip;
  HeadAttachment const head_attachment = capture_head_attachment(pose);
  SkeletonLengths const skeleton = capture_skeleton_lengths(pose);

  float nod = 0.0F;
  if (running) {
    nod = -std::cos(((drive.stride_phase - motion.fore_mid) * k_two_pi) - 0.9F) *
          0.014F * effort;
  } else if (moving) {
    nod = std::cos(((drive.stride_phase - motion.fore_mid) * k_two_pi * 2.0F) - 1.0F) *
          0.009F;
  }
  QVector3D const head_ride =
      (shift * 0.6F) + QVector3D(0.0F, (motion.fore * 0.55F) + nod, 0.0F);
  pose.withers += shift + QVector3D(0.0F, motion.fore * 0.85F, 0.0F);
  for (auto* point : head_chain(pose)) {
    *point += head_ride;
  }

  float const stream = running ? effort : 0.0F;
  float const wag = std::sin(drive.stride_phase * k_two_pi * 2.0F) * 0.022F *
                    (moving ? 0.4F + (effort * 0.6F) : 0.0F);
  float const flap =
      std::cos(((drive.stride_phase - motion.hind_mid) * k_two_pi) - 1.4F) * stream;
  QVector3D const tail_ride = shift + QVector3D(0.0F, motion.hind, 0.0F);
  pose.tail_base += tail_ride;
  pose.tail_mid += tail_ride + QVector3D(wag * 0.5F, 0.010F * flap, -0.016F * stream);
  pose.tail_tip += tail_ride + QVector3D(wag, 0.026F * flap, -0.050F * stream);

  apply_idle_motion(pose, drive);
  apply_startle(pose, drive);
  float const collapse_roll = apply_collapse(pose, drive);
  reattach_head(pose, rolled_head(head_attachment, collapse_roll));
  enforce_skeleton_lengths(pose, skeleton);
  return pose;
}

} // namespace sheep_spec_detail

using namespace sheep_spec_detail;

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

namespace {

auto lod_geometry_slot() noexcept -> Render::Creature::CreatureLodGeometrySlot& {
  static Render::Creature::CreatureLodGeometrySlot slot(&sheep_runtime_manifest,
                                                        "sheep");
  return slot;
}

} // namespace

void initialize_sheep_asset() {
  lod_geometry_slot().initialize();
}

auto sheep_creature_spec() noexcept -> const Render::Creature::CreatureSpec& {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "sheep";
    s.topology = wildlife_topology();
    s.lod_full = lod_geometry_slot().get().full.part_graph();
    s.lod_minimal = lod_geometry_slot().get().minimal.part_graph();
    return s;
  }();
  return spec;
}

} // namespace Render::Wildlife
