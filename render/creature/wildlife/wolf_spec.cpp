#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "render/creature/wildlife/wolf_spec_internal.h"

namespace Render::Wildlife {

namespace wolf_spec_detail {

auto leg_phase_offset(WolfGait gait, std::size_t leg) -> float {
  return gait == WolfGait::Run ? k_run_leg_phase[leg] : k_walk_leg_phase[leg];
}

auto gait_plan(WolfGait gait) noexcept -> GaitPlan {
  switch (gait) {
  case WolfGait::Stalk:
    return k_gait_stalk;
  case WolfGait::Walk:
    return k_gait_walk;
  case WolfGait::Run:
    return k_gait_run;
  case WolfGait::Stand:
    break;
  }
  return GaitPlan{};
}

auto leg_rests() noexcept -> const std::array<LegRest, k_leg_count>& {
  static const std::array<LegRest, k_leg_count> rests = [] {
    std::array<LegRest, k_leg_count> out{};
    for (std::size_t i = 0; i < k_leg_count; ++i) {
      const LegPlan& plan = k_leg_plans[i];
      const LegRestPlan& r = plan.hind ? k_hind_rest : k_fore_rest;
      out[i] = make_leg_rest({plan.x, r.hip_y, r.hip_z},
                             {plan.x, r.knee_y, r.knee_z},
                             {plan.x, r.ankle_y, r.ankle_z},
                             {plan.x, r.toe_y, r.toe_z},
                             plan.phase_offset);
    }
    return out;
  }();
  return rests;
}

auto oval_width_fraction(float height) -> float {
  constexpr std::array<std::pair<float, float>, 6> k_profile{{{1.00F, 0.00F},
                                                              {0.85F, 0.45F},
                                                              {0.50F, 0.85F},
                                                              {-0.05F, 1.00F},
                                                              {-0.70F, 0.65F},
                                                              {-1.00F, 0.00F}}};
  float const h = std::clamp(height, -1.0F, 1.0F);
  for (std::size_t i = 1; i < k_profile.size(); ++i) {
    const auto& [hi_h, hi_w] = k_profile[i - 1U];
    const auto& [lo_h, lo_w] = k_profile[i];
    if (h >= lo_h) {
      float const t = (h - lo_h) / (hi_h - lo_h);
      return lo_w + ((hi_w - lo_w) * t);
    }
  }
  return 0.0F;
}

auto saddle_rings() -> std::vector<Render::Creature::Quadruped::BarrelRing> {
  std::vector<Render::Creature::Quadruped::BarrelRing> rings;
  rings.reserve(k_body_rings.size());
  for (std::size_t i = 0; i < k_body_rings.size(); ++i) {
    const BodyRing& body = k_body_rings[i];
    float const centre = body.y + ((body.top - body.bottom) * 0.5F);
    float const radius = (body.top + body.bottom) * 0.5F;
    float const top_y = body.y + body.top + k_saddle_relief;
    float const bottom_y = centre + (radius * k_saddle_reach[i]);
    float const shell_centre = (top_y + bottom_y) * 0.5F;
    float const shell_radius = (top_y - bottom_y) * 0.5F;
    float const widest = shell_centre - (shell_radius * 0.05F);
    float const half_width =
        (body.half_width * oval_width_fraction((widest - centre) / radius)) +
        k_saddle_relief;
    rings.push_back({body.z,
                     shell_centre,
                     half_width,
                     top_y - shell_centre,
                     shell_centre - bottom_y});
  }
  return rings;
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

auto body_motion(const WolfDrive& drive) -> BodyMotion {
  BodyMotion m;
  if (drive.gait == WolfGait::Stand) {
    return m;
  }
  float const duty = gait_plan(drive.gait).stance_duty;
  float const phase = drive.stride_phase;

  if (drive.gait == WolfGait::Run) {
    float const effort = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
    m.fore_mid = (duty * 0.5F) - ((k_run_leg_phase[0] + k_run_leg_phase[1]) * 0.5F);
    m.hind_mid = (duty * 0.5F) - ((k_run_leg_phase[2] + k_run_leg_phase[3]) * 0.5F);

    float const flight = std::cos((phase - m.fore_mid - 0.25F) * k_two_pi * 2.0F) *
                         k_run_flight * effort;
    m.fore = flight - (std::cos((phase - m.fore_mid) * k_two_pi) * k_run_rock * effort);
    m.hind = flight - (std::cos((phase - m.hind_mid) * k_two_pi) * k_run_rock * effort);
    return m;
  }

  bool const stalking = drive.gait == WolfGait::Stalk;
  float const bob = stalking ? k_stalk_bob : k_walk_bob;
  m.fore_mid = (duty * 0.5F) - k_walk_leg_phase[0];
  m.hind_mid = (duty * 0.5F) - k_walk_leg_phase[2];

  m.fore = std::cos((phase - m.fore_mid) * k_two_pi * 2.0F) * bob;
  m.hind = std::cos((phase - m.hind_mid) * k_two_pi * 2.0F) * bob;

  float const left_support = (m.fore_mid + m.hind_mid) * 0.5F;
  m.sway = -std::cos((phase - left_support) * k_two_pi) *
           (stalking ? k_stalk_sway : k_walk_sway);
  return m;
}

void fill_legs(RigPose& pose, const WolfDrive& drive, const BodyMotion& motion) {
  const GaitPlan plan = gait_plan(drive.gait);
  float const weight = drive.gait == WolfGait::Stand ? 0.0F : 1.0F;
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    LegRest rest = leg_rests()[i];
    rest.hip.setY(rest.hip.y() + (k_leg_plans[i].hind ? motion.hind : motion.fore));
    rest.hip.setX(rest.hip.x() + motion.sway);
    solve_leg(rest,
              plan,
              drive.stride_phase + leg_phase_offset(drive.gait, i),
              weight,
              pose.legs[i]);
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

void fill_head(RigPose& pose, const WolfDrive& drive) {
  float const carriage =
      drive.gait == WolfGait::Run
          ? k_run_carriage * std::clamp(drive.speed_ratio, 0.0F, 1.0F)
          : 0.0F;
  float const lower =
      std::clamp(drive.crouch + (drive.lunge * 0.35F) + carriage, 0.0F, 1.0F);
  QVector3D const root = k_neck_root;
  float const neck_length = (k_poll_high - k_neck_root).length();
  QVector3D const poll =
      root + ((lerp(k_poll_high, k_poll_low, lower) - root).normalized() * neck_length);
  QVector3D const control = lerp(k_neck_bend_high, k_neck_bend_low, lower);

  pose.withers = root;
  pose.poll = poll;

  QVector3D const facing = (poll - bezier(root, control, poll, 0.74F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();
  QVector3D const muzzle_dir = (facing - (head_up * 0.16F)).normalized();
  pose.muzzle = poll + (facing * 0.080F) + (muzzle_dir * 0.115F);
  pose.jaw_hinge = poll + (facing * 0.066F) - (head_up * 0.032F);
  pose.jaw_tip = pose.muzzle - (head_up * 0.026F);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll + (head_up * 0.058F) + (side * (sign * 0.058F)) - (facing * 0.020F);
    QVector3D const erect =
        ((head_up * 0.90F) + (side * (sign * 0.30F)) + (facing * 0.14F)).normalized();
    QVector3D const pinned =
        ((head_up * 0.30F) + (side * (sign * 0.52F)) - (facing * 0.80F)).normalized();
    QVector3D const dir = lerp(erect, pinned, drive.ear_pin).normalized();
    QVector3D const tip = base + (dir * (0.118F - (drive.ear_pin * 0.032F)));
    if (sign_index == 0) {
      pose.ear_base_l = base;
      pose.ear_tip_l = tip;
    } else {
      pose.ear_base_r = base;
      pose.ear_tip_r = tip;
    }
  }
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

auto rotate_about_z(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D {
  QVector3D const d = point - pivot;
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return pivot + QVector3D((d.x() * c) - (d.y() * s), (d.x() * s) + (d.y() * c), d.z());
}

void apply_rear(RigPose& pose, const WolfDrive& drive) {
  float const rear = std::clamp(drive.rear, 0.0F, 1.0F);
  if (rear <= 0.0F) {
    return;
  }

  QVector3D const lift(0.0F, 0.072F * rear, 0.020F * rear);
  QVector3D const pivot = pose.body_rear;
  float const pitch = -0.19F * rear;

  auto raise = [&](QVector3D& point) {
    point += lift;
    point = rotate_about_x(point, pivot, pitch);
  };

  pose.root += lift * 0.45F;
  pose.root = rotate_about_x(pose.root, pivot, pitch * 0.45F);
  raise(pose.body_front);
  raise(pose.withers);
  for (auto* point : head_chain(pose)) {
    raise(*point);
  }

  for (std::size_t i = 0; i < 2U; ++i) {

    pose.legs[i].shoulder += lift * 0.95F;
    pose.legs[i].knee += lift * 0.80F + QVector3D(0.0F, 0.0F, 0.052F * rear);
    pose.legs[i].foot += lift * 0.62F + QVector3D(0.0F, 0.0F, 0.070F * rear);
    pose.legs[i].toe += lift * 0.55F + QVector3D(0.0F, 0.0F, 0.078F * rear);
  }
  for (std::size_t i = 2U; i < k_leg_count; ++i) {

    pose.legs[i].shoulder += QVector3D(0.0F, -0.022F * rear, 0.0F);
    pose.legs[i].knee += QVector3D(0.0F, -0.016F * rear, -0.020F * rear);
  }

  pose.tail_base += lift * 0.35F;
  pose.tail_mid += QVector3D(0.0F, 0.030F * rear, -0.024F * rear);
  pose.tail_tip += QVector3D(0.0F, 0.058F * rear, -0.046F * rear);
}

void apply_lunge(RigPose& pose, const WolfDrive& drive) {
  float const lunge = std::clamp(drive.lunge, 0.0F, 1.0F);
  if (lunge <= 0.0F && drive.jaw_open <= 0.0F && drive.head_shake == 0.0F &&
      drive.head_roll == 0.0F && drive.rear == 0.0F) {
    return;
  }

  QVector3D const surge(0.0F, -0.040F * lunge, 0.300F * lunge);
  QVector3D const front_reach(0.0F, -0.048F * lunge, 0.086F * lunge);

  pose.root += surge;
  pose.body_rear += surge + QVector3D(0.0F, 0.058F * lunge, -0.030F * lunge);
  pose.body_front += surge + (front_reach * 0.6F);

  for (auto& leg : pose.legs) {
    leg.shoulder += surge;
    leg.knee += surge;
    leg.foot += surge;
    leg.toe += surge;
  }
  QVector3D const brace(0.0F, 0.0F, 0.110F * lunge);
  for (std::size_t i = 0; i < 2U; ++i) {
    pose.legs[i].knee += brace * 0.45F;
    pose.legs[i].foot += brace;
    pose.legs[i].toe += brace;
  }

  QVector3D const head_move = surge + front_reach;
  pose.withers += head_move;
  pose.poll += head_move;
  pose.muzzle += head_move;
  pose.jaw_hinge += head_move;
  pose.jaw_tip += head_move;
  pose.ear_base_l += head_move;
  pose.ear_base_r += head_move;
  pose.ear_tip_l += head_move;
  pose.ear_tip_r += head_move;

  float const swing = 0.42F * lunge;
  QVector3D const pivot = pose.withers;
  pose.poll = rotate_about_x(pose.poll, pivot, swing);
  pose.muzzle = rotate_about_x(pose.muzzle, pivot, swing);
  pose.jaw_hinge = rotate_about_x(pose.jaw_hinge, pivot, swing);
  pose.jaw_tip = rotate_about_x(pose.jaw_tip, pivot, swing);
  pose.ear_base_l = rotate_about_x(pose.ear_base_l, pivot, swing);
  pose.ear_base_r = rotate_about_x(pose.ear_base_r, pivot, swing);
  pose.ear_tip_l = rotate_about_x(pose.ear_tip_l, pivot, swing);
  pose.ear_tip_r = rotate_about_x(pose.ear_tip_r, pivot, swing);

  apply_rear(pose, drive);

  float const gape = std::clamp(drive.jaw_open, 0.0F, 1.0F);
  if (gape > 0.0F) {

    pose.muzzle = rotate_about_x(pose.muzzle, pose.poll, -0.45F * gape);
  }
  pose.jaw_tip = rotate_about_x(pose.jaw_tip, pose.jaw_hinge, 0.95F * gape);

  if (drive.head_roll != 0.0F) {

    float const roll = 0.62F * drive.head_roll;
    QVector3D const neck = pose.withers;
    for (auto* point : head_chain(pose)) {
      *point = rotate_about_z(*point, neck, roll);
    }
  }

  if (drive.head_shake != 0.0F) {
    float const counter = -0.05F * drive.head_shake;
    QVector3D const spine_pivot = pose.body_rear;
    pose.body_front = rotate_about_y(pose.body_front, spine_pivot, counter);
    pose.withers = rotate_about_y(pose.withers, spine_pivot, counter);
    for (auto* point : head_chain(pose)) {
      *point = rotate_about_y(*point, spine_pivot, counter);
    }
    for (std::size_t i = 0; i < 2U; ++i) {
      pose.legs[i].shoulder =
          rotate_about_y(pose.legs[i].shoulder, spine_pivot, counter);
      pose.legs[i].knee = rotate_about_y(pose.legs[i].knee, spine_pivot, counter);
    }

    float const yaw = 0.80F * drive.head_shake;
    QVector3D const neck = pose.withers;
    for (auto* point : head_chain(pose)) {
      *point = rotate_about_y(*point, neck, yaw);
    }
  }

  pose.tail_base += surge;
  pose.tail_mid += surge;
  pose.tail_tip += surge;
  float const tail_swing = -0.40F * lunge;
  pose.tail_mid = rotate_about_x(pose.tail_mid, pose.tail_base, tail_swing);
  pose.tail_tip = rotate_about_x(pose.tail_tip, pose.tail_base, tail_swing);
}

auto capture_head_attachment(const RigPose& pose) -> HeadAttachment {
  HeadAttachment held;
  held.neck_length = (pose.poll - pose.withers).length();
  held.muzzle = (pose.muzzle - pose.poll).length();
  held.jaw_hinge = (pose.jaw_hinge - pose.poll).length();
  held.jaw = (pose.jaw_tip - pose.jaw_hinge).length();
  held.ear_base_l = (pose.ear_base_l - pose.poll).length();
  held.ear_l = (pose.ear_tip_l - pose.ear_base_l).length();
  held.ear_base_r = (pose.ear_base_r - pose.poll).length();
  held.ear_r = (pose.ear_tip_r - pose.ear_base_r).length();
  return held;
}

void reattach_head(RigPose& pose, const HeadAttachment& held) {
  hold_bone(pose.withers, pose.poll, held.neck_length);
  hold_bone(pose.poll, pose.muzzle, held.muzzle);
  hold_bone(pose.poll, pose.jaw_hinge, held.jaw_hinge);
  hold_bone(pose.jaw_hinge, pose.jaw_tip, held.jaw);
  hold_bone(pose.poll, pose.ear_base_l, held.ear_base_l);
  hold_bone(pose.ear_base_l, pose.ear_tip_l, held.ear_l);
  hold_bone(pose.poll, pose.ear_base_r, held.ear_base_r);
  hold_bone(pose.ear_base_r, pose.ear_tip_r, held.ear_r);
}

void apply_collapse(RigPose& pose, const WolfDrive& drive) {
  float const phase = std::clamp(drive.collapse, 0.0F, 1.0F);
  if (phase <= 0.0F) {
    return;
  }

  DeathMotion const m = death_motion(phase);

  constexpr float k_lying_y = 0.150F;
  constexpr float k_rest_body_y = 0.505F;
  constexpr float k_roll_radians = -1.42F;

  float const descent = (k_rest_body_y - k_lying_y) * m.fall;
  QVector3D const spine(0.0F, k_lying_y, 0.0F);
  float const roll = k_roll_radians * m.roll;

  float const head_descent = (k_rest_body_y - k_lying_y) * m.head;

  auto settle_bounce = [&](float weight) {
    return m.settle * 0.022F * weight;
  };

  auto place = [&](QVector3D& p, float descent_amount, float bounce_weight) {
    p.setY(p.y() - descent_amount + settle_bounce(bounce_weight));
    p = roll_about_spine(p, spine, roll);
    p.setY(std::max(p.y(), 0.018F));
  };

  place(pose.root, descent * 0.92F, 0.5F);
  place(pose.body_rear, descent, 1.0F);
  place(pose.body_front, descent, 0.9F);
  place(pose.withers, descent, 0.8F);

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    float const out = (i % 2U == 0U) ? -1.0F : 1.0F;
    float const fold = m.buckle;
    float const twitch = m.thrash * 0.030F * out;

    pose.legs[i].knee += QVector3D(0.0F, -0.055F * fold, 0.030F * fold * out);
    pose.legs[i].foot += QVector3D(0.0F, -0.090F * fold, 0.062F * fold * out + twitch);
    pose.legs[i].toe += QVector3D(0.0F, -0.100F * fold, 0.076F * fold * out + twitch);

    place(pose.legs[i].shoulder, descent, 0.9F);
    place(pose.legs[i].knee, descent, 0.7F);
    place(pose.legs[i].foot, descent, 0.5F);
    place(pose.legs[i].toe, descent, 0.4F);

    float const curl = (k_leg_plans[i].hind ? 0.50F : 0.36F) * m.roll;
    pose.legs[i].foot = roll_about_spine(pose.legs[i].foot, pose.legs[i].knee, curl);
    pose.legs[i].toe = roll_about_spine(pose.legs[i].toe, pose.legs[i].knee, curl);
    pose.legs[i].toe =
        roll_about_spine(pose.legs[i].toe, pose.legs[i].foot, curl * 0.8F);
    pose.legs[i].foot.setY(std::max(pose.legs[i].foot.y(), 0.020F));
    pose.legs[i].toe.setY(std::max(pose.legs[i].toe.y(), 0.020F));
  }

  place(pose.poll, head_descent, 0.7F);
  place(pose.muzzle, head_descent, 0.6F);
  pose.muzzle += QVector3D(0.0F, 0.0F, 0.055F * m.head);
  place(pose.jaw_hinge, head_descent, 0.6F);
  place(pose.jaw_tip, head_descent, 0.5F);

  pose.jaw_tip += QVector3D(0.0F, -0.022F * m.head, 0.014F * m.head);

  place(pose.ear_base_l, head_descent, 0.5F);
  place(pose.ear_base_r, head_descent, 0.5F);
  place(pose.ear_tip_l, head_descent, 0.4F);
  place(pose.ear_tip_r, head_descent, 0.4F);

  float const tail_lag = std::clamp(m.head * 1.05F, 0.0F, 1.0F);
  float const tail_flick = m.thrash * 0.045F;
  pose.tail_mid += QVector3D(tail_flick, 0.0F, 0.0F);
  pose.tail_tip += QVector3D(tail_flick * 1.6F, 0.0F, 0.0F);
  place(pose.tail_base, descent, 0.8F);
  place(pose.tail_mid, (k_rest_body_y - k_lying_y) * tail_lag, 0.6F);
  place(pose.tail_tip, (k_rest_body_y - k_lying_y) * tail_lag, 0.5F);
}

void apply_idle_motion(RigPose& pose, const WolfDrive& drive) {
  if (drive.breath == 0.0F && drive.head_turn == 0.0F && drive.head_dip == 0.0F &&
      drive.ear_swivel == 0.0F && drive.tail_sway == 0.0F &&
      drive.weight_shift == 0.0F) {
    return;
  }

  QVector3D const swell(0.0F, 0.013F * drive.breath, 0.0F);
  float const shift = 0.019F * drive.weight_shift;

  auto ride = [&](QVector3D& point, float swell_weight, float shift_weight) {
    point += swell * swell_weight;
    point.setX(point.x() + (shift * shift_weight));
  };

  ride(pose.root, 0.35F, 0.55F);
  ride(pose.body_front, 1.0F, 0.70F);
  ride(pose.body_rear, 0.62F, 1.0F);
  ride(pose.withers, 0.90F, 0.70F);
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    bool const hind = k_leg_plans[i].hind;
    ride(pose.legs[i].shoulder, hind ? 0.45F : 0.85F, hind ? 1.0F : 0.65F);
    ride(pose.legs[i].knee, hind ? 0.18F : 0.34F, hind ? 0.42F : 0.26F);
  }

  QVector3D const neck = pose.withers;
  float const yaw = 0.34F * drive.head_turn;
  float const pitch = 0.20F * drive.head_dip;
  for (auto* point : head_chain(pose)) {
    *point += swell * 0.85F;
    point->setX(point->x() + (shift * 0.72F));
    *point = rotate_about_y(*point, neck, yaw);
    *point = rotate_about_x(*point, neck, pitch);
  }

  float const swivel = 0.62F * drive.ear_swivel;
  pose.ear_tip_l = rotate_about_y(pose.ear_tip_l, pose.ear_base_l, swivel);
  pose.ear_tip_r = rotate_about_y(pose.ear_tip_r, pose.ear_base_r, -swivel);

  ride(pose.tail_base, 0.55F, 1.0F);
  pose.tail_mid += swell * 0.4F;
  pose.tail_tip += swell * 0.3F;
  pose.tail_mid.setX(pose.tail_mid.x() + (0.038F * drive.tail_sway) + shift);
  pose.tail_tip.setX(pose.tail_tip.x() + (0.082F * drive.tail_sway) + shift);
}

auto make_pose(const WolfDrive& drive) -> RigPose {
  RigPose pose;
  float const crouch = drive.crouch * 0.048F;
  float const effort = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
  bool const running = drive.gait == WolfGait::Run;
  bool const moving = drive.gait != WolfGait::Stand;

  BodyMotion motion = body_motion(drive);
  motion.fore -= crouch;
  motion.hind -= crouch;
  float const bob = (motion.fore + motion.hind) * 0.5F;
  QVector3D const shift(motion.sway, 0.0F, 0.0F);

  pose.root = QVector3D(motion.sway * 0.5F, bob, 0.0F);
  pose.body_rear = QVector3D(motion.sway, 0.512F + motion.hind, -0.380F);
  pose.body_front = QVector3D(motion.sway, 0.498F + motion.fore, 0.340F);

  fill_legs(pose, drive, motion);
  fill_head(pose, drive);
  HeadAttachment const head_attachment = capture_head_attachment(pose);

  float nod = 0.0F;
  if (running) {
    nod = -std::cos(((drive.stride_phase - motion.fore_mid) * k_two_pi) - 0.9F) *
          0.012F * effort;
  } else if (moving) {
    nod = std::cos(((drive.stride_phase - motion.fore_mid) * k_two_pi * 2.0F) - 1.0F) *
          (drive.gait == WolfGait::Stalk ? 0.002F : 0.010F);
  }
  QVector3D const head_ride =
      (shift * 0.6F) + QVector3D(0.0F, (motion.fore * 0.5F) + nod, 0.0F);
  pose.withers += shift + QVector3D(0.0F, motion.fore * 0.85F, 0.0F);
  for (auto* point : head_chain(pose)) {
    *point += head_ride;
  }

  float const wag_gain = moving ? 0.35F + (effort * 0.65F) : 0.0F;
  float const wag_mid = std::sin(drive.stride_phase * k_two_pi) * 0.028F * wag_gain;
  float const wag_tip =
      std::sin((drive.stride_phase * k_two_pi) - 0.9F) * 0.060F * wag_gain;
  float const stream = running ? effort : 0.0F;
  float const tuck = drive.crouch;
  float const whip = (drive.stride_phase - motion.hind_mid) * k_two_pi;
  float const bounce_mid = std::cos(whip - 1.2F) * 0.012F * stream;
  float const bounce_tip = std::cos(whip - 2.2F) * 0.030F * stream;

  QVector3D const tail_ride = shift + QVector3D(0.0F, motion.hind, 0.0F);
  pose.tail_base = k_tail_base + tail_ride;
  pose.tail_mid = k_tail_mid + tail_ride;
  pose.tail_tip = k_tail_tip + tail_ride;
  SkeletonLengths const skeleton = capture_skeleton_lengths(pose);
  pose.tail_mid +=
      QVector3D(wag_mid, (0.034F * stream) - (tuck * 0.035F) + bounce_mid, 0.0F);
  pose.tail_tip +=
      QVector3D(wag_tip, (0.150F * stream) - (tuck * 0.070F) + bounce_tip, 0.0F);

  apply_idle_motion(pose, drive);
  auto const planted_legs = pose.legs;
  apply_lunge(pose, drive);
  apply_collapse(pose, drive);
  reattach_head(pose, head_attachment);
  enforce_skeleton_lengths(pose, skeleton);
  if (drive.gait == WolfGait::Stand && drive.lunge > 0.0F && drive.collapse <= 0.0F) {

    for (std::size_t i = 2; i < k_leg_count; ++i) {
      auto const& leg = planted_legs[i];
      auto rest = make_leg_rest(leg.shoulder, leg.knee, leg.foot, leg.toe, 0.0F);
      rest.hip = pose.legs[i].shoulder;
      solve_leg(rest, GaitPlan{}, 0.0F, 0.0F, pose.legs[i]);
    }
  }
  return pose;
}

} // namespace wolf_spec_detail

using namespace wolf_spec_detail;

auto wolf_gait_advance(WolfGait gait) noexcept -> float {
  return gait_advance(gait_plan(gait));
}

auto wolf_bind_pose() noexcept -> const RigPose& {
  static const RigPose pose = make_pose(WolfDrive{});
  return pose;
}

auto wolf_bite_drive(float phase) noexcept -> WolfDrive {
  auto window = [phase](float start, float end) {
    float const t = std::clamp((phase - start) / (end - start), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  constexpr float contact = Engine::Core::WildlifeComponent::k_bite_impact_phase;
  float const coil = window(0.0F, 0.14F);
  float const strike = window(0.14F, contact);
  float const release = window(0.52F, 1.0F);
  float const grip = window(contact, 0.36F) * (1.0F - window(0.52F, 0.76F));

  WolfDrive drive;
  drive.gait = WolfGait::Stand;
  drive.ear_pin = 1.0F;
  drive.crouch =
      0.55F + 0.35F * coil * (1.0F - strike) - 0.20F * strike * (1.0F - release);
  drive.lunge = strike * (1.0F - release);
  drive.jaw_open = coil * (1.0F - strike);
  drive.head_dip = -0.6F * coil * (1.0F - strike);
  drive.rear = 0.18F * strike * (1.0F - release);
  drive.head_shake = std::sin((phase - contact) * k_two_pi * 3.0F) * 0.5F * grip;
  drive.head_roll = -0.20F * grip;
  return drive;
}

auto wolf_pose(const WolfDrive& drive) noexcept -> RigPose {
  return make_pose(drive);
}

auto wolf_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const BonePalette palette = [] {
    BonePalette out{};
    evaluate_wildlife_skeleton(wolf_bind_pose(), out);
    return out;
  }();
  return {palette.data(), palette.size()};
}

auto wolf_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_full);
  return {nodes.data(), nodes.size()};
}

auto wolf_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_minimal);
  return {nodes.data(), nodes.size()};
}

namespace {

auto lod_geometry_slot() noexcept -> Render::Creature::CreatureLodGeometrySlot& {
  static Render::Creature::CreatureLodGeometrySlot slot(&wolf_runtime_manifest, "wolf");
  return slot;
}

} // namespace

void initialize_wolf_asset() {
  lod_geometry_slot().initialize();
}

auto wolf_creature_spec() noexcept -> const Render::Creature::CreatureSpec& {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "wolf";
    s.topology = wildlife_topology();
    s.lod_full = lod_geometry_slot().get().full.part_graph();
    s.lod_minimal = lod_geometry_slot().get().minimal.part_graph();
    return s;
  }();
  return spec;
}

} // namespace Render::Wildlife
