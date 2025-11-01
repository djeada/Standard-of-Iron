#include "rig.h"

#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../humanoid/rig.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

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

inline void drawCylinder(ISubmitter &out, const QMatrix4x4 &model,
                         const QVector3D &a, const QVector3D &b, float radius,
                         const QVector3D &color, float alpha = 1.0F) {
  out.mesh(getUnitCylinder(), cylinderBetween(model, a, b, radius), color,
           nullptr, alpha);
}

inline void drawCone(ISubmitter &out, const QMatrix4x4 &model,
                     const QVector3D &tip, const QVector3D &base, float radius,
                     const QVector3D &color, float alpha = 1.0F) {
  out.mesh(getUnitCone(), coneFromTo(model, tip, base, radius), color, nullptr,
           alpha);
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
  d.barrel_centerY = randBetween(seed, 0x78U, 0.05F, 0.09F);

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

auto compute_mount_frame(const HorseProfile &profile) -> HorseMountFrame {
  const HorseDimensions &d = profile.dims;
  HorseMountFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);

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

  frame.rein_attach_left =
      frame.saddle_center + QVector3D(-d.bodyWidth * 0.62F,
                                      -d.saddleThickness * 0.32F,
                                      d.seatForwardOffset * 0.10F);
  frame.rein_attach_right =
      frame.saddle_center + QVector3D(d.bodyWidth * 0.62F,
                                      -d.saddleThickness * 0.32F,
                                      d.seatForwardOffset * 0.10F);

  QVector3D const neck_top(0.0F,
                           d.barrel_centerY + d.bodyHeight * 0.65F + d.neckRise,
                           d.bodyLength * 0.25F);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.headHeight * 0.10F, d.headLength * 0.40F);
  frame.bridle_base = head_center + QVector3D(0.0F, -d.headHeight * 0.18F,
                                              d.headLength * 0.58F);

  return frame;
}

void HorseRendererBase::render(const DrawContext &ctx,
                               const AnimationInputs &anim,
                               const HumanoidAnimationContext &rider_ctx,
                               const HorseProfile &profile,
                               ISubmitter &out) const {
  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;
  const HorseGait &g = profile.gait;
  HorseMountFrame mount = compute_mount_frame(profile);

  const bool rider_has_motion =
      rider_ctx.is_walking() || rider_ctx.is_running();
  const bool is_moving = rider_has_motion || anim.isMoving;
  const float rider_intensity = rider_ctx.locomotion_normalized_speed();

  float cycle = std::max(0.20F, (rider_ctx.gait.cycle_time > 0.0001F)
                                    ? rider_ctx.gait.cycle_time
                                    : g.cycleTime);
  float phase = 0.0F;
  float bob = 0.0F;

  if (is_moving) {
    if (rider_ctx.gait.cycle_time > 0.0001F) {
      phase = rider_ctx.gait.cycle_phase;
    } else {
      phase = std::fmod(anim.time / cycle, 1.0F);
    }
    float const bob_amp =
        d.idleBobAmplitude +
        rider_intensity * (d.moveBobAmplitude - d.idleBobAmplitude);
    bob = std::sin(phase * 2.0F * k_pi) * bob_amp;
  } else {
    phase = std::fmod(anim.time * 0.25F, 1.0F);
    bob = std::sin(phase * 2.0F * k_pi) * d.idleBobAmplitude;
  }

  float const head_nod = is_moving ? std::sin((phase + 0.25F) * 2.0F * k_pi) *
                                         (0.02F + rider_intensity * 0.03F)
                                   : std::sin(anim.time * 1.5F) * 0.01F;

  uint32_t const vhash = colorHash(v.coatColor);
  float const sock_chance_fl = hash01(vhash ^ 0x101U);
  float const sock_chance_fr = hash01(vhash ^ 0x202U);
  float const sock_chance_rl = hash01(vhash ^ 0x303U);
  float const sock_chance_rr = hash01(vhash ^ 0x404U);
  bool const has_blaze = hash01(vhash ^ 0x505U) > 0.82F;
  float rein_slack = hash01(vhash ^ 0x707U) * 0.08F + 0.02F;
  float rein_tension = rider_intensity;
  if (rider_ctx.gait.has_target) {
    rein_tension += 0.25F;
  }
  if (rider_ctx.is_attacking()) {
    rein_tension += 0.35F;
  }
  rein_tension = std::clamp(rein_tension, 0.0F, 1.0F);
  rein_slack = std::max(0.01F, rein_slack * (1.0F - rein_tension));

  const float coat_seed_a = hash01(vhash ^ 0x701U);
  const float coat_seed_b = hash01(vhash ^ 0x702U);
  const float coat_seed_c = hash01(vhash ^ 0x703U);
  const float coat_seed_d = hash01(vhash ^ 0x704U);

  QVector3D const barrel_center(0.0F, d.barrel_centerY + bob, 0.0F);
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
    QMatrix4x4 chest = ctx.model;
    chest.translate(chest_center);
    chest.scale(d.bodyWidth * 1.12F, d.bodyHeight * 0.95F,
                d.bodyLength * 0.36F);
    QVector3D const chest_color =
        coatGradient(v.coatColor, 0.75F, 0.20F, coat_seed_a);
    out.mesh(getUnitSphere(), chest, chest_color, nullptr, 1.0F);
  }

  {
    QMatrix4x4 withers = ctx.model;
    withers.translate(chest_center + QVector3D(0.0F, d.bodyHeight * 0.55F,
                                               -d.bodyLength * 0.03F));
    withers.scale(d.bodyWidth * 0.75F, d.bodyHeight * 0.35F,
                  d.bodyLength * 0.18F);
    QVector3D const wither_color =
        coatGradient(v.coatColor, 0.88F, 0.35F, coat_seed_b);
    out.mesh(getUnitSphere(), withers, wither_color, nullptr, 1.0F);
  }

  {
    QMatrix4x4 belly = ctx.model;
    belly.translate(belly_center);
    belly.scale(d.bodyWidth * 0.98F, d.bodyHeight * 0.64F,
                d.bodyLength * 0.40F);
    QVector3D const belly_color =
        coatGradient(v.coatColor, 0.25F, -0.10F, coat_seed_c);
    out.mesh(getUnitSphere(), belly, belly_color, nullptr, 1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 ribs = ctx.model;
    ribs.translate(barrel_center + QVector3D(side * d.bodyWidth * 0.90F,
                                             -d.bodyHeight * 0.10F,
                                             -d.bodyLength * 0.05F));
    ribs.scale(d.bodyWidth * 0.38F, d.bodyHeight * 0.42F, d.bodyLength * 0.30F);
    QVector3D const rib_color =
        coatGradient(v.coatColor, 0.45F, 0.05F, coat_seed_d + side * 0.05F);
    out.mesh(getUnitSphere(), ribs, rib_color, nullptr, 1.0F);
  }

  {
    QMatrix4x4 rump = ctx.model;
    rump.translate(rump_center);
    rump.scale(d.bodyWidth * 1.18F, d.bodyHeight * 1.00F, d.bodyLength * 0.36F);
    QVector3D const rump_color =
        coatGradient(v.coatColor, 0.62F, -0.28F, coat_seed_a * 0.7F);
    out.mesh(getUnitSphere(), rump, rump_color, nullptr, 1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 hip = ctx.model;
    hip.translate(rump_center + QVector3D(side * d.bodyWidth * 0.95F,
                                          -d.bodyHeight * 0.10F,
                                          -d.bodyLength * 0.08F));
    hip.scale(d.bodyWidth * 0.45F, d.bodyHeight * 0.42F, d.bodyLength * 0.26F);
    QVector3D const hip_color =
        coatGradient(v.coatColor, 0.58F, -0.18F, coat_seed_b + side * 0.06F);
    out.mesh(getUnitSphere(), hip, hip_color, nullptr, 1.0F);

    QMatrix4x4 haunch = ctx.model;
    haunch.translate(rump_center + QVector3D(side * d.bodyWidth * 0.88F,
                                             d.bodyHeight * 0.24F,
                                             -d.bodyLength * 0.20F));
    haunch.scale(QVector3D(d.bodyWidth * 0.32F, d.bodyHeight * 0.28F,
                           d.bodyLength * 0.18F));
    QVector3D const haunch_color =
        coatGradient(v.coatColor, 0.72F, -0.26F, coat_seed_c + side * 0.04F);
    out.mesh(getUnitSphere(), haunch, lighten(haunch_color, 1.02F), nullptr,
             1.0F);
  }

  QVector3D withers_peak = chest_center + QVector3D(0.0F, d.bodyHeight * 0.62F,
                                                    -d.bodyLength * 0.06F);
  QVector3D croup_peak = rump_center + QVector3D(0.0F, d.bodyHeight * 0.46F,
                                                 -d.bodyLength * 0.18F);

  {
    QMatrix4x4 spine = ctx.model;
    spine.translate(lerp(withers_peak, croup_peak, 0.42F));
    spine.scale(QVector3D(d.bodyWidth * 0.50F, d.bodyHeight * 0.14F,
                          d.bodyLength * 0.54F));
    QVector3D const spine_color =
        coatGradient(v.coatColor, 0.74F, -0.06F, coat_seed_d * 0.92F);
    out.mesh(getUnitSphere(), spine, spine_color, nullptr, 1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const scapula_top =
        withers_peak + QVector3D(side * d.bodyWidth * 0.52F,
                                 d.bodyHeight * 0.08F, d.bodyLength * 0.06F);
    QVector3D const scapula_base =
        chest_center + QVector3D(side * d.bodyWidth * 0.70F,
                                 -d.bodyHeight * 0.02F, d.bodyLength * 0.06F);
    QVector3D const scapula_mid = lerp(scapula_top, scapula_base, 0.55F);
    drawCylinder(
        out, ctx.model, scapula_top, scapula_mid, d.bodyWidth * 0.18F,
        coatGradient(v.coatColor, 0.82F, 0.16F, coat_seed_a + side * 0.05F));

    QMatrix4x4 shoulder_cap = ctx.model;
    shoulder_cap.translate(scapula_base + QVector3D(0.0F, d.bodyHeight * 0.04F,
                                                    d.bodyLength * 0.02F));
    shoulder_cap.scale(QVector3D(d.bodyWidth * 0.32F, d.bodyHeight * 0.24F,
                                 d.bodyLength * 0.18F));
    out.mesh(
        getUnitSphere(), shoulder_cap,
        coatGradient(v.coatColor, 0.66F, 0.12F, coat_seed_b + side * 0.07F),
        nullptr, 1.0F);
  }

  {
    QMatrix4x4 sternum = ctx.model;
    sternum.translate(barrel_center + QVector3D(0.0F, -d.bodyHeight * 0.40F,
                                                d.bodyLength * 0.28F));
    sternum.scale(QVector3D(d.bodyWidth * 0.50F, d.bodyHeight * 0.14F,
                            d.bodyLength * 0.12F));
    out.mesh(getUnitSphere(), sternum,
             coatGradient(v.coatColor, 0.18F, 0.18F, coat_seed_a * 0.4F),
             nullptr, 1.0F);
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
           cylinderBetween(ctx.model, neck_base, neck_mid, neck_radius * 1.00F),
           neck_color_base, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, neck_mid, neck_top, neck_radius * 0.86F),
           lighten(neck_color_base, 1.03F), nullptr, 1.0F);

  {
    QVector3D const jugular_start =
        lerp(neck_base, neck_top, 0.42F) + QVector3D(d.bodyWidth * 0.18F,
                                                     -d.bodyHeight * 0.06F,
                                                     d.bodyLength * 0.04F);
    QVector3D const jugular_end =
        jugular_start +
        QVector3D(0.0F, -d.bodyHeight * 0.24F, d.bodyLength * 0.06F);
    drawCylinder(out, ctx.model, jugular_start, jugular_end,
                 neck_radius * 0.18F, lighten(neck_color_base, 1.08F), 0.85F);
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
    drawCone(out, ctx.model, tip, spine, d.bodyWidth * lerp(0.25F, 0.12F, t),
             mane_color, 1.0F);
  }

  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.headHeight * (0.10F - head_nod * 0.15F),
                           d.headLength * 0.40F);

  {
    QMatrix4x4 skull = ctx.model;
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
    QMatrix4x4 cheek = ctx.model;
    cheek.translate(head_center + QVector3D(side * d.headWidth * 0.55F,
                                            -d.headHeight * 0.15F, 0.0F));
    cheek.scale(d.headWidth * 0.45F, d.headHeight * 0.50F,
                d.headLength * 0.60F);
    QVector3D const cheek_color =
        coatGradient(v.coatColor, 0.70F, 0.18F, coat_seed_a * 0.9F);
    out.mesh(getUnitSphere(), cheek, cheek_color, nullptr, 1.0F);
  }

  QVector3D const muzzle_center =
      head_center +
      QVector3D(0.0F, -d.headHeight * 0.18F, d.headLength * 0.58F);
  {
    QMatrix4x4 muzzle = ctx.model;
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
             coneFromTo(ctx.model, left_base + inward, left_base,
                        d.headWidth * 0.11F),
             darken(v.muzzleColor, 0.6F), nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, right_base + inward, right_base,
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

  out.mesh(
      getUnitCone(),
      coneFromTo(ctx.model, ear_tip_left, ear_base_left, d.headWidth * 0.11F),
      v.mane_color, nullptr, 1.0F);
  out.mesh(
      getUnitCone(),
      coneFromTo(ctx.model, ear_tip_right, ear_base_right, d.headWidth * 0.11F),
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
      QMatrix4x4 eye = ctx.model;
      eye.translate(pos);
      eye.scale(d.headWidth * 0.14F);
      out.mesh(getUnitSphere(), eye, eye_base_color, nullptr, 1.0F);
    }
    {

      QMatrix4x4 pupil = ctx.model;
      pupil.translate(pos + QVector3D(0.0F, 0.0F, d.headWidth * 0.04F));
      pupil.scale(d.headWidth * 0.05F);
      out.mesh(getUnitSphere(), pupil, QVector3D(0.03F, 0.03F, 0.03F), nullptr,
               1.0F);
    }
    {

      QMatrix4x4 spec = ctx.model;
      spec.translate(pos + QVector3D(d.headWidth * 0.03F, d.headWidth * 0.03F,
                                     d.headWidth * 0.03F));
      spec.scale(d.headWidth * 0.02F);
      out.mesh(getUnitSphere(), spec, QVector3D(0.95F, 0.95F, 0.95F), nullptr,
               1.0F);
    }
  };
  draw_eye(eye_left);
  draw_eye(eye_right);

  if (has_blaze) {
    QMatrix4x4 blaze = ctx.model;
    blaze.translate(head_center + QVector3D(0.0F, d.headHeight * 0.15F,
                                            d.headLength * 0.10F));
    blaze.scale(d.headWidth * 0.22F, d.headHeight * 0.32F,
                d.headLength * 0.10F);
    out.mesh(getUnitSphere(), blaze, QVector3D(0.92F, 0.92F, 0.90F), nullptr,
             1.0F);
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
  drawCylinder(out, ctx.model, bridle_base, cheek_anchor_left,
               d.headWidth * 0.07F, tack_color);
  drawCylinder(out, ctx.model, bridle_base, cheek_anchor_right,
               d.headWidth * 0.07F, tack_color);
  drawCylinder(out, ctx.model, cheek_anchor_left, brow, d.headWidth * 0.05F,
               tack_color);
  drawCylinder(out, ctx.model, cheek_anchor_right, brow, d.headWidth * 0.05F,
               tack_color);

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
             cylinderBetween(ctx.model, seg_start, seg_end,
                             d.headWidth * (0.10F * (1.0F - t * 0.4F))),
             v.mane_color * (0.98F + t * 0.05F), nullptr, 1.0F);
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
      drawCone(out, ctx.model, strand_tip, strand_base, d.headWidth * 0.10F,
               v.mane_color * (0.94F + 0.03F * i), 0.96F);
    }
  }

  QVector3D const tail_base =
      rump_center +
      QVector3D(0.0F, d.bodyHeight * 0.36F, -d.bodyLength * 0.48F);
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
    drawCylinder(out, ctx.model, prev_tail, p, radius, tail_color);
    prev_tail = p;
  }

  {
    QMatrix4x4 tail_knot = ctx.model;
    tail_knot.translate(tail_base + QVector3D(0.0F, -d.bodyHeight * 0.06F,
                                              -d.bodyLength * 0.02F));
    tail_knot.scale(QVector3D(d.bodyWidth * 0.24F, d.bodyWidth * 0.18F,
                              d.bodyWidth * 0.20F));
    out.mesh(getUnitSphere(), tail_knot, lighten(tail_color, 0.92F), nullptr,
             1.0F);
  }

  for (int i = 0; i < 3; ++i) {
    float const spread = (i - 1) * d.bodyWidth * 0.14F;
    QVector3D const fan_base =
        tail_end +
        QVector3D(spread * 0.15F, -d.bodyWidth * 0.05F, -d.tailLength * 0.08F);
    QVector3D const fan_tip =
        fan_base +
        QVector3D(spread, -d.tailLength * 0.32F, -d.tailLength * 0.22F);
    drawCone(out, ctx.model, fan_tip, fan_base, d.bodyWidth * 0.24F,
             tail_color * (0.96F + 0.02F * i), 0.88F);
  }

  auto render_hoof = [&](const QVector3D &hoof_top,
                         const QVector3D &hoof_bottom, float wallRadius,
                         const QVector3D &hoof_color, bool is_rear) {
    QVector3D const wall_tint = lighten(hoof_color, is_rear ? 1.04F : 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, hoof_top, hoof_bottom, wallRadius),
             wall_tint, nullptr, 1.0F);

    QVector3D const toe =
        hoof_bottom + QVector3D(0.0F, -d.hoofHeight * 0.14F, 0.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, toe, hoof_bottom, wallRadius * 0.90F),
             wall_tint * 0.96F, nullptr, 1.0F);

    QMatrix4x4 sole = ctx.model;
    sole.translate(lerp(hoof_top, hoof_bottom, 0.88F) +
                   QVector3D(0.0F, -d.hoofHeight * 0.05F, 0.0F));
    sole.scale(
        QVector3D(wallRadius * 1.08F, wallRadius * 0.28F, wallRadius * 1.02F));
    out.mesh(getUnitSphere(), sole, lighten(hoof_color, 1.12F), nullptr, 1.0F);

    QMatrix4x4 coronet = ctx.model;
    coronet.translate(lerp(hoof_top, hoof_bottom, 0.12F));
    coronet.scale(
        QVector3D(wallRadius * 1.05F, wallRadius * 0.24F, wallRadius * 1.05F));
    out.mesh(getUnitSphere(), coronet, lighten(hoof_color, 1.06F), nullptr,
             1.0F);
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

    bool const tighten_legs = is_moving;
    float const shoulder_out = d.bodyWidth * (tighten_legs ? 0.44F : 0.58F);
    QVector3D shoulder = anchor + QVector3D(lateralSign * shoulder_out,
                                            0.05F + lift * 0.05F, stride);
    bool const is_rear = (forwardBias < 0.0F);

    float const gallop_angle = leg_phase * 2.0F * k_pi;
    float const hip_swing = is_moving ? std::sin(gallop_angle) : 0.0F;
    float const lift_factor =
        is_moving
            ? std::max(0.0F,
                       std::sin(gallop_angle + (is_rear ? 0.35F : -0.25F)))
            : 0.0F;

    shoulder.setZ(shoulder.z() + hip_swing * (is_rear ? -0.12F : 0.10F));
    if (tighten_legs) {
      shoulder.setX(shoulder.x() - lateralSign * lift_factor * 0.05F);
    }

    float const thigh_length = d.legLength * (is_rear ? 0.62F : 0.56F);
    float const hip_pitch = hip_swing * (is_rear ? 0.62F : 0.50F);
    float const inward_lean =
        tighten_legs ? (-0.06F - lift_factor * 0.045F) : -0.012F;
    QVector3D thigh_dir(lateralSign * inward_lean, -std::cos(hip_pitch) * 0.90F,
                        (is_rear ? -1.0F : 1.0F) * std::sin(hip_pitch) * 0.65F);
    if (thigh_dir.lengthSquared() > 1e-6F) {
      thigh_dir.normalize();
    }

    QVector3D knee = shoulder + thigh_dir * thigh_length;
    knee.setY(knee.y() + lift_factor * thigh_length * 0.28F);

    QVector3D girdle_top =
        (is_rear ? croup_peak : withers_peak) +
        QVector3D(lateralSign * d.bodyWidth * (is_rear ? 0.44F : 0.48F),
                  is_rear ? -d.bodyHeight * 0.06F : d.bodyHeight * 0.04F,
                  (is_rear ? -d.bodyLength * 0.06F : d.bodyLength * 0.05F));
    girdle_top.setZ(girdle_top.z() + hip_swing * (is_rear ? -0.08F : 0.05F));
    girdle_top.setX(girdle_top.x() - lateralSign * lift_factor * 0.03F);

    QVector3D const socket =
        shoulder +
        QVector3D(0.0F, d.bodyWidth * 0.12F,
                  is_rear ? -d.bodyLength * 0.03F : d.bodyLength * 0.02F);
    drawCylinder(out, ctx.model, girdle_top, socket,
                 d.bodyWidth * (is_rear ? 0.20F : 0.18F),
                 coatGradient(v.coatColor, is_rear ? 0.70F : 0.80F,
                              is_rear ? -0.20F : 0.22F,
                              coat_seed_b + lateralSign * 0.03F));

    QMatrix4x4 socket_cap = ctx.model;
    socket_cap.translate(socket + QVector3D(0.0F, -d.bodyWidth * 0.04F,
                                            is_rear ? -d.bodyLength * 0.02F
                                                    : d.bodyLength * 0.03F));
    socket_cap.scale(QVector3D(d.bodyWidth * (is_rear ? 0.36F : 0.32F),
                               d.bodyWidth * 0.28F, d.bodyLength * 0.18F));
    out.mesh(getUnitSphere(), socket_cap,
             coatGradient(v.coatColor, is_rear ? 0.60F : 0.68F,
                          is_rear ? -0.24F : 0.18F,
                          coat_seed_c + lateralSign * 0.02F),
             nullptr, 1.0F);

    float const knee_flex =
        is_moving
            ? clamp01(std::sin(gallop_angle + (is_rear ? 0.65F : -0.45F)) *
                          0.55F +
                      0.42F)
            : 0.32F;

    float const forearm_length = d.legLength * 0.30F;
    float const bend_cos = std::cos(knee_flex * k_pi * 0.5F);
    float const bend_sin = std::sin(knee_flex * k_pi * 0.5F);
    QVector3D forearm_dir(0.0F, -bend_cos,
                          (is_rear ? -1.0F : 1.0F) * bend_sin * 0.85F);
    if (forearm_dir.lengthSquared() < 1e-6F) {
      forearm_dir = QVector3D(0.0F, -1.0F, 0.0F);
    } else {
      forearm_dir.normalize();
    }
    QVector3D const cannon = knee + forearm_dir * forearm_length;

    float const pastern_length = d.legLength * 0.12F;
    QVector3D const fetlock = cannon + QVector3D(0.0F, -pastern_length, 0.0F);

    float const hoof_pitch =
        is_moving ? (-0.20F + std::sin(leg_phase * 2.0F * k_pi +
                                       (is_rear ? 0.2F : -0.1F)) *
                                  0.10F)
                  : 0.0F;
    QVector3D const hoof_dir =
        rotateAroundZ(QVector3D(0.0F, -1.0F, 0.0F), hoof_pitch);
    QVector3D const hoof_top = fetlock;
    QVector3D const hoof_bottom = hoof_top + hoof_dir * d.hoofHeight;

    float const thigh_belly_r = d.bodyWidth * (is_rear ? 0.58F : 0.50F);
    float const knee_r = d.bodyWidth * (is_rear ? 0.22F : 0.20F);
    float const cannon_r = d.bodyWidth * 0.16F;
    float const pastern_r = d.bodyWidth * 0.11F;

    QVector3D const thigh_belly = shoulder + (knee - shoulder) * 0.62F;

    QVector3D const thigh_color = coatGradient(
        v.coatColor, is_rear ? 0.48F : 0.58F, is_rear ? -0.22F : 0.18F,
        coat_seed_a + lateralSign * 0.07F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, thigh_belly, shoulder, thigh_belly_r),
             thigh_color, nullptr, 1.0F);

    {
      QMatrix4x4 muscle = ctx.model;
      muscle.translate(thigh_belly +
                       QVector3D(0.0F, 0.0F, is_rear ? -0.015F : 0.020F));
      muscle.scale(thigh_belly_r * QVector3D(1.05F, 0.85F, 0.92F));
      out.mesh(getUnitSphere(), muscle, lighten(thigh_color, 1.03F), nullptr,
               1.0F);
    }

    QVector3D const knee_color = darken(thigh_color, 0.96F);
    out.mesh(getUnitCone(), coneFromTo(ctx.model, knee, thigh_belly, knee_r),
             knee_color, nullptr, 1.0F);

    {
      QMatrix4x4 joint = ctx.model;
      joint.translate(knee + QVector3D(0.0F, 0.0F, is_rear ? -0.028F : 0.034F));
      joint.scale(QVector3D(knee_r * 1.18F, knee_r * 1.06F, knee_r * 1.36F));
      out.mesh(getUnitSphere(), joint, darken(knee_color, 0.90F), nullptr,
               1.0F);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, knee, cannon, cannon_r),
             darken(thigh_color, 0.93F), nullptr, 1.0F);

    {
      QMatrix4x4 tendon = ctx.model;
      tendon.translate(
          lerp(knee, cannon, 0.55F) +
          QVector3D(0.0F, 0.0F,
                    is_rear ? -cannon_r * 0.35F : cannon_r * 0.35F));
      tendon.scale(
          QVector3D(cannon_r * 0.45F, cannon_r * 0.95F, cannon_r * 0.55F));
      out.mesh(getUnitSphere(), tendon,
               darken(thigh_color, is_rear ? 0.88F : 0.90F), nullptr, 1.0F);
    }

    {
      QMatrix4x4 joint = ctx.model;
      joint.translate(fetlock);
      joint.scale(
          QVector3D(pastern_r * 1.12F, pastern_r * 1.05F, pastern_r * 1.26F));
      out.mesh(getUnitSphere(), joint, darken(thigh_color, 0.92F), nullptr,
               1.0F);
    }

    float const sock =
        sockChance > 0.78F ? 1.0F : (sockChance > 0.58F ? 0.55F : 0.0F);
    QVector3D const distal_color =
        (sock > 0.0F) ? lighten(v.coatColor, 1.18F) : v.coatColor * 0.92F;
    float const t_sock = smoothstep(0.0F, 1.0F, sock);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cannon, fetlock, pastern_r * 1.05F),
             lerp(v.coatColor * 0.94F, distal_color, t_sock * 0.8F), nullptr,
             1.0F);

    QVector3D const hoof_color = v.hoof_color;
    render_hoof(hoof_top, hoof_bottom, pastern_r * 0.96F, hoof_color, is_rear);

    if (sock > 0.0F) {
      QVector3D const feather_tip = lerp(fetlock, hoof_top, 0.35F) +
                                    QVector3D(0.0F, -pastern_r * 0.60F, 0.0F);
      drawCone(out, ctx.model, feather_tip, fetlock, pastern_r * 0.85F,
               lerp(distal_color, v.coatColor, 0.25F), 0.85F);
    }
  };

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.05F, d.bodyLength * 0.28F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.bodyHeight * 0.02F, -d.bodyLength * 0.32F);

  draw_leg(front_anchor, 1.0F, d.bodyLength * 0.30F, g.frontLegPhase,
           sock_chance_fl);
  draw_leg(front_anchor, -1.0F, d.bodyLength * 0.30F, g.frontLegPhase + 0.5F,
           sock_chance_fr);
  draw_leg(rear_anchor, 1.0F, -d.bodyLength * 0.28F, g.rearLegPhase,
           sock_chance_rl);
  draw_leg(rear_anchor, -1.0F, -d.bodyLength * 0.28F, g.rearLegPhase + 0.5F,
           sock_chance_rr);

  float const saddle_top = d.saddle_height;
  QVector3D saddle_center(0.0F, saddle_top - d.saddleThickness * 0.35F,
                          -d.bodyLength * 0.05F + d.seatForwardOffset * 0.25F);
  mount.saddle_center = saddle_center;
  mount.seat_position =
      saddle_center + QVector3D(0.0F, d.saddleThickness * 0.32F, 0.0F);
  {
    QMatrix4x4 saddle = ctx.model;
    saddle.translate(saddle_center);
    saddle.scale(d.bodyWidth * 1.10F, d.saddleThickness * 1.05F,
                 d.bodyLength * 0.34F);
    out.mesh(getUnitSphere(), saddle, v.saddleColor, nullptr, 1.0F);
  }

  QVector3D const blanket_center =
      saddle_center + QVector3D(0.0F, -d.saddleThickness, 0.0F);
  {
    QMatrix4x4 blanket = ctx.model;
    blanket.translate(blanket_center);
    blanket.scale(d.bodyWidth * 1.26F, d.saddleThickness * 0.38F,
                  d.bodyLength * 0.42F);
    out.mesh(getUnitSphere(), blanket, v.blanketColor * 1.02F, nullptr, 1.0F);
  }

  {
    QMatrix4x4 cantle = ctx.model;
    cantle.translate(saddle_center + QVector3D(0.0F, d.saddleThickness * 0.72F,
                                               -d.bodyLength * 0.12F));
    cantle.scale(QVector3D(d.bodyWidth * 0.52F, d.saddleThickness * 0.60F,
                           d.bodyLength * 0.18F));
    out.mesh(getUnitSphere(), cantle, lighten(v.saddleColor, 1.05F), nullptr,
             1.0F);
  }

  {
    QMatrix4x4 pommel = ctx.model;
    pommel.translate(saddle_center + QVector3D(0.0F, d.saddleThickness * 0.58F,
                                               d.bodyLength * 0.16F));
    pommel.scale(QVector3D(d.bodyWidth * 0.40F, d.saddleThickness * 0.48F,
                           d.bodyLength * 0.14F));
    out.mesh(getUnitSphere(), pommel, darken(v.saddleColor, 0.92F), nullptr,
             1.0F);
  }

  for (int i = 0; i < 6; ++i) {
    float const t = static_cast<float>(i) / 5.0F;
    QMatrix4x4 stitch = ctx.model;
    stitch.translate(blanket_center + QVector3D(d.bodyWidth * (t - 0.5F) * 1.1F,
                                                -d.saddleThickness * 0.35F,
                                                d.bodyLength * 0.28F));
    stitch.scale(QVector3D(d.bodyWidth * 0.05F, d.bodyWidth * 0.02F,
                           d.bodyWidth * 0.12F));
    out.mesh(getUnitSphere(), stitch, v.blanketColor * 0.75F, nullptr, 0.9F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const strap_top =
        saddle_center + QVector3D(side * d.bodyWidth * 0.92F,
                                  d.saddleThickness * 0.32F,
                                  d.bodyLength * 0.02F);
    QVector3D const strap_bottom =
        strap_top +
        QVector3D(0.0F, -d.bodyHeight * 0.94F, -d.bodyLength * 0.06F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, strap_top, strap_bottom,
                             d.bodyWidth * 0.065F),
             v.tack_color * 0.94F, nullptr, 1.0F);

    QMatrix4x4 buckle = ctx.model;
    buckle.translate(lerp(strap_top, strap_bottom, 0.87F) +
                     QVector3D(0.0F, 0.0F, -d.bodyLength * 0.02F));
    buckle.scale(QVector3D(d.bodyWidth * 0.16F, d.bodyWidth * 0.12F,
                           d.bodyWidth * 0.05F));
    out.mesh(getUnitSphere(), buckle, QVector3D(0.42F, 0.39F, 0.35F), nullptr,
             1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const breast_anchor =
        chest_center + QVector3D(side * d.bodyWidth * 0.70F,
                                 -d.bodyHeight * 0.10F, d.bodyLength * 0.18F);
    QVector3D const breast_to_saddle =
        saddle_center + QVector3D(side * d.bodyWidth * 0.48F,
                                  -d.saddleThickness * 0.20F,
                                  d.bodyLength * 0.10F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, breast_anchor, breast_to_saddle,
                             d.bodyWidth * 0.055F),
             v.tack_color * 0.92F, nullptr, 1.0F);
  }

  QVector3D stirrup_attach_left =
      saddle_center + QVector3D(-d.bodyWidth * 0.92F,
                                -d.saddleThickness * 0.10F,
                                d.seatForwardOffset * 0.28F);
  QVector3D stirrup_attach_right =
      saddle_center + QVector3D(d.bodyWidth * 0.92F, -d.saddleThickness * 0.10F,
                                d.seatForwardOffset * 0.28F);
  QVector3D stirrup_bottom_left =
      stirrup_attach_left + QVector3D(0.0F, -d.stirrupDrop, 0.0F);
  QVector3D stirrup_bottom_right =
      stirrup_attach_right + QVector3D(0.0F, -d.stirrupDrop, 0.0F);
  mount.stirrup_attach_left = stirrup_attach_left;
  mount.stirrup_attach_right = stirrup_attach_right;
  mount.stirrup_bottom_left = stirrup_bottom_left;
  mount.stirrup_bottom_right = stirrup_bottom_right;

  auto draw_stirrup = [&](const QVector3D &attach, const QVector3D &bottom) {
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, attach, bottom, d.bodyWidth * 0.048F),
             v.tack_color * 0.98F, nullptr, 1.0F);

    QMatrix4x4 leather_loop = ctx.model;
    leather_loop.translate(lerp(attach, bottom, 0.30F) +
                           QVector3D(0.0F, 0.0F, d.bodyWidth * 0.02F));
    leather_loop.scale(QVector3D(d.bodyWidth * 0.18F, d.bodyWidth * 0.05F,
                                 d.bodyWidth * 0.10F));
    out.mesh(getUnitSphere(), leather_loop, v.tack_color * 0.92F, nullptr,
             1.0F);

    QMatrix4x4 stirrup = ctx.model;
    stirrup.translate(bottom + QVector3D(0.0F, -d.bodyWidth * 0.06F, 0.0F));
    stirrup.scale(d.bodyWidth * 0.20F, d.bodyWidth * 0.07F,
                  d.bodyWidth * 0.16F);
    out.mesh(getUnitSphere(), stirrup, QVector3D(0.66F, 0.65F, 0.62F), nullptr,
             1.0F);
  };

  draw_stirrup(stirrup_attach_left, stirrup_bottom_left);
  draw_stirrup(stirrup_attach_right, stirrup_bottom_right);

  QVector3D const cheek_left_top =
      head_center + QVector3D(d.headWidth * 0.60F, -d.headHeight * 0.10F,
                              d.headLength * 0.25F);
  QVector3D const cheek_left_bottom =
      cheek_left_top + QVector3D(0.0F, -d.headHeight, -d.headLength * 0.12F);
  QVector3D const cheek_right_top =
      head_center + QVector3D(-d.headWidth * 0.60F, -d.headHeight * 0.10F,
                              d.headLength * 0.25F);
  QVector3D const cheek_right_bottom =
      cheek_right_top + QVector3D(0.0F, -d.headHeight, -d.headLength * 0.12F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, cheek_left_top, cheek_left_bottom,
                           d.headWidth * 0.08F),
           v.tack_color, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, cheek_right_top, cheek_right_bottom,
                           d.headWidth * 0.08F),
           v.tack_color, nullptr, 1.0F);

  QVector3D const nose_band_front =
      muzzle_center +
      QVector3D(0.0F, d.headHeight * 0.02F, d.muzzleLength * 0.35F);
  QVector3D const nose_band_left =
      nose_band_front + QVector3D(d.headWidth * 0.55F, 0.0F, 0.0F);
  QVector3D const nose_band_right =
      nose_band_front + QVector3D(-d.headWidth * 0.55F, 0.0F, 0.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, nose_band_left, nose_band_right,
                           d.headWidth * 0.08F),
           v.tack_color * 0.92F, nullptr, 1.0F);

  QVector3D const brow_band_front =
      head_center + QVector3D(0.0F, d.headHeight * 0.28F, d.headLength * 0.15F);
  QVector3D const brow_band_left =
      brow_band_front + QVector3D(d.headWidth * 0.58F, 0.0F, 0.0F);
  QVector3D const brow_band_right =
      brow_band_front + QVector3D(-d.headWidth * 0.58F, 0.0F, 0.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, brow_band_left, brow_band_right,
                           d.headWidth * 0.07F),
           v.tack_color, nullptr, 1.0F);

  QVector3D const bit_left =
      muzzle_center + QVector3D(d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);
  QVector3D const bit_right =
      muzzle_center + QVector3D(-d.headWidth * 0.55F, -d.headHeight * 0.08F,
                                d.muzzleLength * 0.10F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, bit_left, bit_right, d.headWidth * 0.05F),
           QVector3D(0.55F, 0.55F, 0.55F), nullptr, 1.0F);

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const rein_start = (i == 0) ? bit_left : bit_right;
    QVector3D const rein_end =
        saddle_center + QVector3D(side * d.bodyWidth * 0.62F,
                                  -d.saddleThickness * 0.32F,
                                  d.seatForwardOffset * 0.10F);
    if (i == 0) {
      mount.rein_attach_left = rein_end;
    } else {
      mount.rein_attach_right = rein_end;
    }

    QVector3D const mid =
        lerp(rein_start, rein_end, 0.46F) +
        QVector3D(0.0F, -d.bodyHeight * (0.08F + rein_slack), 0.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, rein_start, mid, d.bodyWidth * 0.02F),
             v.tack_color * 0.95F, nullptr, 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mid, rein_end, d.bodyWidth * 0.02F),
             v.tack_color * 0.95F, nullptr, 1.0F);
  }

  drawAttachments(ctx, anim, rider_ctx, profile, mount, phase, bob, rein_slack,
                  out);
}

} // namespace Render::GL
