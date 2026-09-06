#include "wolf_spec.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "render/creature/assets/creature_lod_geometry.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "wildlife_gait.h"
#include "wolf_manifest.h"

namespace Render::Wildlife {

namespace {

using Render::Creature::Quadruped::ConeNode;
using Render::Creature::Quadruped::EllipsoidNode;
using Render::Creature::Quadruped::MeshNode;
using Render::Creature::Quadruped::SnoutNode;
using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;

constexpr float k_fore_half_width = 0.088F;
constexpr float k_hind_half_width = 0.094F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

constexpr std::array<BodyRing, 14> k_body_rings{{
    {0.396F, 0.466F, 0.028F, 0.044F, 0.046F},
    {0.372F, 0.470F, 0.062F, 0.088F, 0.090F},
    {0.320F, 0.486F, 0.104F, 0.122F, 0.140F},
    {0.258F, 0.498F, 0.136F, 0.152F, 0.166F},
    {0.186F, 0.504F, 0.146F, 0.148F, 0.180F},
    {0.108F, 0.506F, 0.140F, 0.140F, 0.172F},
    {0.026F, 0.508F, 0.118F, 0.130F, 0.148F},
    {-0.056F, 0.510F, 0.096F, 0.126F, 0.114F},
    {-0.140F, 0.512F, 0.094F, 0.126F, 0.098F},
    {-0.222F, 0.516F, 0.122F, 0.128F, 0.108F},
    {-0.300F, 0.518F, 0.136F, 0.126F, 0.122F},
    {-0.368F, 0.516F, 0.110F, 0.110F, 0.106F},
    {-0.420F, 0.512F, 0.052F, 0.070F, 0.062F},
    {-0.444F, 0.510F, 0.024F, 0.034F, 0.030F},
}};

constexpr std::array<float, k_body_rings.size()> k_saddle_reach{{0.62F,
                                                                 0.50F,
                                                                 0.30F,
                                                                 0.10F,
                                                                 0.02F,
                                                                 0.10F,
                                                                 0.04F,
                                                                 0.14F,
                                                                 0.06F,
                                                                 0.02F,
                                                                 0.10F,
                                                                 0.26F,
                                                                 0.52F,
                                                                 0.70F}};

constexpr float k_saddle_relief = 0.012F;

struct LegPlan {
  float x;
  float phase_offset;
  bool hind;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-k_fore_half_width, 0.0F, false},
    {k_fore_half_width, 0.5F, false},
    {-k_hind_half_width, 0.5F, true},
    {k_hind_half_width, 0.0F, true},
}};

struct LegRestPlan {
  float knee_y;
  float knee_z;
  float ankle_y;
  float ankle_z;
  float toe_y;
  float toe_z;
  float hip_y;
  float hip_z;
};

constexpr LegRestPlan k_fore_rest{
    0.244F, 0.272F, 0.108F, 0.216F, 0.024F, 0.236F, 0.402F, 0.222F};
constexpr LegRestPlan k_hind_rest{
    0.286F, -0.140F, 0.134F, -0.256F, 0.024F, -0.220F, 0.438F, -0.222F};

constexpr float k_bob_base = 0.014F;
constexpr float k_bob_gain = 0.040F;
constexpr float k_pitch_lag = 0.90F;
constexpr float k_pitch_gain = 0.030F;

constexpr std::array<float, k_leg_count> k_walk_leg_phase{{0.25F, 0.75F, 0.0F, 0.5F}};
constexpr std::array<float, k_leg_count> k_run_leg_phase{{0.0F, 0.12F, 0.48F, 0.60F}};

auto leg_phase_offset(WolfGait gait, std::size_t leg) -> float {
  return gait == WolfGait::Run ? k_run_leg_phase[leg] : k_walk_leg_phase[leg];
}

constexpr GaitPlan k_gait_stalk{0.240F, 0.70F, 0.026F, 0.048F};
constexpr GaitPlan k_gait_walk{0.400F, 0.62F, 0.052F, 0.092F};
constexpr GaitPlan k_gait_run{0.520F, 0.38F, 0.104F, 0.140F};

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

void fill_legs(RigPose& pose,
               const WolfDrive& drive,
               float fore_lift,
               float hind_lift) {
  const GaitPlan plan = gait_plan(drive.gait);
  float const weight = drive.gait == WolfGait::Stand ? 0.0F : 1.0F;
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    LegRest rest = leg_rests()[i];
    rest.hip.setY(rest.hip.y() + (k_leg_plans[i].hind ? hind_lift : fore_lift));
    solve_leg(rest,
              plan,
              drive.stride_phase + leg_phase_offset(drive.gait, i),
              weight,
              pose.legs[i]);
  }
}

void fill_head(RigPose& pose, const WolfDrive& drive) {
  float const lower = std::clamp(drive.crouch + (drive.lunge * 0.35F), 0.0F, 1.0F);
  QVector3D const root(0.0F, 0.560F, 0.320F);
  QVector3D const poll =
      lerp(QVector3D(0.0F, 0.628F, 0.470F), QVector3D(0.0F, 0.500F, 0.500F), lower);
  QVector3D const control =
      lerp(QVector3D(0.0F, 0.640F, 0.372F), QVector3D(0.0F, 0.560F, 0.410F), lower);

  pose.withers = root;
  pose.poll = poll;

  QVector3D const facing = (poll - bezier(root, control, poll, 0.74F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();
  QVector3D const muzzle_dir = (facing - (head_up * 0.16F)).normalized();
  pose.muzzle = poll + (facing * 0.068F) + (muzzle_dir * 0.134F);
  pose.jaw_hinge = poll + (facing * 0.062F) - (head_up * 0.020F);
  pose.jaw_tip = pose.muzzle - (head_up * 0.018F);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll + (head_up * 0.050F) + (side * (sign * 0.046F)) - (facing * 0.016F);
    QVector3D const erect =
        ((head_up * 0.90F) + (side * (sign * 0.30F)) + (facing * 0.14F)).normalized();
    QVector3D const pinned =
        ((head_up * 0.30F) + (side * (sign * 0.52F)) - (facing * 0.80F)).normalized();
    QVector3D const dir = lerp(erect, pinned, drive.ear_pin).normalized();
    QVector3D const tip = base + (dir * (0.105F - (drive.ear_pin * 0.020F)));
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

struct HeadAttachment {
  float neck_length{0.0F};
  float muzzle{0.0F};
  float jaw_hinge{0.0F};
  float jaw{0.0F};
  float ear_base_l{0.0F};
  float ear_l{0.0F};
  float ear_base_r{0.0F};
  float ear_r{0.0F};
};

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
  float const cadence = drive.stride_phase * k_two_pi * 2.0F;
  float const gallop = std::clamp(drive.speed_ratio, 0.0F, 1.0F);
  float const bob = (std::sin(cadence) * (k_bob_base + (k_bob_gain * gallop))) - crouch;
  float const pitch = std::sin(cadence - k_pitch_lag) * k_pitch_gain * gallop;

  pose.root = QVector3D(0.0F, bob, 0.0F);
  pose.body_rear = QVector3D(0.0F, 0.512F + bob + pitch, -0.380F);
  pose.body_front = QVector3D(0.0F, 0.498F + bob - pitch, 0.340F);

  fill_legs(pose, drive, bob - pitch, bob + pitch);
  fill_head(pose, drive);
  HeadAttachment const head_attachment = capture_head_attachment(pose);

  float const nod =
      std::sin(cadence - 1.25F) * (k_bob_base + (k_bob_gain * gallop)) * 0.58F;
  QVector3D const head_ride(0.0F, (bob * 0.55F) + nod, 0.0F);
  pose.withers += QVector3D(0.0F, (bob * 0.85F) - (pitch * 0.6F), 0.0F);
  pose.poll += head_ride;
  pose.muzzle += head_ride;
  pose.jaw_hinge += head_ride;
  pose.jaw_tip += head_ride;
  pose.ear_base_l += head_ride;
  pose.ear_base_r += head_ride;
  pose.ear_tip_l += head_ride;
  pose.ear_tip_r += head_ride;

  float const sway = std::sin(drive.stride_phase * k_two_pi) * 0.040F *
                     (0.35F + (drive.speed_ratio * 0.65F));
  float const lift = drive.crouch;

  float const tail_bounce = std::sin(cadence - 2.1F) * 0.030F * gallop;
  pose.tail_base = QVector3D(0.0F, 0.556F + bob + pitch, -0.412F);
  pose.tail_mid = QVector3D(0.0F, 0.512F + bob + pitch, -0.606F);
  pose.tail_tip = QVector3D(0.0F, 0.392F + bob + pitch, -0.782F);
  SkeletonLengths const skeleton = capture_skeleton_lengths(pose);
  pose.tail_mid += QVector3D(sway * 0.5F, (lift * 0.140F) + tail_bounce, 0.0F);
  pose.tail_tip += QVector3D(sway, (lift * 0.320F) + (tail_bounce * 1.8F), 0.0F);

  apply_idle_motion(pose, drive);
  apply_lunge(pose, drive);
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

auto cone(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& base_center,
          const QVector3D& tip,
          float base_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  ConeNode data;
  data.base_center = base_center;
  data.tip = tip;
  data.base_radius = base_radius;
  node.data = data;
  return node;
}

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode> {
  g_minimal_tessellation = wanted_lod == Render::Creature::k_lod_minimal;
  const RigPose& bind = wolf_bind_pose();
  std::vector<MeshNode> nodes;
  nodes.reserve(56U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  std::vector<Render::Creature::Quadruped::BarrelRing> body_rings;
  body_rings.reserve(k_body_rings.size());
  for (const BodyRing& ring : k_body_rings) {
    body_rings.push_back({ring.z, ring.y, ring.half_width, ring.top, ring.bottom});
  }
  nodes.push_back(
      barrel("wolf.body", Bone::Body, k_wolf_role_fur, std::move(body_rings)));
  nodes.push_back(
      barrel("wolf.saddle", Bone::Body, k_wolf_role_saddle, saddle_rings()));
  nodes.push_back(ellipsoid("wolf.withers",
                            Bone::Body,
                            k_wolf_role_saddle,
                            {0.0F, 0.596F, 0.212F},
                            {0.072F, 0.048F, 0.110F}));
  nodes.push_back(ellipsoid("wolf.chest",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.344F, 0.190F},
                            {0.064F, 0.032F, 0.150F}));
  nodes.push_back(ellipsoid("wolf.belly",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.418F, -0.100F},
                            {0.052F, 0.028F, 0.170F}));
  nodes.push_back(ellipsoid("wolf.blaze",
                            Bone::Body,
                            k_wolf_role_cream,
                            {0.0F, 0.404F, 0.352F},
                            {0.052F, 0.058F, 0.038F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("wolf.neck.lower",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       bind.withers,
                       neck_mid,
                       0.098F,
                       0.078F));
  nodes.push_back(tube("wolf.neck.upper",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       neck_mid,
                       bind.poll,
                       0.078F,
                       0.062F));

  nodes.push_back(ellipsoid("wolf.ruff",
                            Bone::NeckTop,
                            k_wolf_role_fur,
                            neck_mid - QVector3D(0.0F, 0.008F, 0.022F),
                            {0.106F, 0.100F, 0.096F}));
  nodes.push_back(ellipsoid("wolf.mane",
                            Bone::NeckTop,
                            k_wolf_role_saddle,
                            neck_mid + QVector3D(0.0F, 0.042F, -0.016F),
                            {0.082F, 0.062F, 0.088F}));
  nodes.push_back(ellipsoid("wolf.throat",
                            Bone::NeckTop,
                            k_wolf_role_cream,
                            neck_mid + QVector3D(0.0F, -0.050F, 0.014F),
                            {0.056F, 0.040F, 0.056F}));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("wolf.skull",
                            Bone::Head,
                            k_wolf_role_fur,
                            bind.poll + (facing * 0.028F),
                            {0.060F, 0.062F, 0.078F}));
  nodes.push_back(ellipsoid("wolf.crown",
                            Bone::Head,
                            k_wolf_role_saddle,
                            bind.poll + (facing * 0.014F) + (head_up * 0.030F),
                            {0.056F, 0.030F, 0.066F}));
  {
    MeshNode node;
    node.debug_name = "wolf.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_wolf_role_pale;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.068F);
    data.end = bind.muzzle;
    data.base_radius = 0.046F;
    data.tip_radius = 0.026F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(tube("wolf.jaw",
                       Bone::Jaw,
                       k_wolf_role_pale,
                       bind.jaw_hinge,
                       bind.jaw_tip,
                       0.034F,
                       0.022F));
  nodes.push_back(tube("wolf.mouth",
                       Bone::Jaw,
                       k_wolf_role_nose,
                       bind.jaw_hinge + (head_up * 0.014F),
                       bind.jaw_tip + (head_up * 0.010F) - (facing * 0.014F),
                       0.024F,
                       0.013F,
                       k_full));
  nodes.push_back(tube("wolf.bridge",
                       Bone::Head,
                       k_wolf_role_saddle,
                       bind.poll + (facing * 0.062F) + (head_up * 0.024F),
                       bind.muzzle + (head_up * 0.014F),
                       0.030F,
                       0.018F));
  nodes.push_back(ellipsoid("wolf.nose",
                            Bone::Head,
                            k_wolf_role_nose,
                            bind.muzzle + (facing * 0.008F),
                            {0.024F, 0.024F, 0.024F}));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("wolf.cheek",
                              Bone::Head,
                              k_wolf_role_pale,
                              bind.poll + (facing * 0.036F) + (side * (sign * 0.048F)) -
                                  (head_up * 0.014F),
                              {0.028F, 0.040F, 0.060F},
                              k_full));
    nodes.push_back(ellipsoid("wolf.eye",
                              Bone::Head,
                              k_wolf_role_eye,
                              bind.poll + (facing * 0.052F) + (head_up * 0.030F) +
                                  (side * (sign * 0.043F)),
                              {0.0145F, 0.0145F, 0.0145F},
                              k_full));
    nodes.push_back(ellipsoid("wolf.iris",
                              Bone::Head,
                              k_wolf_role_iris,
                              bind.poll + (facing * 0.060F) + (head_up * 0.030F) +
                                  (side * (sign * 0.049F)),
                              {0.009F, 0.009F, 0.009F},
                              k_full));
  }

  nodes.push_back(cone("wolf.ear_l",
                       Bone::EarL,
                       k_wolf_role_saddle,
                       bind.ear_base_l,
                       bind.ear_tip_l,
                       0.044F));
  nodes.push_back(cone("wolf.ear_r",
                       Bone::EarR,
                       k_wolf_role_saddle,
                       bind.ear_base_r,
                       bind.ear_tip_r,
                       0.044F));
  nodes.push_back(cone("wolf.ear_inner_l",
                       Bone::EarL,
                       k_wolf_role_pale,
                       bind.ear_base_l + (facing * 0.012F),
                       bind.ear_tip_l + (facing * 0.010F),
                       0.026F,
                       k_full));
  nodes.push_back(cone("wolf.ear_inner_r",
                       Bone::EarR,
                       k_wolf_role_pale,
                       bind.ear_base_r + (facing * 0.012F),
                       bind.ear_tip_r + (facing * 0.010F),
                       0.026F,
                       k_full));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    bool const hind = k_leg_plans[i].hind;
    nodes.push_back(tube("wolf.leg.upper",
                         k_shoulders[i],
                         k_wolf_role_fur,
                         joints.shoulder,
                         joints.knee,
                         hind ? 0.052F : 0.046F,
                         0.032F));
    nodes.push_back(tube("wolf.leg.mid",
                         k_knees[i],
                         k_wolf_role_limb,
                         joints.knee,
                         joints.foot,
                         0.030F,
                         0.024F));
    nodes.push_back(tube("wolf.leg.lower",
                         k_feet[i],
                         k_wolf_role_limb,
                         joints.foot,
                         joints.toe,
                         0.024F,
                         0.021F));
    nodes.push_back(ellipsoid(
        "wolf.paw", k_feet[i], k_wolf_role_paw, joints.toe, {0.032F, 0.024F, 0.044F}));
    nodes.push_back(ellipsoid(
        "wolf.limb_mass",
        k_shoulders[i],
        k_wolf_role_fur,
        hind ? QVector3D(k_leg_plans[i].x * 0.86F, 0.400F, -0.222F)
             : QVector3D(k_leg_plans[i].x * 0.84F, 0.392F, 0.220F),
        hind ? QVector3D(0.056F, 0.100F, 0.096F) : QVector3D(0.048F, 0.086F, 0.078F)));
  }

  nodes.push_back(tube("wolf.tail.base",
                       Bone::TailBase,
                       k_wolf_role_fur,
                       bind.tail_base,
                       bind.tail_mid,
                       0.036F,
                       0.052F));
  nodes.push_back(tube("wolf.tail.tip",
                       Bone::TailTip,
                       k_wolf_role_saddle,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.052F,
                       0.028F));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  g_minimal_tessellation = false;
  return nodes;
}

} // namespace

auto wolf_gait_advance(WolfGait gait) noexcept -> float {
  return gait_advance(gait_plan(gait));
}

auto wolf_bind_pose() noexcept -> const RigPose& {
  static const RigPose pose = make_pose(WolfDrive{});
  return pose;
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
