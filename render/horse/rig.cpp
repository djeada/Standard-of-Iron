#include "rig.h"

#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../humanoid/rig.h"
#include "../submitter.h"
#include "horse_animation_controller.h"
#include "horse_spec.h"

#include <vector>

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <unordered_map>

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

struct CachedHorseProfileEntry {
  HorseProfile profile;
  QVector3D leather_base;
  QVector3D cloth_base;
  uint32_t frame_number{0};
};

using HorseProfileCacheKey = uint64_t;
static std::unordered_map<HorseProfileCacheKey, CachedHorseProfileEntry>
    s_horse_profile_cache;
static std::mutex s_horse_profile_cache_mutex;
static uint32_t s_horse_cache_frame = 0;
constexpr uint32_t k_horse_profile_cache_max_age = 600;

constexpr uint32_t k_cache_cleanup_interval_mask = 0x1FFU;

constexpr float k_color_hash_multiplier = 31.0F;

constexpr float k_color_comparison_tolerance = 0.001F;

inline auto make_horse_profile_cache_key(
    uint32_t seed, const QVector3D &leather_base,
    const QVector3D &cloth_base) -> HorseProfileCacheKey {

  auto color_to_5bit = [](float c) -> uint32_t {
    return static_cast<uint32_t>(std::clamp(c, 0.0F, 1.0F) *
                                 k_color_hash_multiplier);
  };

  uint32_t color_hash = color_to_5bit(leather_base.x());
  color_hash |= color_to_5bit(leather_base.y()) << 5;
  color_hash |= color_to_5bit(leather_base.z()) << 10;
  color_hash |= color_to_5bit(cloth_base.x()) << 15;
  color_hash |= color_to_5bit(cloth_base.y()) << 20;
  color_hash |= color_to_5bit(cloth_base.z()) << 25;
  return (static_cast<uint64_t>(seed) << 32) |
         static_cast<uint64_t>(color_hash);
}

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

auto get_or_create_cached_horse_profile(
    uint32_t seed, const QVector3D &leather_base,
    const QVector3D &cloth_base) -> HorseProfile {
  std::lock_guard<std::mutex> lock(s_horse_profile_cache_mutex);
  HorseProfileCacheKey cache_key =
      make_horse_profile_cache_key(seed, leather_base, cloth_base);

  auto cache_it = s_horse_profile_cache.find(cache_key);
  if (cache_it != s_horse_profile_cache.end()) {

    CachedHorseProfileEntry &entry = cache_it->second;
    if ((entry.leather_base - leather_base).lengthSquared() <
            k_color_comparison_tolerance &&
        (entry.cloth_base - cloth_base).lengthSquared() <
            k_color_comparison_tolerance) {
      entry.frame_number = s_horse_cache_frame;
      ++s_horseRenderStats.profiles_cached;
      return entry.profile;
    }
  }

  ++s_horseRenderStats.profiles_computed;
  HorseProfile profile = make_horse_profile(seed, leather_base, cloth_base);

  CachedHorseProfileEntry &new_entry = s_horse_profile_cache[cache_key];
  new_entry.profile = profile;
  new_entry.leather_base = leather_base;
  new_entry.cloth_base = cloth_base;
  new_entry.frame_number = s_horse_cache_frame;

  return profile;
}

void advance_horse_profile_cache_frame() {
  std::lock_guard<std::mutex> lock(s_horse_profile_cache_mutex);
  ++s_horse_cache_frame;

  if ((s_horse_cache_frame & k_cache_cleanup_interval_mask) == 0) {
    auto it = s_horse_profile_cache.begin();
    while (it != s_horse_profile_cache.end()) {
      if (s_horse_cache_frame - it->second.frame_number >
          k_horse_profile_cache_max_age) {
        it = s_horse_profile_cache.erase(it);
      } else {
        ++it;
      }
    }
  }
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
    if (speed < kIdleSpeedMax && !anim.is_moving) {
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

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(profile, anim, rider_ctx);
  const HorseGait &g = profile.gait;
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

  // Rider-driven body sway/pitch/nod — preserved so attachment frames
  // (saddle/bridle/rider) continue to align with the legacy silhouette.
  // The baked Full mesh itself is authored at baseline; per-frame
  // variation flows through the bone palette only.
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

  ReinState const rein_state =
      shared_reins ? *shared_reins : compute_rein_state(horse_seed, rider_ctx);
  float const rein_slack = rein_state.slack;
  (void)rein_state.tension;

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob + body_pitch,
                                spine_flex);

  QVector3D const chest_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.34F);
  QVector3D const rump_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.36F);

  QVector3D const neck_base =
      chest_center + QVector3D(head_lateral * 0.3F, d.body_height * 0.42F,
                               d.body_length * 0.08F);
  QVector3D const neck_top =
      neck_base + QVector3D(head_lateral * 0.8F, d.neck_rise + head_nod * 0.4F,
                            d.neck_length);
  QVector3D const head_center =
      neck_top + QVector3D(head_lateral,
                           d.head_height * (0.10F - head_nod * 0.20F),
                           d.head_length * 0.40F + head_nod * 0.03F);
  QVector3D const muzzle_center =
      head_center +
      QVector3D(0.0F, -d.head_height * 0.18F, d.head_length * 0.58F);

  // Build the per-frame Reduced-shaped HorseSpecPose then override the
  // neck/head/feet to inherit the rider-influenced positions above.
  // This keeps the runtime bone palette aligned with the Full silhouette
  // even though the bake itself uses a single static (baseline) PartGraph.
  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      d, g,
      Render::Horse::HorseReducedMotion{phase, bob, is_moving},
      pose);
  pose.barrel_center = barrel_center;
  pose.neck_base = neck_base;
  pose.neck_top = neck_top;
  pose.head_center = head_center;

  Render::Horse::submit_horse_lod(pose, v, Render::Creature::CreatureLOD::Full,
                                  horse_ctx.model, out);

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

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);
  if (!shared_mount) {
    apply_mount_vertical_offset(mount, motion.bob);
  }

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      d, g,
      Render::Horse::HorseReducedMotion{motion.phase, motion.bob,
                                        motion.is_moving},
      pose);
  Render::Horse::submit_horse_lod(
      pose, v, Render::Creature::CreatureLOD::Reduced, world_from_unit, out);
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

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(d, bob, pose);

  Render::Horse::submit_horse_lod(pose, v, Render::Creature::CreatureLOD::Minimal,
                                  world_from_unit, out);
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
