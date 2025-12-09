#include "rig.h"

#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../humanoid/rig.h"
#include "../submitter.h"
#include "horse_animation_controller.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

static HorseRenderStats s_horseRenderStats;

auto getHorseRenderStats() -> const HorseRenderStats & {
  return s_horseRenderStats;
}

void resetHorseRenderStats() { s_horseRenderStats.reset(); }

using Render::Geom::clamp01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::lerp;
using Render::Geom::smoothstep;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

constexpr int k_hash_shift_16 = 16;
constexpr int k_hash_shift_15 = 15;
constexpr uint32_t k_hash_mult_1 = 0x7Feb352dU;
constexpr uint32_t k_hash_mult_2 = 0x846ca68bU;
constexpr uint32_t k_hash_mask_24bit = 0xFFFFFF;
constexpr float k_hash_divisor = 16777216.0F;

constexpr float k_rgb_max = 255.0F;
constexpr int k_rgb_shift_red = 16;
constexpr int k_rgb_shift_green = 8;

inline auto hash01(uint32_t x) -> float {
  x ^= x >> k_hash_shift_16;
  x *= k_hash_mult_1;
  x ^= x >> k_hash_shift_15;
  x *= k_hash_mult_2;
  x ^= x >> k_hash_shift_16;
  return (x & k_hash_mask_24bit) / k_hash_divisor;
}

inline auto randBetween(uint32_t seed, uint32_t salt, float minV,
                        float maxV) -> float {
  const float t = hash01(seed ^ salt);
  return minV + (maxV - minV) * t;
}

inline auto saturate(float x) -> float {
  return std::min(1.0F, std::max(0.0F, x));
}

inline auto rotateAroundY(const QVector3D &v, float angle) -> QVector3D {
  float const s = std::sin(angle);
  float const c = std::cos(angle);
  return {v.x() * c + v.z() * s, v.y(), -v.x() * s + v.z() * c};
}
inline auto rotateAroundZ(const QVector3D &v, float angle) -> QVector3D {
  float const s = std::sin(angle);
  float const c = std::cos(angle);
  return {v.x() * c - v.y() * s, v.x() * s + v.y() * c, v.z()};
}

inline auto darken(const QVector3D &c, float k) -> QVector3D { return c * k; }
inline auto lighten(const QVector3D &c, float k) -> QVector3D {
  return {saturate(c.x() * k), saturate(c.y() * k), saturate(c.z() * k)};
}

inline auto coatGradient(const QVector3D &coat, float verticalFactor,
                         float longitudinalFactor, float seed) -> QVector3D {
  float const highlight = saturate(0.55F + verticalFactor * 0.35F -
                                   longitudinalFactor * 0.20F + seed * 0.08F);
  QVector3D const bright = lighten(coat, 1.08F);
  QVector3D const shadow = darken(coat, 0.86F);
  return shadow * (1.0F - highlight) + bright * highlight;
}

inline auto lerp3(const QVector3D &a, const QVector3D &b,
                  float t) -> QVector3D {
  return {a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
          a.z() + (b.z() - a.z()) * t};
}

inline auto scaledSphere(const QMatrix4x4 &model, const QVector3D &center,
                         const QVector3D &scale) -> QMatrix4x4 {
  QMatrix4x4 m = model;
  m.translate(center);
  m.scale(scale);
  return m;
}

inline void draw_cylinder(ISubmitter &out, const QMatrix4x4 &model,
                          const QVector3D &a, const QVector3D &b, float radius,
                          const QVector3D &color, float alpha = 1.0F,
                          int materialId = 0) {
  out.mesh(getUnitCylinder(), cylinderBetween(model, a, b, radius), color,
           nullptr, alpha, materialId);
}

inline void drawCone(ISubmitter &out, const QMatrix4x4 &model,
                     const QVector3D &tip, const QVector3D &base, float radius,
                     const QVector3D &color, float alpha = 1.0F,
                     int materialId = 0) {
  out.mesh(getUnitCone(), coneFromTo(model, tip, base, radius), color, nullptr,
           alpha, materialId);
}

inline void drawRoundedSegment(ISubmitter &out, const QMatrix4x4 &model,
                               const QVector3D &start, const QVector3D &end,
                               float start_radius, float end_radius,
                               const QVector3D &start_color,
                               const QVector3D &end_color, float alpha = 1.0F,
                               int materialId = 0) {
  float const mid_radius = 0.5F * (start_radius + end_radius);
  QVector3D const tint = lerp(start_color, end_color, 0.5F);
  out.mesh(getUnitCylinder(), cylinderBetween(model, start, end, mid_radius),
           tint, nullptr, alpha, materialId);
  out.mesh(getUnitSphere(), Render::Geom::sphereAt(model, start, start_radius),
           start_color, nullptr, alpha, materialId);
  out.mesh(getUnitSphere(), Render::Geom::sphereAt(model, end, end_radius),
           end_color, nullptr, alpha, materialId);
}

inline auto bezier(const QVector3D &p0, const QVector3D &p1,
                   const QVector3D &p2, float t) -> QVector3D {
  float const u = 1.0F - t;
  return p0 * (u * u) + p1 * (2.0F * u * t) + p2 * (t * t);
}

inline auto colorHash(const QVector3D &c) -> uint32_t {
  auto const r = uint32_t(saturate(c.x()) * k_rgb_max);
  auto const g = uint32_t(saturate(c.y()) * k_rgb_max);
  auto const b = uint32_t(saturate(c.z()) * k_rgb_max);
  uint32_t v = (r << k_rgb_shift_red) | (g << k_rgb_shift_green) | b;

  v ^= v >> k_hash_shift_16;
  v *= k_hash_mult_1;
  v ^= v >> k_hash_shift_15;
  v *= k_hash_mult_2;
  v ^= v >> k_hash_shift_16;
  return v;
}

} // namespace

auto makeHorseDimensions(uint32_t seed) -> HorseDimensions {
  HorseDimensions d{};

  d.bodyLength = randBetween(seed, 0x12U, 0.88F, 0.98F);
  d.bodyWidth = randBetween(seed, 0x34U, 0.18F, 0.22F);
  d.bodyHeight = randBetween(seed, 0x56U, 0.40F, 0.46F);

  d.neckLength = randBetween(seed, 0x9AU, 0.42F, 0.50F);
  d.neckRise = randBetween(seed, 0xBCU, 0.26F, 0.32F);
  d.headLength = randBetween(seed, 0xDEU, 0.28F, 0.34F);
  d.headWidth = randBetween(seed, 0xF1U, 0.14F, 0.17F);
  d.headHeight = randBetween(seed, 0x1357U, 0.18F, 0.22F);
  d.muzzleLength = randBetween(seed, 0x2468U, 0.13F, 0.16F);

  d.legLength = randBetween(seed, 0x369CU, 1.05F, 1.18F);
  d.hoofHeight = randBetween(seed, 0x48AEU, 0.080F, 0.095F);

  d.tailLength = randBetween(seed, 0x5ABCU, 0.38F, 0.48F);

  d.saddleThickness = randBetween(seed, 0x6CDEU, 0.035F, 0.045F);
  d.seatForwardOffset = randBetween(seed, 0x7531U, 0.010F, 0.035F);
  d.stirrupOut = d.bodyWidth * randBetween(seed, 0x8642U, 0.75F, 0.88F);
  d.stirrupDrop = randBetween(seed, 0x9753U, 0.28F, 0.32F);

  d.idleBobAmplitude = randBetween(seed, 0xA864U, 0.004F, 0.007F);
  d.moveBobAmplitude = randBetween(seed, 0xB975U, 0.024F, 0.032F);

  float const avg_leg_segment_ratio = 0.59F + 0.30F + 0.12F;
  float const leg_down_distance =
      d.legLength * avg_leg_segment_ratio + d.hoofHeight;
  float const shoulder_to_barrel_offset = d.bodyHeight * 0.05F + 0.05F;
  d.barrel_centerY = leg_down_distance - shoulder_to_barrel_offset;

  d.saddle_height = d.barrel_centerY + d.bodyHeight * 0.55F + d.saddleThickness;

  return d;
}

auto makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseVariant {
  HorseVariant v;

  float const coat_hue = hash01(seed ^ 0x23456U);
  if (coat_hue < 0.18F) {
    v.coatColor = QVector3D(0.70F, 0.68F, 0.63F);
  } else if (coat_hue < 0.38F) {
    v.coatColor = QVector3D(0.40F, 0.30F, 0.22F);
  } else if (coat_hue < 0.65F) {
    v.coatColor = QVector3D(0.28F, 0.22F, 0.19F);
  } else if (coat_hue < 0.85F) {
    v.coatColor = QVector3D(0.18F, 0.15F, 0.13F);
  } else {
    v.coatColor = QVector3D(0.48F, 0.42F, 0.39F);
  }

  float const blaze_chance = hash01(seed ^ 0x1122U);
  if (blaze_chance > 0.82F) {
    v.coatColor = lerp(v.coatColor, QVector3D(0.92F, 0.92F, 0.90F), 0.25F);
  }

  v.mane_color = lerp(v.coatColor, QVector3D(0.10F, 0.09F, 0.08F),
                      randBetween(seed, 0x3344U, 0.55F, 0.85F));
  v.tail_color = lerp(v.mane_color, v.coatColor, 0.35F);

  v.muzzleColor = lerp(v.coatColor, QVector3D(0.18F, 0.14F, 0.12F), 0.65F);
  v.hoof_color =
      lerp(QVector3D(0.16F, 0.14F, 0.12F), QVector3D(0.40F, 0.35F, 0.32F),
           randBetween(seed, 0x5566U, 0.15F, 0.65F));

  float const leather_tone = randBetween(seed, 0x7788U, 0.78F, 0.96F);
  float const tack_tone = randBetween(seed, 0x88AAU, 0.58F, 0.78F);
  QVector3D const leather_tint = leatherBase * leather_tone;
  QVector3D tack_tint = leatherBase * tack_tone;
  if (blaze_chance > 0.90F) {

    tack_tint = lerp(tack_tint, QVector3D(0.18F, 0.19F, 0.22F), 0.25F);
  }
  v.saddleColor = leather_tint;
  v.tack_color = tack_tint;

  v.blanketColor = clothBase * randBetween(seed, 0x99B0U, 0.92F, 1.05F);

  return v;
}

auto makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseProfile {
  HorseProfile profile;
  profile.dims = makeHorseDimensions(seed);
  profile.variant = makeHorseVariant(seed, leatherBase, clothBase);

  profile.gait.cycleTime = randBetween(seed, 0xAA12U, 0.60F, 0.72F);
  profile.gait.frontLegPhase = randBetween(seed, 0xBB34U, 0.08F, 0.16F);
  float const diagonal_lead = randBetween(seed, 0xCC56U, 0.44F, 0.54F);
  profile.gait.rearLegPhase =
      std::fmod(profile.gait.frontLegPhase + diagonal_lead, 1.0F);
  profile.gait.strideSwing = randBetween(seed, 0xDD78U, 0.26F, 0.32F);
  profile.gait.strideLift = randBetween(seed, 0xEE9AU, 0.10F, 0.14F);

  return profile;
}

auto MountedAttachmentFrame::stirrup_attach(bool is_left) const
    -> const QVector3D & {
  return is_left ? stirrup_attach_left : stirrup_attach_right;
}

auto MountedAttachmentFrame::stirrup_bottom(bool is_left) const
    -> const QVector3D & {
  return is_left ? stirrup_bottom_left : stirrup_bottom_right;
}

auto compute_mount_frame(const HorseProfile &profile)
    -> MountedAttachmentFrame {
  const HorseDimensions &d = profile.dims;
  MountedAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.ground_offset = QVector3D(0.0F, -d.barrel_centerY, 0.0F);

  frame.saddle_center =
      QVector3D(0.0F, d.saddle_height - d.saddleThickness * 0.35F,
                -d.bodyLength * 0.05F + d.seatForwardOffset * 0.25F);

  frame.seat_position =
      frame.saddle_center + QVector3D(0.0F, d.saddleThickness * 0.32F, 0.0F);

  frame.stirrup_attach_left =
      frame.saddle_center + QVector3D(-d.bodyWidth * 0.92F,
                                      -d.saddleThickness * 0.10F,
                                      d.seatForwardOffset * 0.28F);
  frame.stirrup_attach_right =
      frame.saddle_center + QVector3D(d.bodyWidth * 0.92F,
                                      -d.saddleThickness * 0.10F,
                                      d.seatForwardOffset * 0.28F);

  frame.stirrup_bottom_left =
      frame.stirrup_attach_left + QVector3D(0.0F, -d.stirrupDrop, 0.0F);
  frame.stirrup_bottom_right =
      frame.stirrup_attach_right + QVector3D(0.0F, -d.stirrupDrop, 0.0F);

  QVector3D const neck_top(0.0F,
                           d.barrel_centerY + d.bodyHeight * 0.65F + d.neckRise,
                           d.bodyLength * 0.25F);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.headHeight * 0.10F, d.headLength * 0.40F);

  QVector3D const muzzle_center =
      head_center +
      QVector3D(0.0F, -d.headHeight * 0.18F, d.headLength * 0.58F);
  frame.bridle_base = muzzle_center + QVector3D(0.0F, -d.headHeight * 0.05F,
                                                d.muzzleLength * 0.20F);
  frame.rein_bit_left =
      muzzle_center + QVector3D(d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);
  frame.rein_bit_right =
      muzzle_center + QVector3D(-d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);

  return frame;
}

auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx)
    -> ReinState {
  float const base_slack = hash01(horse_seed ^ 0x707U) * 0.08F + 0.02F;
  float rein_tension = rider_ctx.locomotion_normalized_speed();
  if (rider_ctx.gait.has_target) {
    rein_tension += 0.25F;
  }
  if (rider_ctx.is_attacking()) {
    rein_tension += 0.35F;
  }
  rein_tension = std::clamp(rein_tension, 0.0F, 1.0F);
  float const rein_slack = std::max(0.01F, base_slack * (1.0F - rein_tension));
  return ReinState{rein_slack, rein_tension};
}

auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D {
  float const clamped_slack = std::clamp(slack, 0.0F, 1.0F);
  float const clamped_tension = std::clamp(tension, 0.0F, 1.0F);

  QVector3D const &bit = is_left ? mount.rein_bit_left : mount.rein_bit_right;

  QVector3D desired = mount.seat_position;
  desired += (is_left ? -mount.seat_right : mount.seat_right) * 0.08F;
  desired += -mount.seat_forward * (0.18F + clamped_tension * 0.18F);
  desired += mount.seat_up *
             (-0.10F - clamped_slack * 0.30F + clamped_tension * 0.04F);

  QVector3D dir = desired - bit;
  if (dir.lengthSquared() < 1e-4F) {
    dir = -mount.seat_forward;
  }
  dir.normalize();

  constexpr float k_base_length = 0.85F;
  float const rein_length = k_base_length + clamped_slack * 0.12F;
  return bit + dir * rein_length;
}

auto evaluate_horse_motion(HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx)
    -> HorseMotionSample {
  HorseMotionSample sample{};
  HorseAnimationController controller(profile, anim, rider_ctx);
  sample.rider_intensity = rider_ctx.locomotion_normalized_speed();
  bool const rider_has_motion =
      rider_ctx.is_walking() || rider_ctx.is_running();
  sample.is_moving = rider_has_motion || anim.is_moving;

  if (sample.is_moving) {
    float const speed = rider_ctx.locomotion_speed();
    if (speed < 0.5F) {
      controller.idle(1.0F);
    } else if (speed < 3.0F) {
      controller.setGait(GaitType::WALK);
    } else if (speed < 5.5F) {
      controller.setGait(GaitType::TROT);
    } else if (speed < 8.0F) {
      controller.setGait(GaitType::CANTER);
    } else {
      controller.setGait(GaitType::GALLOP);
    }
  } else {
    controller.idle(1.0F);
  }

  controller.updateGaitParameters();
  sample.phase = controller.getCurrentPhase();
  sample.bob = controller.getCurrentBob();
  return sample;
}

void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.saddle_center += offset;
  frame.seat_position += offset;
  frame.stirrup_attach_left += offset;
  frame.stirrup_attach_right += offset;
  frame.stirrup_bottom_left += offset;
  frame.stirrup_bottom_right += offset;
  frame.rein_bit_left += offset;
  frame.rein_bit_right += offset;
  frame.bridle_base += offset;
}

void HorseRendererBase::renderFull(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
    const MountedAttachmentFrame *shared_mount, const ReinState *shared_reins,
    const HorseMotionSample *shared_motion, ISubmitter &out) const {
  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;
  const HorseGait &g = profile.gait;
  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(profile, anim, rider_ctx);
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;
  const float rider_intensity = motion.rider_intensity;

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);
  if (!shared_mount) {
    apply_mount_vertical_offset(mount, bob);
  }

  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  horse_ctx.model.translate(mount.ground_offset);

  float const head_nod = is_moving ? std::sin((phase + 0.25F) * 2.0F * k_pi) *
                                         (0.02F + rider_intensity * 0.03F)
                                   : std::sin(anim.time * 1.5F) * 0.01F;

  uint32_t const vhash = colorHash(v.coatColor);
  float const sock_chance_fl = hash01(vhash ^ 0x101U);
  float const sock_chance_fr = hash01(vhash ^ 0x202U);
  float const sock_chance_rl = hash01(vhash ^ 0x303U);
  float const sock_chance_rr = hash01(vhash ^ 0x404U);
  bool const has_blaze = hash01(vhash ^ 0x505U) > 0.82F;

  ReinState const rein_state =
      shared_reins ? *shared_reins : compute_rein_state(horse_seed, rider_ctx);
  float const rein_slack = rein_state.slack;
  float const rein_tension = rein_state.tension;

  const float coat_seed_a = hash01(vhash ^ 0x701U);
  const float coat_seed_b = hash01(vhash ^ 0x702U);
  const float coat_seed_c = hash01(vhash ^ 0x703U);
  const float coat_seed_d = hash01(vhash ^ 0x704U);

  QVector3D const barrel_center(0.0F, d.barrel_centerY + bob, 0.0F);

  float const ground_offset = -d.barrel_centerY - bob;

  QVector3D const chest_center =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.12F, d.bodyLength * 0.34F);
  QVector3D const rump_center =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.08F, -d.bodyLength * 0.36F);
  QVector3D const belly_center =
      barrel_center +
      QVector3D(0.0F, -d.bodyHeight * 0.35F, -d.bodyLength * 0.05F);

  {
    QMatrix4x4 chest = horse_ctx.model;
    chest.translate(chest_center);
    chest.scale(d.bodyWidth * 1.12F, d.bodyHeight * 0.95F,
                d.bodyLength * 0.36F);
    QVector3D const chest_color =
        coatGradient(v.coatColor, 0.75F, 0.20F, coat_seed_a);
    out.mesh(getUnitSphere(), chest, chest_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 withers = horse_ctx.model;
    withers.translate(chest_center + QVector3D(0.0F, d.bodyHeight * 0.55F,
                                               -d.bodyLength * 0.03F));
    withers.scale(d.bodyWidth * 0.75F, d.bodyHeight * 0.35F,
                  d.bodyLength * 0.18F);
    QVector3D const wither_color =
        coatGradient(v.coatColor, 0.88F, 0.35F, coat_seed_b);
    out.mesh(getUnitSphere(), withers, wither_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 belly = horse_ctx.model;
    belly.translate(belly_center);
    belly.scale(d.bodyWidth * 0.98F, d.bodyHeight * 0.64F,
                d.bodyLength * 0.40F);
    QVector3D const belly_color =
        coatGradient(v.coatColor, 0.25F, -0.10F, coat_seed_c);
    out.mesh(getUnitSphere(), belly, belly_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 rump = horse_ctx.model;
    rump.translate(rump_center);
    rump.scale(d.bodyWidth * 1.18F, d.bodyHeight * 1.00F, d.bodyLength * 0.36F);
    QVector3D const rump_color =
        coatGradient(v.coatColor, 0.62F, -0.28F, coat_seed_a * 0.7F);
    out.mesh(getUnitSphere(), rump, rump_color, nullptr, 1.0F, 6);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 hip = horse_ctx.model;
    hip.translate(rump_center + QVector3D(side * d.bodyWidth * 0.95F,
                                          -d.bodyHeight * 0.10F,
                                          -d.bodyLength * 0.08F));
    hip.scale(d.bodyWidth * 0.45F, d.bodyHeight * 0.42F, d.bodyLength * 0.26F);
    QVector3D const hip_color =
        coatGradient(v.coatColor, 0.58F, -0.18F, coat_seed_b + side * 0.06F);
    out.mesh(getUnitSphere(), hip, hip_color, nullptr, 1.0F, 6);

    QMatrix4x4 haunch = horse_ctx.model;
    haunch.translate(rump_center + QVector3D(side * d.bodyWidth * 0.88F,
                                             d.bodyHeight * 0.24F,
                                             -d.bodyLength * 0.20F));
    haunch.scale(QVector3D(d.bodyWidth * 0.32F, d.bodyHeight * 0.28F,
                           d.bodyLength * 0.18F));
    QVector3D const haunch_color =
        coatGradient(v.coatColor, 0.72F, -0.26F, coat_seed_c + side * 0.04F);
    out.mesh(getUnitSphere(), haunch, lighten(haunch_color, 1.02F), nullptr,
             1.0F, 6);
  }

  QVector3D withers_peak = chest_center + QVector3D(0.0F, d.bodyHeight * 0.62F,
                                                    -d.bodyLength * 0.06F);
  QVector3D croup_peak = rump_center + QVector3D(0.0F, d.bodyHeight * 0.46F,
                                                 -d.bodyLength * 0.18F);

  {
    QMatrix4x4 spine = horse_ctx.model;
    spine.translate(lerp(withers_peak, croup_peak, 0.42F));
    spine.scale(QVector3D(d.bodyWidth * 0.50F, d.bodyHeight * 0.14F,
                          d.bodyLength * 0.54F));
    QVector3D const spine_color =
        coatGradient(v.coatColor, 0.74F, -0.06F, coat_seed_d * 0.92F);
    out.mesh(getUnitSphere(), spine, spine_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 sternum = horse_ctx.model;
    sternum.translate(barrel_center + QVector3D(0.0F, -d.bodyHeight * 0.40F,
                                                d.bodyLength * 0.28F));
    sternum.scale(QVector3D(d.bodyWidth * 0.50F, d.bodyHeight * 0.14F,
                            d.bodyLength * 0.12F));
    out.mesh(getUnitSphere(), sternum,
             coatGradient(v.coatColor, 0.18F, 0.18F, coat_seed_a * 0.4F),
             nullptr, 1.0F, 6);
  }

  QVector3D const neck_base =
      chest_center +
      QVector3D(0.0F, d.bodyHeight * 0.38F, d.bodyLength * 0.06F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, d.neckRise, d.neckLength);
  float const neck_radius = d.bodyWidth * 0.42F;

  QVector3D const neck_mid =
      lerp(neck_base, neck_top, 0.55F) +
      QVector3D(0.0F, d.bodyHeight * 0.02F, d.bodyLength * 0.02F);
  QVector3D const neck_color_base =
      coatGradient(v.coatColor, 0.78F, 0.12F, coat_seed_c * 0.6F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(horse_ctx.model, neck_base, neck_mid,
                           neck_radius * 1.00F),
           neck_color_base, nullptr, 1.0F);
  out.mesh(
      getUnitCylinder(),
      cylinderBetween(horse_ctx.model, neck_mid, neck_top, neck_radius * 0.86F),
      lighten(neck_color_base, 1.03F), nullptr, 1.0F);

  {
    QVector3D const jugular_start =
        lerp(neck_base, neck_top, 0.42F) + QVector3D(d.bodyWidth * 0.18F,
                                                     -d.bodyHeight * 0.06F,
                                                     d.bodyLength * 0.04F);
    QVector3D const jugular_end =
        jugular_start +
        QVector3D(0.0F, -d.bodyHeight * 0.24F, d.bodyLength * 0.06F);
    draw_cylinder(out, horse_ctx.model, jugular_start, jugular_end,
                  neck_radius * 0.18F, lighten(neck_color_base, 1.08F), 0.85F,
                  6);
  }

  const int mane_sections = 8;
  QVector3D const mane_color =
      lerp3(v.mane_color, QVector3D(0.12F, 0.09F, 0.08F), 0.35F);
  for (int i = 0; i < mane_sections; ++i) {
    float const t =
        static_cast<float>(i) / static_cast<float>(mane_sections - 1);
    QVector3D const spine = lerp(neck_base, neck_top, t) +
                            QVector3D(0.0F, d.bodyHeight * 0.12F, 0.0F);
    float const length = lerp(0.14F, 0.08F, t) * d.bodyHeight * 1.4F;
    QVector3D const tip =
        spine + QVector3D(0.0F, length * 1.2F, 0.02F * length);
    drawCone(out, horse_ctx.model, tip, spine,
             d.bodyWidth * lerp(0.25F, 0.12F, t), mane_color, 1.0F, 7);
  }

  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.headHeight * (0.10F - head_nod * 0.15F),
                           d.headLength * 0.40F);

  {
    QMatrix4x4 skull = horse_ctx.model;
    skull.translate(head_center + QVector3D(0.0F, d.headHeight * 0.10F,
                                            -d.headLength * 0.10F));
    skull.scale(d.headWidth * 0.95F, d.headHeight * 0.90F,
                d.headLength * 0.80F);
    QVector3D const skull_color =
        coatGradient(v.coatColor, 0.82F, 0.30F, coat_seed_d * 0.8F);
    out.mesh(getUnitSphere(), skull, skull_color, nullptr, 1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 cheek = horse_ctx.model;
    cheek.translate(head_center + QVector3D(side * d.headWidth * 0.55F,
                                            -d.headHeight * 0.15F, 0.0F));
    cheek.scale(d.headWidth * 0.45F, d.headHeight * 0.50F,
                d.headLength * 0.60F);
    QVector3D const cheek_color =
        coatGradient(v.coatColor, 0.70F, 0.18F, coat_seed_a * 0.9F);
    out.mesh(getUnitSphere(), cheek, cheek_color, nullptr, 1.0F, 6);
  }

  QVector3D const muzzle_center =
      head_center +
      QVector3D(0.0F, -d.headHeight * 0.18F, d.headLength * 0.58F);
  {
    QMatrix4x4 muzzle = horse_ctx.model;
    muzzle.translate(muzzle_center +
                     QVector3D(0.0F, -d.headHeight * 0.05F, 0.0F));
    muzzle.scale(d.headWidth * 0.68F, d.headHeight * 0.60F,
                 d.muzzleLength * 1.05F);
    out.mesh(getUnitSphere(), muzzle, v.muzzleColor, nullptr, 1.0F);
  }

  {
    QVector3D const nostril_base =
        muzzle_center +
        QVector3D(0.0F, -d.headHeight * 0.02F, d.muzzleLength * 0.60F);
    QVector3D const left_base =
        nostril_base + QVector3D(d.headWidth * 0.26F, 0.0F, 0.0F);
    QVector3D const right_base =
        nostril_base + QVector3D(-d.headWidth * 0.26F, 0.0F, 0.0F);
    QVector3D const inward =
        QVector3D(0.0F, -d.headHeight * 0.02F, d.muzzleLength * -0.30F);
    out.mesh(getUnitCone(),
             coneFromTo(horse_ctx.model, left_base + inward, left_base,
                        d.headWidth * 0.11F),
             darken(v.muzzleColor, 0.6F), nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(horse_ctx.model, right_base + inward, right_base,
                        d.headWidth * 0.11F),
             darken(v.muzzleColor, 0.6F), nullptr, 1.0F);
  }

  float const ear_flick_l = std::sin(anim.time * 1.7F + 1.3F) * 0.15F;
  float const ear_flick_r = std::sin(anim.time * 1.9F + 2.1F) * -0.12F;

  QVector3D const ear_base_left =
      head_center + QVector3D(d.headWidth * 0.45F, d.headHeight * 0.42F,
                              -d.headLength * 0.20F);
  QVector3D const ear_tip_left =
      ear_base_left +
      rotateAroundY(QVector3D(d.headWidth * 0.08F, d.headHeight * 0.42F,
                              -d.headLength * 0.10F),
                    ear_flick_l);
  QVector3D const ear_base_right =
      head_center + QVector3D(-d.headWidth * 0.45F, d.headHeight * 0.42F,
                              -d.headLength * 0.20F);
  QVector3D const ear_tip_right =
      ear_base_right +
      rotateAroundY(QVector3D(-d.headWidth * 0.08F, d.headHeight * 0.42F,
                              -d.headLength * 0.10F),
                    ear_flick_r);

  out.mesh(getUnitCone(),
           coneFromTo(horse_ctx.model, ear_tip_left, ear_base_left,
                      d.headWidth * 0.11F),
           v.mane_color, nullptr, 1.0F);
  out.mesh(getUnitCone(),
           coneFromTo(horse_ctx.model, ear_tip_right, ear_base_right,
                      d.headWidth * 0.11F),
           v.mane_color, nullptr, 1.0F);

  QVector3D const eye_left =
      head_center + QVector3D(d.headWidth * 0.48F, d.headHeight * 0.10F,
                              d.headLength * 0.05F);
  QVector3D const eye_right =
      head_center + QVector3D(-d.headWidth * 0.48F, d.headHeight * 0.10F,
                              d.headLength * 0.05F);
  QVector3D eye_base_color(0.10F, 0.10F, 0.10F);

  auto draw_eye = [&](const QVector3D &pos) {
    {
      QMatrix4x4 eye = horse_ctx.model;
      eye.translate(pos);
      eye.scale(d.headWidth * 0.14F);
      out.mesh(getUnitSphere(), eye, eye_base_color, nullptr, 1.0F, 6);
    }
    {

      QMatrix4x4 pupil = horse_ctx.model;
      pupil.translate(pos + QVector3D(0.0F, 0.0F, d.headWidth * 0.04F));
      pupil.scale(d.headWidth * 0.05F);
      out.mesh(getUnitSphere(), pupil, QVector3D(0.03F, 0.03F, 0.03F), nullptr,
               1.0F, 6);
    }
    {

      QMatrix4x4 spec = horse_ctx.model;
      spec.translate(pos + QVector3D(d.headWidth * 0.03F, d.headWidth * 0.03F,
                                     d.headWidth * 0.03F));
      spec.scale(d.headWidth * 0.02F);
      out.mesh(getUnitSphere(), spec, QVector3D(0.95F, 0.95F, 0.95F), nullptr,
               1.0F, 6);
    }
  };
  draw_eye(eye_left);
  draw_eye(eye_right);

  if (has_blaze) {
    QMatrix4x4 blaze = horse_ctx.model;
    blaze.translate(head_center + QVector3D(0.0F, d.headHeight * 0.15F,
                                            d.headLength * 0.10F));
    blaze.scale(d.headWidth * 0.22F, d.headHeight * 0.32F,
                d.headLength * 0.10F);
    out.mesh(getUnitSphere(), blaze, QVector3D(0.92F, 0.92F, 0.90F), nullptr,
             1.0F, 6);
  }

  QVector3D bridle_base = muzzle_center + QVector3D(0.0F, -d.headHeight * 0.05F,
                                                    d.muzzleLength * 0.20F);
  mount.bridle_base = bridle_base;
  QVector3D const cheek_anchor_left =
      head_center + QVector3D(d.headWidth * 0.55F, d.headHeight * 0.05F,
                              -d.headLength * 0.05F);
  QVector3D const cheek_anchor_right =
      head_center + QVector3D(-d.headWidth * 0.55F, d.headHeight * 0.05F,
                              -d.headLength * 0.05F);
  QVector3D const brow = head_center + QVector3D(0.0F, d.headHeight * 0.38F,
                                                 -d.headLength * 0.28F);
  QVector3D const tack_color = lighten(v.tack_color, 0.9F);
  draw_cylinder(out, horse_ctx.model, bridle_base, cheek_anchor_left,
                d.headWidth * 0.07F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, bridle_base, cheek_anchor_right,
                d.headWidth * 0.07F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, cheek_anchor_left, brow,
                d.headWidth * 0.05F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, cheek_anchor_right, brow,
                d.headWidth * 0.05F, tack_color, 1.0F, 10);

  QVector3D const mane_root =
      neck_top + QVector3D(0.0F, d.headHeight * 0.20F, -d.headLength * 0.20F);
  constexpr int k_mane_segments = 12;
  constexpr float k_mane_segment_divisor = 11.0F;
  for (int i = 0; i < k_mane_segments; ++i) {
    float const t = i / k_mane_segment_divisor;
    QVector3D seg_start = lerp(mane_root, neck_base, t);
    seg_start.setY(seg_start.y() + (0.07F - t * 0.05F));
    float const sway =
        (is_moving ? std::sin((phase + t * 0.15F) * 2.0F * k_pi) *
                         (0.025F + rider_intensity * 0.025F)
                   : std::sin((anim.time * 0.8F + t * 2.3F)) * 0.02F);
    QVector3D const seg_end =
        seg_start + QVector3D(sway, 0.07F - t * 0.05F, -0.05F - t * 0.03F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(horse_ctx.model, seg_start, seg_end,
                             d.headWidth * (0.10F * (1.0F - t * 0.4F))),
             v.mane_color * (0.98F + t * 0.05F), nullptr, 1.0F, 7);
  }

  {
    QVector3D const forelock_base =
        head_center +
        QVector3D(0.0F, d.headHeight * 0.28F, -d.headLength * 0.18F);
    for (int i = 0; i < 3; ++i) {
      float const offset = (i - 1) * d.headWidth * 0.10F;
      QVector3D const strand_base =
          forelock_base + QVector3D(offset, 0.0F, 0.0F);
      QVector3D const strand_tip =
          strand_base +
          QVector3D(offset * 0.4F, -d.headHeight * 0.25F, d.headLength * 0.12F);
      drawCone(out, horse_ctx.model, strand_tip, strand_base,
               d.headWidth * 0.10F, v.mane_color * (0.94F + 0.03F * i), 0.96F,
               7);
    }
  }

  QVector3D const tail_base =
      rump_center +
      QVector3D(0.0F, d.bodyHeight * 0.36F, -d.bodyLength * 0.34F);
  QVector3D const tail_ctrl =
      tail_base + QVector3D(0.0F, -d.tailLength * 0.20F, -d.tailLength * 0.28F);
  QVector3D const tail_end =
      tail_base + QVector3D(0.0F, -d.tailLength, -d.tailLength * 0.70F);
  QVector3D const tail_color = lerp3(v.tail_color, v.mane_color, 0.35F);
  QVector3D prev_tail = tail_base;
  for (int i = 1; i <= 8; ++i) {
    float const t = static_cast<float>(i) / 8.0F;
    QVector3D p = bezier(tail_base, tail_ctrl, tail_end, t);
    float const swing =
        (is_moving ? std::sin((phase + t * 0.12F) * 2.0F * k_pi)
                   : std::sin((phase * 0.7F + t * 0.3F) * 2.0F * k_pi)) *
        (0.025F + rider_intensity * 0.020F + 0.015F * (1.0F - t));
    p.setX(p.x() + swing);
    float const radius = d.bodyWidth * (0.20F - 0.018F * i);
    draw_cylinder(out, horse_ctx.model, prev_tail, p, radius, tail_color, 1.0F,
                  7);
    prev_tail = p;
  }

  {
    QMatrix4x4 tail_knot = horse_ctx.model;
    tail_knot.translate(tail_base + QVector3D(0.0F, -d.bodyHeight * 0.06F,
                                              -d.bodyLength * 0.01F));
    tail_knot.scale(QVector3D(d.bodyWidth * 0.24F, d.bodyWidth * 0.18F,
                              d.bodyWidth * 0.20F));
    out.mesh(getUnitSphere(), tail_knot, lighten(tail_color, 0.92F), nullptr,
             1.0F, 7);
  }

  for (int i = 0; i < 3; ++i) {
    float const spread = (i - 1) * d.bodyWidth * 0.14F;
    QVector3D const fan_base =
        tail_end +
        QVector3D(spread * 0.15F, -d.bodyWidth * 0.05F, -d.tailLength * 0.08F);
    QVector3D const fan_tip =
        fan_base +
        QVector3D(spread, -d.tailLength * 0.32F, -d.tailLength * 0.22F);
    drawCone(out, horse_ctx.model, fan_tip, fan_base, d.bodyWidth * 0.24F,
             tail_color * (0.96F + 0.02F * i), 0.88F, 7);
  }

  auto render_hoof = [&](const QVector3D &hoof_top, float hoof_height,
                         float half_width, float half_depth,
                         const QVector3D &hoof_color, bool is_rear) {
    QVector3D const hoof_center =
        hoof_top + QVector3D(0.0F, -hoof_height * 0.5F, 0.0F);
    QVector3D const wall_tint = lighten(hoof_color, is_rear ? 1.02F : 1.05F);
    QMatrix4x4 hoof_block = horse_ctx.model;
    hoof_block.translate(hoof_center);
    hoof_block.scale(QVector3D(half_width, hoof_height * 0.5F, half_depth));
    out.mesh(getUnitCylinder(), hoof_block, wall_tint, nullptr, 1.0F, 8);

    QMatrix4x4 sole = horse_ctx.model;
    sole.translate(hoof_center + QVector3D(0.0F, -hoof_height * 0.45F, 0.0F));
    sole.scale(
        QVector3D(half_width * 0.92F, hoof_height * 0.08F, half_depth * 0.95F));
    out.mesh(getUnitCylinder(), sole, darken(hoof_color, 0.72F), nullptr, 1.0F,
             8);

    QMatrix4x4 toe = horse_ctx.model;
    toe.translate(hoof_center + QVector3D(0.0F, -hoof_height * 0.10F,
                                          is_rear ? -half_depth * 0.35F
                                                  : half_depth * 0.30F));
    toe.scale(
        QVector3D(half_width * 0.85F, hoof_height * 0.20F, half_depth * 0.70F));
    out.mesh(getUnitSphere(), toe, lighten(hoof_color, 1.10F), nullptr, 1.0F,
             8);

    QMatrix4x4 coronet = horse_ctx.model;
    coronet.translate(hoof_top + QVector3D(0.0F, -hoof_height * 0.10F, 0.0F));
    coronet.scale(
        QVector3D(half_width * 0.95F, half_width * 0.60F, half_depth * 1.05F));
    out.mesh(getUnitSphere(), coronet, lighten(hoof_color, 1.16F), nullptr,
             1.0F, 8);
  };

  auto draw_leg = [&](const QVector3D &anchor, float lateralSign,
                      float forwardBias, float phase_offset, float sockChance) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.strideSwing * 0.75F + forwardBias;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.strideLift
                             : lift_raw * g.strideLift * 0.22F;
    } else {
      float const idle = std::sin(leg_phase * 2.0F * k_pi);
      stride = idle * g.strideSwing * 0.06F + forwardBias;
      lift = idle * d.idleBobAmplitude * 2.0F;
    }

    bool const is_rear = (forwardBias < 0.0F);
    if (!is_rear) {
      stride = std::clamp(stride, -d.bodyLength * 0.02F, d.bodyLength * 0.18F);
    }

    bool const tighten_legs = is_moving;
    float const shoulder_out =
        d.bodyWidth * (tighten_legs ? 0.42F : 0.56F) * (is_rear ? 0.96F : 1.0F);
    float const shoulder_height = (is_rear ? 0.02F : 0.05F);
    float const stance_pull =
        is_rear ? -d.bodyLength * 0.04F : d.bodyLength * 0.05F;
    float const stance_stagger =
        lateralSign *
        (is_rear ? -d.bodyLength * 0.020F : d.bodyLength * 0.030F);
    QVector3D shoulder =
        anchor + QVector3D(lateralSign * shoulder_out,
                           shoulder_height + lift * 0.04F,
                           stride + stance_pull + stance_stagger);

    float const gallop_angle = leg_phase * 2.0F * k_pi;
    float const hip_swing = is_moving ? std::sin(gallop_angle) : 0.0F;
    float const lift_factor =
        is_moving
            ? std::max(0.0F,
                       std::sin(gallop_angle + (is_rear ? 0.35F : -0.25F)))
            : 0.0F;

    shoulder.setZ(shoulder.z() + hip_swing * (is_rear ? -0.10F : 0.08F));
    if (tighten_legs) {
      shoulder.setX(shoulder.x() - lateralSign * lift_factor * 0.04F);
    }

    QVector3D girdle_top =
        (is_rear ? croup_peak : withers_peak) +
        QVector3D(lateralSign * d.bodyWidth * (is_rear ? 0.44F : 0.48F),
                  is_rear ? -d.bodyHeight * 0.06F : d.bodyHeight * 0.04F,
                  (is_rear ? -d.bodyLength * 0.08F : d.bodyLength * 0.07F));
    girdle_top.setZ(girdle_top.z() + hip_swing * (is_rear ? -0.08F : 0.05F));
    girdle_top.setX(girdle_top.x() - lateralSign * lift_factor * 0.03F);

    QVector3D const socket =
        shoulder +
        QVector3D(0.0F, d.bodyWidth * 0.12F,
                  is_rear ? -d.bodyLength * 0.05F : d.bodyLength * 0.04F);
    if (is_rear) {
      draw_cylinder(out, horse_ctx.model, girdle_top, socket,
                    d.bodyWidth * (is_rear ? 0.20F : 0.18F),
                    coatGradient(v.coatColor, is_rear ? 0.70F : 0.80F,
                                 is_rear ? -0.20F : 0.22F,
                                 coat_seed_b + lateralSign * 0.03F),
                    1.0F, 6);
    }

    if (is_rear) {
      QMatrix4x4 socket_cap = horse_ctx.model;
      socket_cap.translate(socket + QVector3D(0.0F, -d.bodyWidth * 0.04F,
                                              -d.bodyLength * 0.02F));
      socket_cap.scale(QVector3D(d.bodyWidth * 0.36F, d.bodyWidth * 0.28F,
                                 d.bodyLength * 0.18F));
      out.mesh(getUnitSphere(), socket_cap,
               coatGradient(v.coatColor, 0.60F, -0.24F,
                            coat_seed_c + lateralSign * 0.02F),
               nullptr, 1.0F);
    }

    float const upper_length = d.legLength * (is_rear ? 0.48F : 0.46F);
    float const lower_length = d.legLength * (is_rear ? 0.43F : 0.49F);
    float const pastern_length = d.legLength * (is_rear ? 0.12F : 0.14F);

    float const stance_phase = smoothstep(0.0F, 0.3F, leg_phase);
    float const swing_phase = smoothstep(0.3F, 0.7F, leg_phase);
    float const extend_phase = smoothstep(0.7F, 1.0F, leg_phase);

    float const knee_flex =
        is_moving
            ? (swing_phase * (1.0F - extend_phase) * (is_rear ? 0.85F : 0.75F))
            : 0.35F;

    float const cannon_flex = is_moving ? smoothstep(0.35F, 0.65F, leg_phase) *
                                              (1.0F - extend_phase) *
                                              (is_rear ? 0.70F : 0.60F)
                                        : 0.35F;

    float const fetlock_compress =
        is_moving ? std::max(stance_phase * 0.4F,
                             (1.0F - swing_phase) * extend_phase * 0.6F)
                  : 0.2F;

    float const backward_bias = is_rear ? -0.42F : -0.18F;
    float const hip_drive = (is_rear ? -1.0F : 1.0F) * hip_swing * 0.20F;

    float const upper_vertical =
        -0.90F - lift_factor * 0.08F - knee_flex * 0.25F;
    QVector3D upper_dir(lateralSign * (tighten_legs ? -0.05F : -0.02F),
                        upper_vertical, backward_bias + hip_drive);
    if (upper_dir.lengthSquared() < 1e-6F) {
      upper_dir = QVector3D(0.0F, -1.0F, backward_bias);
    }
    upper_dir.normalize();

    QVector3D knee = shoulder + upper_dir * upper_length;
    float const knee_out = d.bodyWidth * (is_rear ? 0.08F : 0.06F);
    knee.setX(knee.x() + lateralSign * knee_out);

    float const joint_drive =
        is_moving
            ? clamp01(std::sin(gallop_angle + (is_rear ? 0.50F : -0.35F)) *
                          0.55F +
                      0.45F)
            : 0.35F;

    float const lower_forward =
        (is_rear ? 0.44F : 0.20F) +
        (is_rear ? 0.30F : 0.18F) * (joint_drive - 0.5F) - cannon_flex * 0.35F;

    float const lower_vertical = -0.95F + cannon_flex * 0.15F;
    QVector3D lower_dir(lateralSign * (tighten_legs ? -0.02F : -0.01F),
                        lower_vertical, lower_forward);
    if (lower_dir.lengthSquared() < 1e-6F) {
      lower_dir = QVector3D(0.0F, -1.0F, lower_forward);
    }
    lower_dir.normalize();

    QVector3D cannon = knee + lower_dir * lower_length;

    float const pastern_bias = is_rear ? -0.30F : 0.08F;
    float const pastern_dyn =
        (is_rear ? -0.10F : 0.05F) * (joint_drive - 0.5F) +
        fetlock_compress * 0.25F;
    QVector3D pastern_dir(0.0F, -1.0F, pastern_bias + pastern_dyn);
    if (pastern_dir.lengthSquared() < 1e-6F) {
      pastern_dir = QVector3D(0.0F, -1.0F, pastern_bias);
    }
    pastern_dir.normalize();

    QVector3D fetlock = cannon + pastern_dir * pastern_length;

    QVector3D hoof_top = fetlock;
    if (is_moving) {

      float hoof_lift_amount = 0.0F;
      if (leg_phase > 0.25F && leg_phase < 0.85F) {

        float const lift_progress = (leg_phase - 0.25F) / 0.60F;
        float const lift_curve = std::sin(lift_progress * k_pi);
        hoof_lift_amount = lift_curve * lift;
      }
      hoof_top.setY(hoof_top.y() + hoof_lift_amount);
      fetlock = hoof_top;
    }

    float const shoulder_r = d.bodyWidth * (is_rear ? 0.35F : 0.32F);
    float const upper_r = shoulder_r * (is_rear ? 0.95F : 0.92F);
    float const knee_r = upper_r * 0.98F;
    float const cannon_r = knee_r * 0.96F;
    float const pastern_r = cannon_r * 0.84F;

    QVector3D const thigh_color = coatGradient(
        v.coatColor, is_rear ? 0.48F : 0.58F, is_rear ? -0.22F : 0.18F,
        coat_seed_a + lateralSign * 0.07F);

    draw_cylinder(out, horse_ctx.model, shoulder, knee,
                  (shoulder_r + upper_r) * 0.5F, thigh_color, 1.0F, 6);

    QVector3D const shin_color = darken(thigh_color, is_rear ? 0.90F : 0.92F);

    draw_cylinder(out, horse_ctx.model, knee, cannon,
                  (knee_r + cannon_r) * 0.5F, shin_color, 1.0F, 6);

    QVector3D const hoof_joint_color =
        darken(shin_color, is_rear ? 0.92F : 0.94F);

    float const sock =
        sockChance > 0.78F ? 1.0F : (sockChance > 0.58F ? 0.55F : 0.0F);
    QVector3D const distal_color =
        (sock > 0.0F) ? lighten(v.coatColor, 1.18F) : v.coatColor * 0.92F;
    float const t_sock = smoothstep(0.0F, 1.0F, sock);
    QVector3D const pastern_color =
        lerp(hoof_joint_color, distal_color, t_sock * 0.8F);

    draw_cylinder(out, horse_ctx.model, cannon, fetlock,
                  (cannon_r * 0.90F + pastern_r) * 0.5F,
                  lerp(hoof_joint_color, pastern_color, 0.5F), 1.0F, 6);

    QVector3D const fetlock_color = lerp(pastern_color, distal_color, 0.25F);

    QVector3D const hoof_color = v.hoof_color;
    float const hoof_width = pastern_r * (is_rear ? 1.55F : 1.45F);
    float const hoof_depth = hoof_width * (is_rear ? 0.90F : 1.05F);
    render_hoof(hoof_top, d.hoofHeight, hoof_width, hoof_depth, hoof_color,
                is_rear);

    if (sock > 0.0F) {
      QVector3D const feather_tip = lerp(fetlock, hoof_top, 0.35F) +
                                    QVector3D(0.0F, -pastern_r * 0.60F, 0.0F);
      drawCone(out, horse_ctx.model, feather_tip, fetlock, pastern_r * 0.85F,
               lerp(distal_color, v.coatColor, 0.25F), 0.85F, 6);
    }
  };

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.05F, d.bodyLength * 0.32F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.02F, -d.bodyLength * 0.30F);

  float const front_forward_bias = d.bodyLength * 0.16F;
  float const front_bias_offset = d.bodyLength * 0.035F;
  draw_leg(front_anchor, 1.0F, front_forward_bias + front_bias_offset,
           g.frontLegPhase, sock_chance_fl);
  draw_leg(front_anchor, -1.0F, front_forward_bias - front_bias_offset,
           g.frontLegPhase + 0.48F, sock_chance_fr);
  float const rear_forward_bias = -d.bodyLength * 0.16F;
  float const rear_bias_offset = d.bodyLength * 0.032F;
  draw_leg(rear_anchor, 1.0F, rear_forward_bias - rear_bias_offset,
           g.rearLegPhase, sock_chance_rl);
  draw_leg(rear_anchor, -1.0F, rear_forward_bias + rear_bias_offset,
           g.rearLegPhase + 0.52F, sock_chance_rr);

  QVector3D const bit_left =
      muzzle_center + QVector3D(d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);
  QVector3D const bit_right =
      muzzle_center + QVector3D(-d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);
  mount.rein_bit_left = bit_left;
  mount.rein_bit_right = bit_right;

  HorseBodyFrames body_frames;
  QVector3D const forward(0.0F, 0.0F, 1.0F);
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const right(1.0F, 0.0F, 0.0F);

  body_frames.head.origin = head_center;
  body_frames.head.right = right;
  body_frames.head.up = up;
  body_frames.head.forward = forward;

  body_frames.neck_base.origin = neck_base;
  body_frames.neck_base.right = right;
  body_frames.neck_base.up = up;
  body_frames.neck_base.forward = forward;

  QVector3D const withers_pos =
      chest_center +
      QVector3D(0.0F, d.bodyHeight * 0.55F, -d.bodyLength * 0.06F);
  body_frames.withers.origin = withers_pos;
  body_frames.withers.right = right;
  body_frames.withers.up = up;
  body_frames.withers.forward = forward;

  body_frames.back_center.origin = mount.saddle_center;
  body_frames.back_center.right = right;
  body_frames.back_center.up = up;
  body_frames.back_center.forward = forward;

  QVector3D const croup_pos =
      rump_center +
      QVector3D(0.0F, d.bodyHeight * 0.46F, -d.bodyLength * 0.18F);
  body_frames.croup.origin = croup_pos;
  body_frames.croup.right = right;
  body_frames.croup.up = up;
  body_frames.croup.forward = forward;

  body_frames.chest.origin = chest_center;
  body_frames.chest.right = right;
  body_frames.chest.up = up;
  body_frames.chest.forward = forward;

  body_frames.barrel.origin = barrel_center;
  body_frames.barrel.right = right;
  body_frames.barrel.up = up;
  body_frames.barrel.forward = forward;

  body_frames.rump.origin = rump_center;
  body_frames.rump.right = right;
  body_frames.rump.up = up;
  body_frames.rump.forward = forward;

  QVector3D const tail_base_pos =
      rump_center + QVector3D(0.0F, d.bodyHeight * 0.20F, -100.05F);
  body_frames.tail_base.origin = tail_base_pos;
  body_frames.tail_base.right = right;
  body_frames.tail_base.up = up;
  body_frames.tail_base.forward = forward;

  body_frames.muzzle.origin = muzzle_center;
  body_frames.muzzle.right = right;
  body_frames.muzzle.up = up;
  body_frames.muzzle.forward = forward;

  drawAttachments(horse_ctx, anim, rider_ctx, profile, mount, phase, bob,
                  rein_slack, body_frames, out);
}

void HorseRendererBase::renderSimplified(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
    const MountedAttachmentFrame *shared_mount,
    const HorseMotionSample *shared_motion, ISubmitter &out) const {

  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;
  const HorseGait &g = profile.gait;

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(profile, anim, rider_ctx);
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);
  if (!shared_mount) {
    apply_mount_vertical_offset(mount, bob);
  }

  DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  horse_ctx.model.translate(mount.ground_offset);

  QVector3D const barrel_center(0.0F, d.barrel_centerY + bob, 0.0F);

  {
    QMatrix4x4 body = horse_ctx.model;
    body.translate(barrel_center);
    body.scale(d.bodyWidth * 1.0F, d.bodyHeight * 0.85F, d.bodyLength * 0.80F);
    out.mesh(getUnitSphere(), body, v.coatColor, nullptr, 1.0F, 6);
  }

  QVector3D const neck_base =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.35F, d.bodyLength * 0.35F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, d.neckRise, d.neckLength);
  draw_cylinder(out, horse_ctx.model, neck_base, neck_top, d.bodyWidth * 0.40F,
                v.coatColor, 1.0F);

  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.headHeight * 0.10F, d.headLength * 0.40F);
  {
    QMatrix4x4 head = horse_ctx.model;
    head.translate(head_center);
    head.scale(d.headWidth * 0.90F, d.headHeight * 0.85F, d.headLength * 0.75F);
    out.mesh(getUnitSphere(), head, v.coatColor, nullptr, 1.0F);
  }

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.05F, d.bodyLength * 0.30F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.02F, -d.bodyLength * 0.28F);

  auto draw_simple_leg = [&](const QVector3D &anchor, float lateralSign,
                             float forwardBias, float phase_offset) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.strideSwing * 0.6F + forwardBias;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.strideLift * 0.8F : 0.0F;
    }

    float const shoulder_out = d.bodyWidth * 0.45F;
    QVector3D shoulder =
        anchor + QVector3D(lateralSign * shoulder_out, lift * 0.05F, stride);

    float const leg_length = d.legLength * 0.85F;
    QVector3D const foot = shoulder + QVector3D(0.0F, -leg_length + lift, 0.0F);

    draw_cylinder(out, horse_ctx.model, shoulder, foot, d.bodyWidth * 0.22F,
                  v.coatColor * 0.85F, 1.0F, 6);

    QMatrix4x4 hoof = horse_ctx.model;
    hoof.translate(foot);
    hoof.scale(d.bodyWidth * 0.28F, d.hoofHeight, d.bodyWidth * 0.30F);
    out.mesh(getUnitCylinder(), hoof, v.hoof_color, nullptr, 1.0F, 8);
  };

  draw_simple_leg(front_anchor, 1.0F, d.bodyLength * 0.15F, g.frontLegPhase);
  draw_simple_leg(front_anchor, -1.0F, d.bodyLength * 0.15F,
                  g.frontLegPhase + 0.48F);
  draw_simple_leg(rear_anchor, 1.0F, -d.bodyLength * 0.15F, g.rearLegPhase);
  draw_simple_leg(rear_anchor, -1.0F, -d.bodyLength * 0.15F,
                  g.rearLegPhase + 0.52F);
}

void HorseRendererBase::renderMinimal(const DrawContext &ctx,
                                      HorseProfile &profile,
                                      const HorseMotionSample *shared_motion,
                                      ISubmitter &out) const {

  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;

  MountedAttachmentFrame mount = compute_mount_frame(profile);
  apply_mount_vertical_offset(mount, bob);

  DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  horse_ctx.model.translate(mount.ground_offset);

  QVector3D const center(0.0F, d.barrel_centerY + bob, 0.0F);

  QMatrix4x4 body = horse_ctx.model;
  body.translate(center);
  body.scale(d.bodyWidth * 1.2F, d.bodyHeight + d.neckRise * 0.5F,
             d.bodyLength + d.headLength * 0.5F);
  out.mesh(getUnitSphere(), body, v.coatColor, nullptr, 1.0F, 6);

  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.bodyLength * 0.25F : -d.bodyLength * 0.25F;

    QVector3D const top = center + QVector3D(x_sign * d.bodyWidth * 0.40F,
                                             -d.bodyHeight * 0.3F, z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.legLength * 0.60F, 0.0F);

    draw_cylinder(out, horse_ctx.model, top, bottom, d.bodyWidth * 0.15F,
                  v.coatColor * 0.75F, 1.0F, 6);
  }
}

void HorseRendererBase::render(const DrawContext &ctx,
                               const AnimationInputs &anim,
                               const HumanoidAnimationContext &rider_ctx,
                               HorseProfile &profile,
                               const MountedAttachmentFrame *shared_mount,
                               const ReinState *shared_reins,
                               const HorseMotionSample *shared_motion,
                               ISubmitter &out, HorseLOD lod) const {

  ++s_horseRenderStats.horsesTotal;

  if (lod == HorseLOD::Billboard) {
    ++s_horseRenderStats.horsesSkippedLOD;
    return;
  }

  ++s_horseRenderStats.horsesRendered;

  switch (lod) {
  case HorseLOD::Full:
    ++s_horseRenderStats.lodFull;
    renderFull(ctx, anim, rider_ctx, profile, shared_mount, shared_reins,
               shared_motion, out);
    break;

  case HorseLOD::Reduced:
    ++s_horseRenderStats.lodReduced;
    renderSimplified(ctx, anim, rider_ctx, profile, shared_mount, shared_motion,
                     out);
    break;

  case HorseLOD::Minimal:
    ++s_horseRenderStats.lodMinimal;
    renderMinimal(ctx, profile, shared_motion, out);
    break;

  case HorseLOD::Billboard:

    break;
  }
}

void HorseRendererBase::render(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
    const MountedAttachmentFrame *shared_mount, const ReinState *shared_reins,
    const HorseMotionSample *shared_motion, ISubmitter &out) const {
  render(ctx, anim, rider_ctx, profile, shared_mount, shared_reins,
         shared_motion, out, HorseLOD::Full);
}

} // namespace Render::GL
