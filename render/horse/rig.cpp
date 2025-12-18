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

auto get_horse_render_stats() -> const HorseRenderStats & {
  return s_horseRenderStats;
}

void reset_horse_render_stats() { s_horseRenderStats.reset(); }

using Render::Geom::clamp01;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
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

inline auto rand_between(uint32_t seed, uint32_t salt, float min_val,
                         float max_val) -> float {
  const float t = hash01(seed ^ salt);
  return min_val + (max_val - min_val) * t;
}

inline auto saturate(float x) -> float {
  return std::min(1.0F, std::max(0.0F, x));
}

inline auto rotate_around_y(const QVector3D &v, float angle) -> QVector3D {
  float const s = std::sin(angle);
  float const c = std::cos(angle);
  return {v.x() * c + v.z() * s, v.y(), -v.x() * s + v.z() * c};
}
inline auto rotate_around_z(const QVector3D &v, float angle) -> QVector3D {
  float const s = std::sin(angle);
  float const c = std::cos(angle);
  return {v.x() * c - v.y() * s, v.x() * s + v.y() * c, v.z()};
}

inline auto darken(const QVector3D &c, float k) -> QVector3D { return c * k; }
inline auto lighten(const QVector3D &c, float k) -> QVector3D {
  return {saturate(c.x() * k), saturate(c.y() * k), saturate(c.z() * k)};
}

constexpr float k_coat_highlight_base = 0.55F;
constexpr float k_coat_vertical_factor = 0.35F;
constexpr float k_coat_longitudinal_factor = 0.20F;
constexpr float k_coat_seed_factor = 0.08F;
constexpr float k_coat_bright_factor = 1.08F;
constexpr float k_coat_shadow_factor = 0.86F;

inline auto coat_gradient(const QVector3D &coat, float vertical_factor,
                          float longitudinal_factor, float seed) -> QVector3D {
  float const highlight = saturate(
      k_coat_highlight_base + vertical_factor * k_coat_vertical_factor -
      longitudinal_factor * k_coat_longitudinal_factor +
      seed * k_coat_seed_factor);
  QVector3D const bright = lighten(coat, k_coat_bright_factor);
  QVector3D const shadow = darken(coat, k_coat_shadow_factor);
  return shadow * (1.0F - highlight) + bright * highlight;
}

inline auto lerp3(const QVector3D &a, const QVector3D &b,
                  float t) -> QVector3D {
  return {a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
          a.z() + (b.z() - a.z()) * t};
}

inline auto scaled_sphere(const QMatrix4x4 &model, const QVector3D &center,
                          const QVector3D &scale) -> QMatrix4x4 {
  QMatrix4x4 m = model;
  m.translate(center);
  m.scale(scale);
  return m;
}

inline void draw_cylinder(ISubmitter &out, const QMatrix4x4 &model,
                          const QVector3D &a, const QVector3D &b, float radius,
                          const QVector3D &color, float alpha = 1.0F,
                          int material_id = 0) {
  out.mesh(get_unit_cylinder(), cylinder_between(model, a, b, radius), color,
           nullptr, alpha, material_id);
}

inline void draw_cone(ISubmitter &out, const QMatrix4x4 &model,
                      const QVector3D &tip, const QVector3D &base, float radius,
                      const QVector3D &color, float alpha = 1.0F,
                      int material_id = 0) {
  out.mesh(get_unit_cone(), cone_from_to(model, tip, base, radius), color,
           nullptr, alpha, material_id);
}

inline void draw_rounded_segment(ISubmitter &out, const QMatrix4x4 &model,
                                 const QVector3D &start, const QVector3D &end,
                                 float start_radius, float end_radius,
                                 const QVector3D &start_color,
                                 const QVector3D &end_color, float alpha = 1.0F,
                                 int material_id = 0) {
  float const mid_radius = 0.5F * (start_radius + end_radius);
  QVector3D const tint = lerp(start_color, end_color, 0.5F);
  out.mesh(get_unit_cylinder(), cylinder_between(model, start, end, mid_radius),
           tint, nullptr, alpha, material_id);
  out.mesh(get_unit_sphere(),
           Render::Geom::sphere_at(model, start, start_radius), start_color,
           nullptr, alpha, material_id);
  out.mesh(get_unit_sphere(), Render::Geom::sphere_at(model, end, end_radius),
           end_color, nullptr, alpha, material_id);
}

inline auto bezier(const QVector3D &p0, const QVector3D &p1,
                   const QVector3D &p2, float t) -> QVector3D {
  float const u = 1.0F - t;
  return p0 * (u * u) + p1 * (2.0F * u * t) + p2 * (t * t);
}

inline auto color_hash(const QVector3D &c) -> uint32_t {
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

namespace HorseDimensionRange {

constexpr float kBodyLengthMin = 0.92F;
constexpr float kBodyLengthMax = 1.08F;
constexpr float kBodyWidthMin = 0.20F;
constexpr float kBodyWidthMax = 0.28F;
constexpr float kBodyHeightMin = 0.42F;
constexpr float kBodyHeightMax = 0.52F;

constexpr float kNeckLengthMin = 0.48F;
constexpr float kNeckLengthMax = 0.58F;
constexpr float kNeckRiseMin = 0.30F;
constexpr float kNeckRiseMax = 0.38F;

constexpr float kHeadLengthMin = 0.34F;
constexpr float kHeadLengthMax = 0.42F;
constexpr float kHeadWidthMin = 0.16F;
constexpr float kHeadWidthMax = 0.20F;
constexpr float kHeadHeightMin = 0.22F;
constexpr float kHeadHeightMax = 0.28F;
constexpr float kMuzzleLengthMin = 0.16F;
constexpr float kMuzzleLengthMax = 0.20F;

constexpr float kLegLengthMin = 1.05F;
constexpr float kLegLengthMax = 1.18F;
constexpr float kHoofHeightMin = 0.095F;
constexpr float kHoofHeightMax = 0.115F;

constexpr float kTailLengthMin = 0.55F;
constexpr float kTailLengthMax = 0.72F;

constexpr float kSaddleThicknessMin = 0.035F;
constexpr float kSaddleThicknessMax = 0.045F;
constexpr float kSeatForwardOffsetMin = 0.010F;
constexpr float kSeatForwardOffsetMax = 0.035F;
constexpr float kStirrupOutScaleMin = 0.75F;
constexpr float kStirrupOutScaleMax = 0.88F;
constexpr float kStirrupDropMin = 0.28F;
constexpr float kStirrupDropMax = 0.32F;

constexpr float kIdleBobAmplitudeMin = 0.004F;
constexpr float kIdleBobAmplitudeMax = 0.007F;
constexpr float kMoveBobAmplitudeMin = 0.024F;
constexpr float kMoveBobAmplitudeMax = 0.032F;

constexpr float kLegSegmentRatioUpper = 0.59F;
constexpr float kLegSegmentRatioMiddle = 0.30F;
constexpr float kLegSegmentRatioLower = 0.12F;
constexpr float kShoulderBarrelOffsetScale = 0.05F;
constexpr float kShoulderBarrelOffsetBase = 0.05F;
constexpr float kSaddleHeightBodyScale = 0.55F;

constexpr uint32_t kSaltBodyLength = 0x12U;
constexpr uint32_t kSaltBodyWidth = 0x34U;
constexpr uint32_t kSaltBodyHeight = 0x56U;
constexpr uint32_t kSaltNeckLength = 0x9AU;
constexpr uint32_t kSaltNeckRise = 0xBCU;
constexpr uint32_t kSaltHeadLength = 0xDEU;
constexpr uint32_t kSaltHeadWidth = 0xF1U;
constexpr uint32_t kSaltHeadHeight = 0x1357U;
constexpr uint32_t kSaltMuzzleLength = 0x2468U;
constexpr uint32_t kSaltLegLength = 0x369CU;
constexpr uint32_t kSaltHoofHeight = 0x48AEU;
constexpr uint32_t kSaltTailLength = 0x5ABCU;
constexpr uint32_t kSaltSaddleThickness = 0x6CDEU;
constexpr uint32_t kSaltSeatForwardOffset = 0x7531U;
constexpr uint32_t kSaltStirrupOut = 0x8642U;
constexpr uint32_t kSaltStirrupDrop = 0x9753U;
constexpr uint32_t kSaltIdleBob = 0xA864U;
constexpr uint32_t kSaltMoveBob = 0xB975U;

} // namespace HorseDimensionRange

auto make_horse_dimensions(uint32_t seed) -> HorseDimensions {
  using namespace HorseDimensionRange;
  HorseDimensions d{};

  d.body_length =
      rand_between(seed, kSaltBodyLength, kBodyLengthMin, kBodyLengthMax);
  d.body_width =
      rand_between(seed, kSaltBodyWidth, kBodyWidthMin, kBodyWidthMax);
  d.body_height =
      rand_between(seed, kSaltBodyHeight, kBodyHeightMin, kBodyHeightMax);

  d.neck_length =
      rand_between(seed, kSaltNeckLength, kNeckLengthMin, kNeckLengthMax);
  d.neck_rise = rand_between(seed, kSaltNeckRise, kNeckRiseMin, kNeckRiseMax);
  d.head_length =
      rand_between(seed, kSaltHeadLength, kHeadLengthMin, kHeadLengthMax);
  d.head_width =
      rand_between(seed, kSaltHeadWidth, kHeadWidthMin, kHeadWidthMax);
  d.head_height =
      rand_between(seed, kSaltHeadHeight, kHeadHeightMin, kHeadHeightMax);
  d.muzzle_length =
      rand_between(seed, kSaltMuzzleLength, kMuzzleLengthMin, kMuzzleLengthMax);

  d.leg_length =
      rand_between(seed, kSaltLegLength, kLegLengthMin, kLegLengthMax);
  d.hoof_height =
      rand_between(seed, kSaltHoofHeight, kHoofHeightMin, kHoofHeightMax);

  d.tail_length =
      rand_between(seed, kSaltTailLength, kTailLengthMin, kTailLengthMax);

  d.saddle_thickness = rand_between(seed, kSaltSaddleThickness,
                                    kSaddleThicknessMin, kSaddleThicknessMax);
  d.seat_forward_offset =
      rand_between(seed, kSaltSeatForwardOffset, kSeatForwardOffsetMin,
                   kSeatForwardOffsetMax);
  d.stirrup_out =
      d.body_width * rand_between(seed, kSaltStirrupOut, kStirrupOutScaleMin,
                                  kStirrupOutScaleMax);
  d.stirrup_drop =
      rand_between(seed, kSaltStirrupDrop, kStirrupDropMin, kStirrupDropMax);

  d.idle_bob_amplitude = rand_between(seed, kSaltIdleBob, kIdleBobAmplitudeMin,
                                      kIdleBobAmplitudeMax);
  d.move_bob_amplitude = rand_between(seed, kSaltMoveBob, kMoveBobAmplitudeMin,
                                      kMoveBobAmplitudeMax);

  float const avg_leg_segment_ratio =
      kLegSegmentRatioUpper + kLegSegmentRatioMiddle + kLegSegmentRatioLower;
  float const leg_down_distance =
      d.leg_length * avg_leg_segment_ratio + d.hoof_height;
  float const shoulder_to_barrel_offset =
      d.body_height * kShoulderBarrelOffsetScale + kShoulderBarrelOffsetBase;
  d.barrel_center_y = leg_down_distance - shoulder_to_barrel_offset;

  d.saddle_height = d.barrel_center_y + d.body_height * kSaddleHeightBodyScale +
                    d.saddle_thickness;

  return d;
}

namespace HorseVariantConstants {

constexpr float kGrayCoatThreshold = 0.18F;
constexpr float kBayCoatThreshold = 0.38F;
constexpr float kChestnutCoatThreshold = 0.65F;
constexpr float kBlackCoatThreshold = 0.85F;

constexpr float kGrayCoatR = 0.70F;
constexpr float kGrayCoatG = 0.68F;
constexpr float kGrayCoatB = 0.63F;
constexpr float kBayCoatR = 0.40F;
constexpr float kBayCoatG = 0.30F;
constexpr float kBayCoatB = 0.22F;
constexpr float kChestnutCoatR = 0.28F;
constexpr float kChestnutCoatG = 0.22F;
constexpr float kChestnutCoatB = 0.19F;
constexpr float kBlackCoatR = 0.18F;
constexpr float kBlackCoatG = 0.15F;
constexpr float kBlackCoatB = 0.13F;
constexpr float kDunCoatR = 0.48F;
constexpr float kDunCoatG = 0.42F;
constexpr float kDunCoatB = 0.39F;

constexpr float kBlazeChanceThreshold = 0.82F;
constexpr float kBlazeColorR = 0.92F;
constexpr float kBlazeColorG = 0.92F;
constexpr float kBlazeColorB = 0.90F;
constexpr float kBlazeBlendFactor = 0.25F;

constexpr float kManeBlendMin = 0.55F;
constexpr float kManeBlendMax = 0.85F;
constexpr float kManeBaseR = 0.10F;
constexpr float kManeBaseG = 0.09F;
constexpr float kManeBaseB = 0.08F;
constexpr float kTailBlendFactor = 0.35F;

constexpr float kMuzzleBlendFactor = 0.65F;
constexpr float kMuzzleBaseR = 0.18F;
constexpr float kMuzzleBaseG = 0.14F;
constexpr float kMuzzleBaseB = 0.12F;
constexpr float kHoofDarkR = 0.16F;
constexpr float kHoofDarkG = 0.14F;
constexpr float kHoofDarkB = 0.12F;
constexpr float kHoofLightR = 0.40F;
constexpr float kHoofLightG = 0.35F;
constexpr float kHoofLightB = 0.32F;
constexpr float kHoofBlendMin = 0.15F;
constexpr float kHoofBlendMax = 0.65F;

constexpr float kLeatherToneMin = 0.78F;
constexpr float kLeatherToneMax = 0.96F;
constexpr float kTackToneMin = 0.58F;
constexpr float kTackToneMax = 0.78F;
constexpr float kSpecialTackThreshold = 0.90F;
constexpr float kSpecialTackR = 0.18F;
constexpr float kSpecialTackG = 0.19F;
constexpr float kSpecialTackB = 0.22F;
constexpr float kSpecialTackBlend = 0.25F;

constexpr float kBlanketTintMin = 0.92F;
constexpr float kBlanketTintMax = 1.05F;

constexpr float kEarInnerBaseR = 0.45F;
constexpr float kEarInnerBaseG = 0.35F;
constexpr float kEarInnerBaseB = 0.32F;
constexpr float kEarInnerBlendFactor = 0.30F;

constexpr uint32_t kSaltCoatHue = 0x23456U;
constexpr uint32_t kSaltBlazeChance = 0x1122U;
constexpr uint32_t kSaltManeBlend = 0x3344U;
constexpr uint32_t kSaltHoofBlend = 0x5566U;
constexpr uint32_t kSaltLeatherTone = 0x7788U;
constexpr uint32_t kSaltTackTone = 0x88AAU;
constexpr uint32_t kSaltBlanketTint = 0x99B0U;

} // namespace HorseVariantConstants

namespace HorseGaitConstants {

constexpr float kCycleTimeMin = 0.60F;
constexpr float kCycleTimeMax = 0.72F;
constexpr float kFrontLegPhaseMin = 0.08F;
constexpr float kFrontLegPhaseMax = 0.16F;
constexpr float kDiagonalLeadMin = 0.44F;
constexpr float kDiagonalLeadMax = 0.54F;
constexpr float kStrideSwingMin = 0.26F;
constexpr float kStrideSwingMax = 0.32F;
constexpr float kStrideLiftMin = 0.10F;
constexpr float kStrideLiftMax = 0.14F;

constexpr uint32_t kSaltCycleTime = 0xAA12U;
constexpr uint32_t kSaltFrontLegPhase = 0xBB34U;
constexpr uint32_t kSaltDiagonalLead = 0xCC56U;
constexpr uint32_t kSaltStrideSwing = 0xDD78U;
constexpr uint32_t kSaltStrideLift = 0xEE9AU;

} // namespace HorseGaitConstants

auto make_horse_variant(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseVariant {
  using namespace HorseVariantConstants;
  HorseVariant v;

  float const coat_hue = hash01(seed ^ kSaltCoatHue);
  if (coat_hue < kGrayCoatThreshold) {
    v.coat_color = QVector3D(kGrayCoatR, kGrayCoatG, kGrayCoatB);
  } else if (coat_hue < kBayCoatThreshold) {
    v.coat_color = QVector3D(kBayCoatR, kBayCoatG, kBayCoatB);
  } else if (coat_hue < kChestnutCoatThreshold) {
    v.coat_color = QVector3D(kChestnutCoatR, kChestnutCoatG, kChestnutCoatB);
  } else if (coat_hue < kBlackCoatThreshold) {
    v.coat_color = QVector3D(kBlackCoatR, kBlackCoatG, kBlackCoatB);
  } else {
    v.coat_color = QVector3D(kDunCoatR, kDunCoatG, kDunCoatB);
  }

  float const blaze_chance = hash01(seed ^ kSaltBlazeChance);
  if (blaze_chance > kBlazeChanceThreshold) {
    v.coat_color =
        lerp(v.coat_color, QVector3D(kBlazeColorR, kBlazeColorG, kBlazeColorB),
             kBlazeBlendFactor);
  }

  v.mane_color =
      lerp(v.coat_color, QVector3D(kManeBaseR, kManeBaseG, kManeBaseB),
           rand_between(seed, kSaltManeBlend, kManeBlendMin, kManeBlendMax));
  v.tail_color = lerp(v.mane_color, v.coat_color, kTailBlendFactor);

  v.muzzle_color =
      lerp(v.coat_color, QVector3D(kMuzzleBaseR, kMuzzleBaseG, kMuzzleBaseB),
           kMuzzleBlendFactor);
  v.hoof_color =
      lerp(QVector3D(kHoofDarkR, kHoofDarkG, kHoofDarkB),
           QVector3D(kHoofLightR, kHoofLightG, kHoofLightB),
           rand_between(seed, kSaltHoofBlend, kHoofBlendMin, kHoofBlendMax));

  float const leather_tone =
      rand_between(seed, kSaltLeatherTone, kLeatherToneMin, kLeatherToneMax);
  float const tack_tone =
      rand_between(seed, kSaltTackTone, kTackToneMin, kTackToneMax);
  QVector3D const leather_tint = leather_base * leather_tone;
  QVector3D tack_tint = leather_base * tack_tone;
  if (blaze_chance > kSpecialTackThreshold) {
    tack_tint =
        lerp(tack_tint, QVector3D(kSpecialTackR, kSpecialTackG, kSpecialTackB),
             kSpecialTackBlend);
  }
  v.saddle_color = leather_tint;
  v.tack_color = tack_tint;

  v.blanket_color = cloth_base * rand_between(seed, kSaltBlanketTint,
                                              kBlanketTintMin, kBlanketTintMax);

  return v;
}

auto make_horse_profile(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseProfile {
  using namespace HorseGaitConstants;
  HorseProfile profile;
  profile.dims = make_horse_dimensions(seed);
  profile.variant = make_horse_variant(seed, leather_base, cloth_base);

  profile.gait.cycle_time =
      rand_between(seed, kSaltCycleTime, kCycleTimeMin, kCycleTimeMax);
  profile.gait.front_leg_phase = rand_between(
      seed, kSaltFrontLegPhase, kFrontLegPhaseMin, kFrontLegPhaseMax);
  float const diagonal_lead =
      rand_between(seed, kSaltDiagonalLead, kDiagonalLeadMin, kDiagonalLeadMax);
  profile.gait.rear_leg_phase =
      std::fmod(profile.gait.front_leg_phase + diagonal_lead, 1.0F);
  profile.gait.stride_swing =
      rand_between(seed, kSaltStrideSwing, kStrideSwingMin, kStrideSwingMax);
  profile.gait.stride_lift =
      rand_between(seed, kSaltStrideLift, kStrideLiftMin, kStrideLiftMax);

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

namespace MountFrameConstants {

constexpr float kSaddleThicknessOffset = 0.35F;
constexpr float kSaddleBodyLengthOffset = 0.05F;
constexpr float kSaddleSeatForwardScale = 0.25F;
constexpr float kSeatPositionHeightScale = 0.32F;

constexpr float kStirrupWidthScale = 0.92F;
constexpr float kStirrupThicknessOffset = 0.10F;
constexpr float kStirrupForwardScale = 0.28F;

constexpr float kNeckTopBodyHeightScale = 0.65F;
constexpr float kNeckTopBodyLengthScale = 0.25F;
constexpr float kHeadCenterHeightScale = 0.10F;
constexpr float kHeadCenterLengthScale = 0.40F;

constexpr float kMuzzleHeightOffset = 0.18F;
constexpr float kMuzzleLengthOffset = 0.58F;
constexpr float kBridleHeightOffset = 0.05F;
constexpr float kBridleLengthOffset = 0.20F;

constexpr float kBitWidthOffset = 0.55F;
constexpr float kBitHeightOffset = 0.08F;
constexpr float kBitLengthOffset = 0.10F;

} // namespace MountFrameConstants

auto compute_mount_frame(const HorseProfile &profile)
    -> MountedAttachmentFrame {
  using namespace MountFrameConstants;
  const HorseDimensions &d = profile.dims;
  MountedAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.ground_offset = QVector3D(0.0F, -d.barrel_center_y, 0.0F);

  frame.saddle_center = QVector3D(
      0.0F, d.saddle_height - d.saddle_thickness * kSaddleThicknessOffset,
      -d.body_length * kSaddleBodyLengthOffset +
          d.seat_forward_offset * kSaddleSeatForwardScale);

  frame.seat_position =
      frame.saddle_center +
      QVector3D(0.0F, d.saddle_thickness * kSeatPositionHeightScale, 0.0F);

  frame.stirrup_attach_left =
      frame.saddle_center +
      QVector3D(-d.body_width * kStirrupWidthScale,
                -d.saddle_thickness * kStirrupThicknessOffset,
                d.seat_forward_offset * kStirrupForwardScale);
  frame.stirrup_attach_right =
      frame.saddle_center +
      QVector3D(d.body_width * kStirrupWidthScale,
                -d.saddle_thickness * kStirrupThicknessOffset,
                d.seat_forward_offset * kStirrupForwardScale);

  frame.stirrup_bottom_left =
      frame.stirrup_attach_left + QVector3D(0.0F, -d.stirrup_drop, 0.0F);
  frame.stirrup_bottom_right =
      frame.stirrup_attach_right + QVector3D(0.0F, -d.stirrup_drop, 0.0F);

  QVector3D const neck_top(
      0.0F,
      d.barrel_center_y + d.body_height * kNeckTopBodyHeightScale + d.neck_rise,
      d.body_length * kNeckTopBodyLengthScale);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * kHeadCenterHeightScale,
                           d.head_length * kHeadCenterLengthScale);

  QVector3D const muzzle_center =
      head_center + QVector3D(0.0F, -d.head_height * kMuzzleHeightOffset,
                              d.head_length * kMuzzleLengthOffset);
  frame.bridle_base =
      muzzle_center + QVector3D(0.0F, -d.head_height * kBridleHeightOffset,
                                d.muzzle_length * kBridleLengthOffset);
  frame.rein_bit_left =
      muzzle_center + QVector3D(d.head_width * kBitWidthOffset,
                                -d.head_height * kBitHeightOffset,
                                d.muzzle_length * kBitLengthOffset);
  frame.rein_bit_right =
      muzzle_center + QVector3D(-d.head_width * kBitWidthOffset,
                                -d.head_height * kBitHeightOffset,
                                d.muzzle_length * kBitLengthOffset);

  return frame;
}

namespace ReinConstants {

constexpr uint32_t kSlackSeedSalt = 0x707U;
constexpr float kBaseSlackScale = 0.08F;
constexpr float kBaseSlackOffset = 0.02F;
constexpr float kTargetTensionBonus = 0.25F;
constexpr float kAttackTensionBonus = 0.35F;
constexpr float kMinSlack = 0.01F;

constexpr float kHandleRightOffset = 0.08F;
constexpr float kHandleForwardBase = 0.18F;
constexpr float kHandleForwardTensionScale = 0.18F;
constexpr float kHandleUpBase = -0.10F;
constexpr float kHandleUpSlackScale = -0.30F;
constexpr float kHandleUpTensionScale = 0.04F;
constexpr float kDirLengthThreshold = 1e-4F;
constexpr float kReinBaseLength = 0.85F;
constexpr float kSlackLengthScale = 0.12F;

} // namespace ReinConstants

auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx)
    -> ReinState {
  using namespace ReinConstants;
  float const base_slack =
      hash01(horse_seed ^ kSlackSeedSalt) * kBaseSlackScale + kBaseSlackOffset;
  float rein_tension = rider_ctx.locomotion_normalized_speed();
  if (rider_ctx.gait.has_target) {
    rein_tension += kTargetTensionBonus;
  }
  if (rider_ctx.is_attacking()) {
    rein_tension += kAttackTensionBonus;
  }
  rein_tension = std::clamp(rein_tension, 0.0F, 1.0F);
  float const rein_slack =
      std::max(kMinSlack, base_slack * (1.0F - rein_tension));
  return ReinState{rein_slack, rein_tension};
}

auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D {
  using namespace ReinConstants;
  float const clamped_slack = std::clamp(slack, 0.0F, 1.0F);
  float const clamped_tension = std::clamp(tension, 0.0F, 1.0F);

  QVector3D const &bit = is_left ? mount.rein_bit_left : mount.rein_bit_right;

  QVector3D desired = mount.seat_position;
  desired +=
      (is_left ? -mount.seat_right : mount.seat_right) * kHandleRightOffset;
  desired +=
      -mount.seat_forward *
      (kHandleForwardBase + clamped_tension * kHandleForwardTensionScale);
  desired +=
      mount.seat_up * (kHandleUpBase + clamped_slack * kHandleUpSlackScale +
                       clamped_tension * kHandleUpTensionScale);

  QVector3D dir = desired - bit;
  if (dir.lengthSquared() < kDirLengthThreshold) {
    dir = -mount.seat_forward;
  }
  dir.normalize();

  float const rein_length = kReinBaseLength + clamped_slack * kSlackLengthScale;
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

  constexpr float kIdleSpeedMax = 0.5F;
  constexpr float kWalkSpeedMax = 3.0F;
  constexpr float kTrotSpeedMax = 5.5F;
  constexpr float kCanterSpeedMax = 8.0F;

  if (sample.is_moving) {
    float const speed = rider_ctx.locomotion_speed();
    if (speed < kIdleSpeedMax) {
      controller.idle(1.0F);
    } else if (speed < kWalkSpeedMax) {
      controller.set_gait(GaitType::WALK);
    } else if (speed < kTrotSpeedMax) {
      controller.set_gait(GaitType::TROT);
    } else if (speed < kCanterSpeedMax) {
      controller.set_gait(GaitType::CANTER);
    } else {
      controller.set_gait(GaitType::GALLOP);
    }
  } else {
    controller.idle(1.0F);
  }

  controller.update_gait_parameters();
  sample.phase = controller.get_current_phase();
  sample.bob = controller.get_current_bob();
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

void HorseRendererBase::render_full(
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

  float const sway_intensity =
      is_moving ? (1.0F - rider_intensity * 0.5F) : 0.3F;
  float const body_sway =
      is_moving ? std::sin(phase * 2.0F * k_pi) * 0.012F * sway_intensity
                : std::sin(anim.time * 0.4F) * 0.005F;

  float const pitch_intensity = rider_intensity * 0.7F + 0.1F;
  float const body_pitch = is_moving ? std::sin((phase + 0.25F) * 2.0F * k_pi) *
                                           0.008F * pitch_intensity
                                     : std::sin(anim.time * 0.25F) * 0.003F;

  float const nod_base = is_moving ? std::sin((phase + 0.25F) * 2.0F * k_pi) *
                                         (0.025F + rider_intensity * 0.02F)
                                   : std::sin(anim.time * 1.5F) * 0.008F;
  float const nod_secondary = std::sin(anim.time * 0.8F) * 0.004F;
  float const head_nod = nod_base + nod_secondary;

  float const head_lateral = body_sway * 0.6F;

  float const spine_flex =
      is_moving ? std::sin(phase * 2.0F * k_pi) * 0.006F * rider_intensity
                : 0.0F;

  uint32_t const vhash = color_hash(v.coat_color);
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

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob + body_pitch,
                                spine_flex);

  float const ground_offset = -d.barrel_center_y - bob;

  QVector3D const chest_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.34F);
  QVector3D const rump_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.36F);
  QVector3D const belly_center =
      barrel_center +
      QVector3D(0.0F, -d.body_height * 0.35F, -d.body_length * 0.05F);

  {
    QMatrix4x4 chest = horse_ctx.model;
    chest.translate(chest_center);
    chest.scale(d.body_width * 1.12F, d.body_height * 0.95F,
                d.body_length * 0.36F);
    QVector3D const chest_color =
        coat_gradient(v.coat_color, 0.75F, 0.20F, coat_seed_a);
    out.mesh(get_unit_sphere(), chest, chest_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 withers = horse_ctx.model;
    withers.translate(chest_center + QVector3D(0.0F, d.body_height * 0.55F,
                                               -d.body_length * 0.03F));
    withers.scale(d.body_width * 0.75F, d.body_height * 0.35F,
                  d.body_length * 0.18F);
    QVector3D const wither_color =
        coat_gradient(v.coat_color, 0.88F, 0.35F, coat_seed_b);
    out.mesh(get_unit_sphere(), withers, wither_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 belly = horse_ctx.model;
    belly.translate(belly_center);
    belly.scale(d.body_width * 0.98F, d.body_height * 0.64F,
                d.body_length * 0.40F);
    QVector3D const belly_color =
        coat_gradient(v.coat_color, 0.25F, -0.10F, coat_seed_c);
    out.mesh(get_unit_sphere(), belly, belly_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 rump = horse_ctx.model;
    rump.translate(rump_center);
    rump.scale(d.body_width * 1.22F, d.body_height * 1.05F,
               d.body_length * 0.38F);
    QVector3D const rump_color =
        coat_gradient(v.coat_color, 0.62F, -0.28F, coat_seed_a * 0.7F);
    out.mesh(get_unit_sphere(), rump, rump_color, nullptr, 1.0F, 6);
  }

  QVector3D withers_peak = chest_center + QVector3D(0.0F, d.body_height * 0.65F,
                                                    -d.body_length * 0.04F);
  QVector3D croup_peak = rump_center + QVector3D(0.0F, d.body_height * 0.50F,
                                                 -d.body_length * 0.16F);

  {
    QMatrix4x4 spine = horse_ctx.model;
    spine.translate(lerp(withers_peak, croup_peak, 0.42F));
    spine.scale(QVector3D(d.body_width * 0.55F, d.body_height * 0.16F,
                          d.body_length * 0.58F));
    QVector3D const spine_color =
        coat_gradient(v.coat_color, 0.74F, -0.06F, coat_seed_d * 0.92F);
    out.mesh(get_unit_sphere(), spine, spine_color, nullptr, 1.0F, 6);
  }

  {
    QMatrix4x4 sternum = horse_ctx.model;
    sternum.translate(barrel_center + QVector3D(0.0F, -d.body_height * 0.42F,
                                                d.body_length * 0.30F));
    sternum.scale(QVector3D(d.body_width * 0.55F, d.body_height * 0.18F,
                            d.body_length * 0.14F));
    out.mesh(get_unit_sphere(), sternum,
             coat_gradient(v.coat_color, 0.18F, 0.18F, coat_seed_a * 0.4F),
             nullptr, 1.0F, 6);
  }

  QVector3D const neck_base =
      chest_center + QVector3D(head_lateral * 0.3F, d.body_height * 0.42F,
                               d.body_length * 0.08F);
  QVector3D const neck_top =
      neck_base + QVector3D(head_lateral * 0.8F, d.neck_rise + head_nod * 0.4F,
                            d.neck_length);
  float const neck_radius = d.body_width * 0.48F;

  QVector3D const neck_mid =
      lerp(neck_base, neck_top, 0.55F) + QVector3D(head_lateral * 0.5F,
                                                   d.body_height * 0.03F,
                                                   d.body_length * 0.02F);
  QVector3D const neck_color_base =
      coat_gradient(v.coat_color, 0.78F, 0.12F, coat_seed_c * 0.6F);
  out.mesh(get_unit_cylinder(),
           cylinder_between(horse_ctx.model, neck_base, neck_mid,
                            neck_radius * 1.00F),
           neck_color_base, nullptr, 1.0F);
  out.mesh(get_unit_cylinder(),
           cylinder_between(horse_ctx.model, neck_mid, neck_top,
                            neck_radius * 0.86F),
           lighten(neck_color_base, 1.03F), nullptr, 1.0F);

  {
    QVector3D const jugular_start =
        lerp(neck_base, neck_top, 0.42F) + QVector3D(d.body_width * 0.18F,
                                                     -d.body_height * 0.06F,
                                                     d.body_length * 0.04F);
    QVector3D const jugular_end =
        jugular_start +
        QVector3D(0.0F, -d.body_height * 0.24F, d.body_length * 0.06F);
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
                            QVector3D(0.0F, d.body_height * 0.12F, 0.0F);
    float const length = lerp(0.14F, 0.08F, t) * d.body_height * 1.4F;
    QVector3D const tip =
        spine + QVector3D(0.0F, length * 1.2F, 0.02F * length);
    draw_cone(out, horse_ctx.model, tip, spine,
              d.body_width * lerp(0.25F, 0.12F, t), mane_color, 1.0F, 7);
  }

  QVector3D const head_center =
      neck_top + QVector3D(head_lateral,
                           d.head_height * (0.10F - head_nod * 0.20F),
                           d.head_length * 0.40F + head_nod * 0.03F);

  {
    QMatrix4x4 skull = horse_ctx.model;
    skull.translate(head_center + QVector3D(0.0F, d.head_height * 0.10F,
                                            -d.head_length * 0.10F));
    skull.scale(d.head_width * 0.95F, d.head_height * 0.90F,
                d.head_length * 0.80F);
    QVector3D const skull_color =
        coat_gradient(v.coat_color, 0.82F, 0.30F, coat_seed_d * 0.8F);
    out.mesh(get_unit_sphere(), skull, skull_color, nullptr, 1.0F);
  }

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 cheek = horse_ctx.model;
    cheek.translate(head_center + QVector3D(side * d.head_width * 0.55F,
                                            -d.head_height * 0.15F, 0.0F));
    cheek.scale(d.head_width * 0.45F, d.head_height * 0.50F,
                d.head_length * 0.60F);
    QVector3D const cheek_color =
        coat_gradient(v.coat_color, 0.70F, 0.18F, coat_seed_a * 0.9F);
    out.mesh(get_unit_sphere(), cheek, cheek_color, nullptr, 1.0F, 6);
  }

  QVector3D const muzzle_center =
      head_center +
      QVector3D(0.0F, -d.head_height * 0.18F, d.head_length * 0.58F);
  {
    QMatrix4x4 muzzle = horse_ctx.model;
    muzzle.translate(muzzle_center +
                     QVector3D(0.0F, -d.head_height * 0.05F, 0.0F));
    muzzle.scale(d.head_width * 0.68F, d.head_height * 0.60F,
                 d.muzzle_length * 1.05F);
    out.mesh(get_unit_sphere(), muzzle, v.muzzle_color, nullptr, 1.0F);
  }

  {
    QVector3D const nostril_base =
        muzzle_center +
        QVector3D(0.0F, -d.head_height * 0.02F, d.muzzle_length * 0.60F);
    QVector3D const left_base =
        nostril_base + QVector3D(d.head_width * 0.26F, 0.0F, 0.0F);
    QVector3D const right_base =
        nostril_base + QVector3D(-d.head_width * 0.26F, 0.0F, 0.0F);
    QVector3D const inward =
        QVector3D(0.0F, -d.head_height * 0.02F, d.muzzle_length * -0.30F);
    out.mesh(get_unit_cone(),
             cone_from_to(horse_ctx.model, left_base + inward, left_base,
                          d.head_width * 0.11F),
             darken(v.muzzle_color, 0.6F), nullptr, 1.0F);
    out.mesh(get_unit_cone(),
             cone_from_to(horse_ctx.model, right_base + inward, right_base,
                          d.head_width * 0.11F),
             darken(v.muzzle_color, 0.6F), nullptr, 1.0F);
  }

  float const ear_flick_l = std::sin(anim.time * 1.7F + 1.3F) * 0.25F;
  float const ear_flick_r = std::sin(anim.time * 1.9F + 2.1F) * -0.22F;
  float const ear_forward_l = std::sin(anim.time * 0.8F) * 0.15F;
  float const ear_forward_r = std::sin(anim.time * 0.9F + 0.5F) * 0.12F;

  QVector3D const ear_base_left =
      head_center + QVector3D(d.head_width * 0.42F, d.head_height * 0.48F,
                              -d.head_length * 0.15F);
  QVector3D const ear_tip_left =
      ear_base_left +
      rotate_around_y(QVector3D(d.head_width * 0.12F, d.head_height * 0.55F,
                                -d.head_length * 0.08F + ear_forward_l),
                      ear_flick_l);
  QVector3D const ear_base_right =
      head_center + QVector3D(-d.head_width * 0.42F, d.head_height * 0.48F,
                              -d.head_length * 0.15F);
  QVector3D const ear_tip_right =
      ear_base_right +
      rotate_around_y(QVector3D(-d.head_width * 0.12F, d.head_height * 0.55F,
                                -d.head_length * 0.08F + ear_forward_r),
                      ear_flick_r);

  float const ear_base_radius = d.head_width * 0.14F;
  float const ear_tip_radius = d.head_width * 0.06F;
  out.mesh(get_unit_cone(),
           cone_from_to(horse_ctx.model, ear_tip_left, ear_base_left,
                        ear_base_radius),
           v.mane_color, nullptr, 1.0F);
  out.mesh(get_unit_cone(),
           cone_from_to(horse_ctx.model, ear_tip_right, ear_base_right,
                        ear_base_radius),
           v.mane_color, nullptr, 1.0F);

  QVector3D const ear_inner_base(HorseVariantConstants::kEarInnerBaseR,
                                 HorseVariantConstants::kEarInnerBaseG,
                                 HorseVariantConstants::kEarInnerBaseB);
  QVector3D const ear_inner_color =
      lerp(v.mane_color, ear_inner_base,
           HorseVariantConstants::kEarInnerBlendFactor);
  QVector3D const ear_mid_left = lerp(ear_base_left, ear_tip_left, 0.4F);
  QVector3D const ear_mid_right = lerp(ear_base_right, ear_tip_right, 0.4F);
  out.mesh(
      get_unit_cone(),
      cone_from_to(horse_ctx.model, ear_tip_left, ear_mid_left, ear_tip_radius),
      ear_inner_color, nullptr, 1.0F);
  out.mesh(get_unit_cone(),
           cone_from_to(horse_ctx.model, ear_tip_right, ear_mid_right,
                        ear_tip_radius),
           ear_inner_color, nullptr, 1.0F);

  QVector3D const eye_left =
      head_center + QVector3D(d.head_width * 0.48F, d.head_height * 0.10F,
                              d.head_length * 0.05F);
  QVector3D const eye_right =
      head_center + QVector3D(-d.head_width * 0.48F, d.head_height * 0.10F,
                              d.head_length * 0.05F);
  QVector3D eye_base_color(0.10F, 0.10F, 0.10F);

  auto draw_eye = [&](const QVector3D &pos) {
    {
      QMatrix4x4 eye = horse_ctx.model;
      eye.translate(pos);
      eye.scale(d.head_width * 0.14F);
      out.mesh(get_unit_sphere(), eye, eye_base_color, nullptr, 1.0F, 6);
    }
    {

      QMatrix4x4 pupil = horse_ctx.model;
      pupil.translate(pos + QVector3D(0.0F, 0.0F, d.head_width * 0.04F));
      pupil.scale(d.head_width * 0.05F);
      out.mesh(get_unit_sphere(), pupil, QVector3D(0.03F, 0.03F, 0.03F),
               nullptr, 1.0F, 6);
    }
    {

      QMatrix4x4 spec = horse_ctx.model;
      spec.translate(pos + QVector3D(d.head_width * 0.03F, d.head_width * 0.03F,
                                     d.head_width * 0.03F));
      spec.scale(d.head_width * 0.02F);
      out.mesh(get_unit_sphere(), spec, QVector3D(0.95F, 0.95F, 0.95F), nullptr,
               1.0F, 6);
    }
  };
  draw_eye(eye_left);
  draw_eye(eye_right);

  if (has_blaze) {
    QMatrix4x4 blaze = horse_ctx.model;
    blaze.translate(head_center + QVector3D(0.0F, d.head_height * 0.15F,
                                            d.head_length * 0.10F));
    blaze.scale(d.head_width * 0.22F, d.head_height * 0.32F,
                d.head_length * 0.10F);
    out.mesh(get_unit_sphere(), blaze, QVector3D(0.92F, 0.92F, 0.90F), nullptr,
             1.0F, 6);
  }

  QVector3D bridle_base =
      muzzle_center +
      QVector3D(0.0F, -d.head_height * 0.05F, d.muzzle_length * 0.20F);
  mount.bridle_base = bridle_base;
  QVector3D const cheek_anchor_left =
      head_center + QVector3D(d.head_width * 0.55F, d.head_height * 0.05F,
                              -d.head_length * 0.05F);
  QVector3D const cheek_anchor_right =
      head_center + QVector3D(-d.head_width * 0.55F, d.head_height * 0.05F,
                              -d.head_length * 0.05F);
  QVector3D const brow = head_center + QVector3D(0.0F, d.head_height * 0.38F,
                                                 -d.head_length * 0.28F);
  QVector3D const tack_color = lighten(v.tack_color, 0.9F);
  draw_cylinder(out, horse_ctx.model, bridle_base, cheek_anchor_left,
                d.head_width * 0.07F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, bridle_base, cheek_anchor_right,
                d.head_width * 0.07F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, cheek_anchor_left, brow,
                d.head_width * 0.05F, tack_color, 1.0F, 10);
  draw_cylinder(out, horse_ctx.model, cheek_anchor_right, brow,
                d.head_width * 0.05F, tack_color, 1.0F, 10);

  QVector3D const mane_root =
      neck_top + QVector3D(0.0F, d.head_height * 0.25F, -d.head_length * 0.15F);
  constexpr int k_mane_segments = 16;
  constexpr float k_mane_segment_divisor =
      static_cast<float>(k_mane_segments - 1);
  for (int i = 0; i < k_mane_segments; ++i) {
    float const t = static_cast<float>(i) / k_mane_segment_divisor;
    QVector3D seg_start = lerp(mane_root, neck_base, t);

    seg_start.setY(seg_start.y() + (0.10F - t * 0.06F));
    float const sway =
        (is_moving ? std::sin((phase + t * 0.18F) * 2.0F * k_pi) *
                         (0.035F + rider_intensity * 0.030F)
                   : std::sin((anim.time * 0.7F + t * 2.5F)) * 0.025F);

    QVector3D const seg_end =
        seg_start + QVector3D(sway, 0.10F - t * 0.06F, -0.07F - t * 0.04F);

    float const mane_thickness = d.head_width * (0.14F * (1.0F - t * 0.35F));
    out.mesh(
        get_unit_cylinder(),
        cylinder_between(horse_ctx.model, seg_start, seg_end, mane_thickness),
        v.mane_color * (0.96F + t * 0.06F), nullptr, 1.0F, 7);
  }

  {
    QVector3D const forelock_base =
        head_center +
        QVector3D(0.0F, d.head_height * 0.35F, -d.head_length * 0.12F);
    for (int i = 0; i < 5; ++i) {
      float const offset = (i - 2) * d.head_width * 0.08F;
      float const wave = std::sin(anim.time * 0.9F + i * 0.5F) * 0.01F;
      QVector3D const strand_base =
          forelock_base + QVector3D(offset, 0.0F, 0.0F);
      QVector3D const strand_tip =
          strand_base + QVector3D(offset * 0.3F + wave, -d.head_height * 0.35F,
                                  d.head_length * 0.15F);
      draw_cone(out, horse_ctx.model, strand_tip, strand_base,
                d.head_width * 0.12F, v.mane_color * (0.92F + 0.04F * (i % 3)),
                0.94F, 7);
    }
  }

  QVector3D const tail_base =
      rump_center +
      QVector3D(0.0F, d.body_height * 0.36F, -d.body_length * 0.34F);
  QVector3D const tail_ctrl =
      tail_base +
      QVector3D(0.0F, -d.tail_length * 0.25F, -d.tail_length * 0.22F);
  QVector3D const tail_end = tail_base + QVector3D(0.0F, -d.tail_length * 1.1F,
                                                   -d.tail_length * 0.55F);
  QVector3D const tail_color = lerp3(v.tail_color, v.mane_color, 0.35F);
  QVector3D prev_tail = tail_base;

  constexpr int k_tail_segments = 14;
  for (int i = 1; i <= k_tail_segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_tail_segments);
    QVector3D p = bezier(tail_base, tail_ctrl, tail_end, t);

    float const primary_swing =
        (is_moving ? std::sin((phase + t * 0.15F) * 2.0F * k_pi)
                   : std::sin((phase * 0.5F + t * 0.4F) * 2.0F * k_pi)) *
        (0.035F + rider_intensity * 0.030F + 0.025F * (1.0F - t));
    float const secondary_swing =
        std::sin(anim.time * 1.2F + t * 3.5F) * 0.015F * (1.0F - t * 0.5F);
    p.setX(p.x() + primary_swing + secondary_swing);

    float const radius = d.body_width * (0.22F - 0.014F * i);
    draw_cylinder(out, horse_ctx.model, prev_tail, p, radius, tail_color, 1.0F,
                  7);
    prev_tail = p;
  }

  {
    QMatrix4x4 tail_knot = horse_ctx.model;
    tail_knot.translate(tail_base + QVector3D(0.0F, -d.body_height * 0.04F,
                                              -d.body_length * 0.01F));
    tail_knot.scale(QVector3D(d.body_width * 0.28F, d.body_width * 0.22F,
                              d.body_width * 0.24F));
    out.mesh(get_unit_sphere(), tail_knot, lighten(tail_color, 0.92F), nullptr,
             1.0F, 7);
  }

  for (int i = 0; i < 5; ++i) {
    float const spread = (i - 2) * d.body_width * 0.12F;
    float const swing_offset = std::sin(anim.time * 0.8F + i * 0.7F) * 0.02F;
    QVector3D const fan_base =
        tail_end + QVector3D(spread * 0.1F + swing_offset,
                             -d.body_width * 0.04F, -d.tail_length * 0.06F);
    QVector3D const fan_tip =
        fan_base + QVector3D(spread + swing_offset * 2.0F,
                             -d.tail_length * 0.38F, -d.tail_length * 0.18F);
    draw_cone(out, horse_ctx.model, fan_tip, fan_base, d.body_width * 0.20F,
              tail_color * (0.94F + 0.03F * (i % 3)), 0.85F, 7);
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
    out.mesh(get_unit_cylinder(), hoof_block, wall_tint, nullptr, 1.0F, 8);

    QMatrix4x4 sole = horse_ctx.model;
    sole.translate(hoof_center + QVector3D(0.0F, -hoof_height * 0.45F, 0.0F));
    sole.scale(
        QVector3D(half_width * 0.92F, hoof_height * 0.08F, half_depth * 0.95F));
    out.mesh(get_unit_cylinder(), sole, darken(hoof_color, 0.72F), nullptr,
             1.0F, 8);

    QMatrix4x4 toe = horse_ctx.model;
    toe.translate(hoof_center + QVector3D(0.0F, -hoof_height * 0.10F,
                                          is_rear ? -half_depth * 0.35F
                                                  : half_depth * 0.30F));
    toe.scale(
        QVector3D(half_width * 0.85F, hoof_height * 0.20F, half_depth * 0.70F));
    out.mesh(get_unit_sphere(), toe, lighten(hoof_color, 1.10F), nullptr, 1.0F,
             8);

    QMatrix4x4 coronet = horse_ctx.model;
    coronet.translate(hoof_top + QVector3D(0.0F, -hoof_height * 0.10F, 0.0F));
    coronet.scale(
        QVector3D(half_width * 0.95F, half_width * 0.60F, half_depth * 1.05F));
    out.mesh(get_unit_sphere(), coronet, lighten(hoof_color, 1.16F), nullptr,
             1.0F, 8);
  };

  constexpr float k_swing_phase_end = 0.5F;
  constexpr float k_impact_settle_duration = 0.15F;
  constexpr float k_impact_settle_intensity = 0.08F;

  auto draw_leg = [&](const QVector3D &anchor, float lateralSign,
                      float forwardBias, float phase_offset, float sockChance) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    float const weight_shift_time = anim.time * 0.15F;
    bool const is_rear = (forwardBias < 0.0F);
    float weight_shift = 0.0F;
    if (!is_moving && is_rear) {

      float const shift_cycle =
          std::sin(weight_shift_time + lateralSign * k_pi * 0.5F);
      weight_shift = shift_cycle * 0.03F;
    }

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;

      bool const is_galloping = (g.stride_swing > 1.0F);

      float const swing_progress =
          leg_phase < k_swing_phase_end ? leg_phase / k_swing_phase_end : 0.0F;
      float const stance_progress =
          leg_phase >= k_swing_phase_end
              ? (leg_phase - k_swing_phase_end) / (1.0F - k_swing_phase_end)
              : 0.0F;

      float const swing_ease =
          swing_progress * swing_progress * (3.0F - 2.0F * swing_progress);

      float const stance_motion = stance_progress;

      float stride_mult = 0.85F;
      float lift_mult = 1.0F;

      if (is_galloping) {

        float const suspension_phase =
            (leg_phase > 0.35F && leg_phase < 0.45F) ? 1.0F : 0.0F;

        stride_mult = 0.95F + swing_ease * 0.15F;

        lift_mult = 1.0F + suspension_phase * 0.25F;

        float const reach_factor = is_rear ? 0.85F : 1.15F;
        stride_mult *= reach_factor;
      }

      float const stride_forward = swing_ease * g.stride_swing * stride_mult;
      float const stride_backward = stance_motion * g.stride_swing * 0.65F;
      stride = (leg_phase < k_swing_phase_end
                    ? stride_forward
                    : (g.stride_swing * stride_mult - stride_backward)) +
               forwardBias - g.stride_swing * 0.25F;

      float const lift_curve = std::sin(swing_progress * k_pi);
      float const impact_settle =
          stance_progress < k_impact_settle_duration
              ? std::sin(stance_progress / k_impact_settle_duration * k_pi) *
                    k_impact_settle_intensity
              : 0.0F;
      lift = leg_phase < k_swing_phase_end
                 ? lift_curve * g.stride_lift * lift_mult
                 : -impact_settle * g.stride_lift;

    } else {

      float const idle = std::sin(leg_phase * 2.0F * k_pi);
      float const secondary =
          std::sin(anim.time * 0.7F + lateralSign * 1.2F) * 0.015F;
      stride = idle * g.stride_swing * 0.08F + forwardBias + secondary +
               weight_shift;
      lift = idle * d.idle_bob_amplitude * 2.5F;
    }

    if (!is_rear) {
      stride =
          std::clamp(stride, -d.body_length * 0.02F, d.body_length * 0.18F);
    }

    bool const tighten_legs = is_moving;
    float const shoulder_out = d.body_width * (tighten_legs ? 0.42F : 0.56F) *
                               (is_rear ? 0.96F : 1.0F);
    float const shoulder_height = (is_rear ? 0.02F : 0.05F);
    float const stance_pull =
        is_rear ? -d.body_length * 0.04F : d.body_length * 0.05F;
    float const stance_stagger =
        lateralSign *
        (is_rear ? -d.body_length * 0.020F : d.body_length * 0.030F);
    QVector3D shoulder =
        anchor + QVector3D(lateralSign * shoulder_out,
                           shoulder_height + lift * 0.04F,
                           stride + stance_pull + stance_stagger);

    float const gallop_angle = leg_phase * 2.0F * k_pi;
    bool const is_galloping = (g.stride_swing > 1.0F);

    float hip_swing_mult = is_galloping ? 1.4F : 1.0F;
    float const hip_swing =
        is_moving ? std::sin(gallop_angle) * hip_swing_mult : 0.0F;

    float const lift_factor =
        is_moving
            ? std::max(0.0F,
                       std::sin(gallop_angle + (is_rear ? 0.35F : -0.25F)))
            : 0.0F;

    float const hip_flex =
        is_galloping ? (is_rear ? -0.14F : 0.12F) : (is_rear ? -0.10F : 0.08F);
    shoulder.setZ(shoulder.z() + hip_swing * hip_flex);
    if (tighten_legs) {
      shoulder.setX(shoulder.x() - lateralSign * lift_factor * 0.04F);
    }

    float const upper_length = d.leg_length * (is_rear ? 0.48F : 0.46F);
    float const lower_length = d.leg_length * (is_rear ? 0.43F : 0.49F);
    float const pastern_length = d.leg_length * (is_rear ? 0.12F : 0.14F);

    float const stance_phase = smoothstep(0.0F, 0.3F, leg_phase);
    float const swing_phase = smoothstep(0.3F, 0.7F, leg_phase);
    float const extend_phase = smoothstep(0.7F, 1.0F, leg_phase);

    float knee_flex_base =
        (swing_phase * (1.0F - extend_phase) * (is_rear ? 0.85F : 0.75F));
    float knee_flex_gallop =
        is_galloping ? knee_flex_base * 1.25F : knee_flex_base;
    float const knee_flex = is_moving ? knee_flex_gallop : 0.35F;

    float cannon_flex_base = smoothstep(0.35F, 0.65F, leg_phase) *
                             (1.0F - extend_phase) * (is_rear ? 0.70F : 0.60F);
    float cannon_flex_gallop =
        is_galloping ? cannon_flex_base * 1.15F : cannon_flex_base;
    float const cannon_flex = is_moving ? cannon_flex_gallop : 0.35F;

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
    float const knee_out = d.body_width * (is_rear ? 0.08F : 0.06F);
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

    float const shoulder_r = d.body_width * (is_rear ? 0.38F : 0.35F);
    float const upper_r = shoulder_r * (is_rear ? 0.92F : 0.88F);
    float const knee_r = upper_r * 0.85F;
    float const cannon_r = knee_r * 0.72F;
    float const pastern_r = cannon_r * 0.78F;

    QVector3D const thigh_color = coat_gradient(
        v.coat_color, is_rear ? 0.48F : 0.58F, is_rear ? -0.22F : 0.18F,
        coat_seed_a + lateralSign * 0.07F);

    draw_cylinder(out, horse_ctx.model, shoulder, knee,
                  (shoulder_r + upper_r) * 0.5F, thigh_color, 1.0F, 6);

    {
      QMatrix4x4 knee_joint = horse_ctx.model;
      knee_joint.translate(knee);
      float const joint_scale = knee_r * 1.15F;
      knee_joint.scale(joint_scale, joint_scale * 0.9F, joint_scale);
      QVector3D const joint_color = darken(thigh_color, 0.95F);
      out.mesh(get_unit_sphere(), knee_joint, joint_color, nullptr, 1.0F, 6);
    }

    QVector3D const shin_color = darken(thigh_color, is_rear ? 0.90F : 0.92F);

    draw_cylinder(out, horse_ctx.model, knee, cannon,
                  (knee_r + cannon_r) * 0.45F, shin_color, 1.0F, 6);

    {
      QVector3D const tendon_offset =
          QVector3D(0.0F, 0.0F, is_rear ? cannon_r * 0.4F : -cannon_r * 0.4F);
      QVector3D const tendon_start = knee + tendon_offset;
      QVector3D const tendon_end = cannon + tendon_offset;
      draw_cylinder(out, horse_ctx.model, tendon_start, tendon_end,
                    cannon_r * 0.35F, darken(shin_color, 0.88F), 1.0F, 6);
    }

    QVector3D const hoof_joint_color =
        darken(shin_color, is_rear ? 0.92F : 0.94F);

    float const sock =
        sockChance > 0.78F ? 1.0F : (sockChance > 0.58F ? 0.55F : 0.0F);
    QVector3D const distal_color =
        (sock > 0.0F) ? lighten(v.coat_color, 1.18F) : v.coat_color * 0.92F;
    float const t_sock = smoothstep(0.0F, 1.0F, sock);
    QVector3D const pastern_color =
        lerp(hoof_joint_color, distal_color, t_sock * 0.8F);

    {
      QMatrix4x4 fetlock_joint = horse_ctx.model;
      fetlock_joint.translate(cannon);
      float const fetlock_scale = cannon_r * 1.1F;
      fetlock_joint.scale(fetlock_scale, fetlock_scale * 0.85F, fetlock_scale);
      out.mesh(get_unit_sphere(), fetlock_joint,
               lerp(hoof_joint_color, pastern_color, 0.3F), nullptr, 1.0F, 6);
    }

    draw_cylinder(out, horse_ctx.model, cannon, fetlock,
                  (cannon_r * 0.85F + pastern_r) * 0.5F,
                  lerp(hoof_joint_color, pastern_color, 0.5F), 1.0F, 6);

    QVector3D const fetlock_color = lerp(pastern_color, distal_color, 0.25F);

    QVector3D const hoof_color = v.hoof_color;
    float const hoof_width = pastern_r * (is_rear ? 1.70F : 1.60F);
    float const hoof_depth = hoof_width * (is_rear ? 0.95F : 1.10F);
    render_hoof(hoof_top, d.hoof_height, hoof_width, hoof_depth, hoof_color,
                is_rear);

    if (sock > 0.0F) {
      QVector3D const feather_tip = lerp(fetlock, hoof_top, 0.35F) +
                                    QVector3D(0.0F, -pastern_r * 0.65F, 0.0F);
      draw_cone(out, horse_ctx.model, feather_tip, fetlock, pastern_r * 0.90F,
                lerp(distal_color, v.coat_color, 0.25F), 0.85F, 6);
    }
  };

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.05F, d.body_length * 0.32F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.02F, -d.body_length * 0.30F);

  float const front_forward_bias = d.body_length * 0.16F;
  float const front_bias_offset = d.body_length * 0.035F;

  draw_leg(front_anchor, 1.0F, front_forward_bias + front_bias_offset,
           g.front_leg_phase, sock_chance_fl);

  draw_leg(front_anchor, -1.0F, front_forward_bias - front_bias_offset,
           g.front_leg_phase + 0.50F, sock_chance_fr);

  float const rear_forward_bias = -d.body_length * 0.16F;
  float const rear_bias_offset = d.body_length * 0.032F;

  draw_leg(rear_anchor, 1.0F, rear_forward_bias - rear_bias_offset,
           g.rear_leg_phase + 0.50F, sock_chance_rl);

  draw_leg(rear_anchor, -1.0F, rear_forward_bias + rear_bias_offset,
           g.rear_leg_phase, sock_chance_rr);

  QVector3D const bit_left =
      muzzle_center + QVector3D(d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
  QVector3D const bit_right =
      muzzle_center + QVector3D(-d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
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
      QVector3D(0.0F, d.body_height * 0.55F, -d.body_length * 0.06F);
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
      QVector3D(0.0F, d.body_height * 0.46F, -d.body_length * 0.18F);
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
      rump_center + QVector3D(0.0F, d.body_height * 0.20F, -100.05F);
  body_frames.tail_base.origin = tail_base_pos;
  body_frames.tail_base.right = right;
  body_frames.tail_base.up = up;
  body_frames.tail_base.forward = forward;

  body_frames.muzzle.origin = muzzle_center;
  body_frames.muzzle.right = right;
  body_frames.muzzle.up = up;
  body_frames.muzzle.forward = forward;

  draw_attachments(horse_ctx, anim, rider_ctx, profile, mount, phase, bob,
                   rein_slack, body_frames, out);
}

void HorseRendererBase::render_simplified(
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

  QVector3D const barrel_center(0.0F, d.barrel_center_y + bob, 0.0F);

  {
    QMatrix4x4 body = horse_ctx.model;
    body.translate(barrel_center);
    body.scale(d.body_width * 1.0F, d.body_height * 0.85F,
               d.body_length * 0.80F);
    out.mesh(get_unit_sphere(), body, v.coat_color, nullptr, 1.0F, 6);
  }

  QVector3D const neck_base =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.35F, d.body_length * 0.35F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, d.neck_rise, d.neck_length);
  draw_cylinder(out, horse_ctx.model, neck_base, neck_top, d.body_width * 0.40F,
                v.coat_color, 1.0F);

  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * 0.10F, d.head_length * 0.40F);
  {
    QMatrix4x4 head = horse_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 0.90F, d.head_height * 0.85F,
               d.head_length * 0.75F);
    out.mesh(get_unit_sphere(), head, v.coat_color, nullptr, 1.0F);
  }

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.05F, d.body_length * 0.30F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.02F, -d.body_length * 0.28F);

  auto draw_simple_leg = [&](const QVector3D &anchor, float lateralSign,
                             float forwardBias, float phase_offset) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.stride_swing * 0.6F + forwardBias;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * 0.8F : 0.0F;
    }

    float const shoulder_out = d.body_width * 0.45F;
    QVector3D shoulder =
        anchor + QVector3D(lateralSign * shoulder_out, lift * 0.05F, stride);

    float const leg_length = d.leg_length * 0.85F;
    QVector3D const foot = shoulder + QVector3D(0.0F, -leg_length + lift, 0.0F);

    draw_cylinder(out, horse_ctx.model, shoulder, foot, d.body_width * 0.22F,
                  v.coat_color * 0.85F, 1.0F, 6);

    QMatrix4x4 hoof = horse_ctx.model;
    hoof.translate(foot);
    hoof.scale(d.body_width * 0.28F, d.hoof_height, d.body_width * 0.30F);
    out.mesh(get_unit_cylinder(), hoof, v.hoof_color, nullptr, 1.0F, 8);
  };

  draw_simple_leg(front_anchor, 1.0F, d.body_length * 0.15F, g.front_leg_phase);
  draw_simple_leg(front_anchor, -1.0F, d.body_length * 0.15F,
                  g.front_leg_phase + 0.48F);
  draw_simple_leg(rear_anchor, 1.0F, -d.body_length * 0.15F, g.rear_leg_phase);
  draw_simple_leg(rear_anchor, -1.0F, -d.body_length * 0.15F,
                  g.rear_leg_phase + 0.52F);
}

void HorseRendererBase::render_minimal(const DrawContext &ctx,
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

  QVector3D const center(0.0F, d.barrel_center_y + bob, 0.0F);

  QMatrix4x4 body = horse_ctx.model;
  body.translate(center);
  body.scale(d.body_width * 1.2F, d.body_height + d.neck_rise * 0.5F,
             d.body_length + d.head_length * 0.5F);
  out.mesh(get_unit_sphere(), body, v.coat_color, nullptr, 1.0F, 6);

  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.body_length * 0.25F : -d.body_length * 0.25F;

    QVector3D const top = center + QVector3D(x_sign * d.body_width * 0.40F,
                                             -d.body_height * 0.3F, z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.leg_length * 0.60F, 0.0F);

    draw_cylinder(out, horse_ctx.model, top, bottom, d.body_width * 0.15F,
                  v.coat_color * 0.75F, 1.0F, 6);
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

  ++s_horseRenderStats.horses_total;

  if (lod == HorseLOD::Billboard) {
    ++s_horseRenderStats.horses_skipped_lod;
    return;
  }

  ++s_horseRenderStats.horses_rendered;

  switch (lod) {
  case HorseLOD::Full:
    ++s_horseRenderStats.lod_full;
    render_full(ctx, anim, rider_ctx, profile, shared_mount, shared_reins,
                shared_motion, out);
    break;

  case HorseLOD::Reduced:
    ++s_horseRenderStats.lod_reduced;
    render_simplified(ctx, anim, rider_ctx, profile, shared_mount,
                      shared_motion, out);
    break;

  case HorseLOD::Minimal:
    ++s_horseRenderStats.lod_minimal;
    render_minimal(ctx, profile, shared_motion, out);
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
