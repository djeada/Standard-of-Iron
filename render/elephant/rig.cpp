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
#include <unordered_map>

namespace Render::GL {

static ElephantRenderStats s_elephantRenderStats;

auto get_elephant_render_stats() -> const ElephantRenderStats & {
  return s_elephantRenderStats;
}

void reset_elephant_render_stats() { s_elephantRenderStats.reset(); }

using Render::Geom::clamp01;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::lerp;
using Render::Geom::smoothstep;

namespace {

struct CachedElephantProfileEntry {
  ElephantProfile profile;
  QVector3D fabric_base;
  QVector3D metal_base;
  uint32_t frame_number{0};
};

using ElephantProfileCacheKey = uint64_t;
static std::unordered_map<ElephantProfileCacheKey, CachedElephantProfileEntry>
    s_elephant_profile_cache;
static uint32_t s_elephant_cache_frame = 0;
constexpr uint32_t k_elephant_profile_cache_max_age = 600;

constexpr uint32_t k_cache_cleanup_interval_mask = 0x1FFU;

constexpr float k_color_hash_multiplier = 31.0F;

constexpr float k_color_comparison_tolerance = 0.001F;

inline auto make_elephant_profile_cache_key(
    uint32_t seed, const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantProfileCacheKey {

  auto color_to_5bit = [](float c) -> uint32_t {
    return static_cast<uint32_t>(std::clamp(c, 0.0F, 1.0F) *
                                 k_color_hash_multiplier);
  };

  uint32_t color_hash = color_to_5bit(fabric_base.x());
  color_hash |= color_to_5bit(fabric_base.y()) << 5;
  color_hash |= color_to_5bit(fabric_base.z()) << 10;
  color_hash |= color_to_5bit(metal_base.x()) << 15;
  color_hash |= color_to_5bit(metal_base.y()) << 20;
  color_hash |= color_to_5bit(metal_base.z()) << 25;
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

inline auto darken(const QVector3D &c, float k) -> QVector3D { return c * k; }
inline auto lighten(const QVector3D &c, float k) -> QVector3D {
  return {saturate(c.x() * k), saturate(c.y() * k), saturate(c.z() * k)};
}

inline auto lerp3(const QVector3D &a, const QVector3D &b,
                  float t) -> QVector3D {
  return {a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
          a.z() + (b.z() - a.z()) * t};
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

constexpr float k_skin_highlight_base = 0.50F;
constexpr float k_skin_vertical_factor = 0.30F;
constexpr float k_skin_longitudinal_factor = 0.15F;
constexpr float k_skin_seed_factor = 0.10F;
constexpr float k_skin_bright_factor = 1.06F;
constexpr float k_skin_shadow_factor = 0.88F;

inline auto skin_gradient(const QVector3D &skin, float vertical_factor,
                          float longitudinal_factor, float seed) -> QVector3D {
  float const highlight = saturate(
      k_skin_highlight_base + vertical_factor * k_skin_vertical_factor -
      longitudinal_factor * k_skin_longitudinal_factor +
      seed * k_skin_seed_factor);
  QVector3D const bright = lighten(skin, k_skin_bright_factor);
  QVector3D const shadow = darken(skin, k_skin_shadow_factor);
  return shadow * (1.0F - highlight) + bright * highlight;
}

} // namespace

namespace ElephantDimensionRange {

constexpr float kBodyLengthMin = 0.7333333F;
constexpr float kBodyLengthMax = 0.8666667F;
constexpr float kBodyWidthMin = 0.30F;
constexpr float kBodyWidthMax = 0.3666667F;
constexpr float kBodyHeightMin = 0.40F;
constexpr float kBodyHeightMax = 0.50F;

constexpr float kNeckLengthMin = 0.175F;
constexpr float kNeckLengthMax = 0.25F;
constexpr float kNeckWidthMin = 0.225F;
constexpr float kNeckWidthMax = 0.275F;

constexpr float kHeadLengthMin = 0.275F;
constexpr float kHeadLengthMax = 0.35F;
constexpr float kHeadWidthMin = 0.25F;
constexpr float kHeadWidthMax = 0.325F;
constexpr float kHeadHeightMin = 0.275F;
constexpr float kHeadHeightMax = 0.35F;

constexpr float kTrunkLengthMin = 0.80F;
constexpr float kTrunkLengthMax = 1.00F;
constexpr float kTrunkBaseRadiusMin = 0.09F;
constexpr float kTrunkBaseRadiusMax = 0.12F;
constexpr float kTrunkTipRadiusMin = 0.02F;
constexpr float kTrunkTipRadiusMax = 0.035F;

constexpr float kEarWidthMin = 0.35F;
constexpr float kEarWidthMax = 0.45F;
constexpr float kEarHeightMin = 0.40F;
constexpr float kEarHeightMax = 0.50F;
constexpr float kEarThicknessMin = 0.012F;
constexpr float kEarThicknessMax = 0.022F;

constexpr float kLegLengthMin = 0.70F;
constexpr float kLegLengthMax = 0.85F;
constexpr float kLegRadiusMin = 0.09F;
constexpr float kLegRadiusMax = 0.125F;
constexpr float kFootRadiusMin = 0.11F;
constexpr float kFootRadiusMax = 0.15F;

constexpr float kTailLengthMin = 0.35F;
constexpr float kTailLengthMax = 0.475F;

constexpr float kTuskLengthMin = 0.25F;
constexpr float kTuskLengthMax = 0.425F;
constexpr float kTuskRadiusMin = 0.02F;
constexpr float kTuskRadiusMax = 0.035F;

constexpr float kHowdahWidthMin = 0.40F;
constexpr float kHowdahWidthMax = 0.50F;
constexpr float kHowdahLengthMin = 0.50F;
constexpr float kHowdahLengthMax = 0.65F;
constexpr float kHowdahHeightMin = 0.20F;
constexpr float kHowdahHeightMax = 0.275F;

constexpr float kIdleBobAmplitudeMin = 0.004F;
constexpr float kIdleBobAmplitudeMax = 0.0075F;
constexpr float kMoveBobAmplitudeMin = 0.0175F;
constexpr float kMoveBobAmplitudeMax = 0.0275F;

constexpr uint32_t kSaltBodyLength = 0x12U;
constexpr uint32_t kSaltBodyWidth = 0x34U;
constexpr uint32_t kSaltBodyHeight = 0x56U;
constexpr uint32_t kSaltNeckLength = 0x78U;
constexpr uint32_t kSaltNeckWidth = 0x9AU;
constexpr uint32_t kSaltHeadLength = 0xBCU;
constexpr uint32_t kSaltHeadWidth = 0xDEU;
constexpr uint32_t kSaltHeadHeight = 0xF0U;
constexpr uint32_t kSaltTrunkLength = 0x123U;
constexpr uint32_t kSaltTrunkBaseRadius = 0x234U;
constexpr uint32_t kSaltTrunkTipRadius = 0x345U;
constexpr uint32_t kSaltEarWidth = 0x456U;
constexpr uint32_t kSaltEarHeight = 0x567U;
constexpr uint32_t kSaltEarThickness = 0x678U;
constexpr uint32_t kSaltLegLength = 0x789U;
constexpr uint32_t kSaltLegRadius = 0x89AU;
constexpr uint32_t kSaltFootRadius = 0x9ABU;
constexpr uint32_t kSaltTailLength = 0xABCU;
constexpr uint32_t kSaltTuskLength = 0xBCDU;
constexpr uint32_t kSaltTuskRadius = 0xCDEU;
constexpr uint32_t kSaltHowdahWidth = 0xDEFU;
constexpr uint32_t kSaltHowdahLength = 0xEF0U;
constexpr uint32_t kSaltHowdahHeight = 0xF01U;
constexpr uint32_t kSaltIdleBob = 0x102U;
constexpr uint32_t kSaltMoveBob = 0x213U;

} // namespace ElephantDimensionRange

auto make_elephant_dimensions(uint32_t seed) -> ElephantDimensions {
  using namespace ElephantDimensionRange;
  ElephantDimensions d{};

  d.body_length =
      rand_between(seed, kSaltBodyLength, kBodyLengthMin, kBodyLengthMax);
  d.body_width =
      rand_between(seed, kSaltBodyWidth, kBodyWidthMin, kBodyWidthMax);
  d.body_height =
      rand_between(seed, kSaltBodyHeight, kBodyHeightMin, kBodyHeightMax);

  d.neck_length =
      rand_between(seed, kSaltNeckLength, kNeckLengthMin, kNeckLengthMax);
  d.neck_width =
      rand_between(seed, kSaltNeckWidth, kNeckWidthMin, kNeckWidthMax);

  d.head_length =
      rand_between(seed, kSaltHeadLength, kHeadLengthMin, kHeadLengthMax);
  d.head_width =
      rand_between(seed, kSaltHeadWidth, kHeadWidthMin, kHeadWidthMax);
  d.head_height =
      rand_between(seed, kSaltHeadHeight, kHeadHeightMin, kHeadHeightMax);

  d.trunk_length =
      rand_between(seed, kSaltTrunkLength, kTrunkLengthMin, kTrunkLengthMax);
  d.trunk_base_radius = rand_between(seed, kSaltTrunkBaseRadius,
                                     kTrunkBaseRadiusMin, kTrunkBaseRadiusMax);
  d.trunk_tip_radius = rand_between(seed, kSaltTrunkTipRadius,
                                    kTrunkTipRadiusMin, kTrunkTipRadiusMax);

  d.ear_width = rand_between(seed, kSaltEarWidth, kEarWidthMin, kEarWidthMax);
  d.ear_height =
      rand_between(seed, kSaltEarHeight, kEarHeightMin, kEarHeightMax);
  d.ear_thickness =
      rand_between(seed, kSaltEarThickness, kEarThicknessMin, kEarThicknessMax);

  d.leg_length =
      rand_between(seed, kSaltLegLength, kLegLengthMin, kLegLengthMax);
  d.leg_radius =
      rand_between(seed, kSaltLegRadius, kLegRadiusMin, kLegRadiusMax);
  d.foot_radius =
      rand_between(seed, kSaltFootRadius, kFootRadiusMin, kFootRadiusMax);

  d.tail_length =
      rand_between(seed, kSaltTailLength, kTailLengthMin, kTailLengthMax);

  d.tusk_length =
      rand_between(seed, kSaltTuskLength, kTuskLengthMin, kTuskLengthMax);
  d.tusk_radius =
      rand_between(seed, kSaltTuskRadius, kTuskRadiusMin, kTuskRadiusMax);

  d.howdah_width =
      rand_between(seed, kSaltHowdahWidth, kHowdahWidthMin, kHowdahWidthMax);
  d.howdah_length =
      rand_between(seed, kSaltHowdahLength, kHowdahLengthMin, kHowdahLengthMax);
  d.howdah_height =
      rand_between(seed, kSaltHowdahHeight, kHowdahHeightMin, kHowdahHeightMax);

  d.idle_bob_amplitude =
      rand_between(seed, kSaltIdleBob, kIdleBobAmplitudeMin, kIdleBobAmplitudeMax);
  d.move_bob_amplitude =
      rand_between(seed, kSaltMoveBob, kMoveBobAmplitudeMin, kMoveBobAmplitudeMax);

  // Keep the lowest foot pad on the ground (y=0) in the neutral stance.
  // In `draw_leg`, the hip is offset down by `d.body_height * 0.35F` and the
  // leg reaches down by `d.leg_length`. The foot pad extends below `foot` by
  // roughly `d.foot_radius * (0.3F + 0.5F) = d.foot_radius * 0.8F`.
  d.barrel_center_y = d.leg_length + d.body_height * 0.35F + d.foot_radius * 0.8F;

  return d;
}

namespace ElephantVariantConstants {

constexpr float kSkinBaseR = 0.45F;
constexpr float kSkinBaseG = 0.42F;
constexpr float kSkinBaseB = 0.40F;

constexpr float kSkinVariationMin = 0.85F;
constexpr float kSkinVariationMax = 1.15F;

constexpr float kHighlightBlend = 0.15F;
constexpr float kShadowBlend = 0.20F;

constexpr float kEarInnerR = 0.55F;
constexpr float kEarInnerG = 0.45F;
constexpr float kEarInnerB = 0.42F;

constexpr float kTuskR = 0.95F;
constexpr float kTuskG = 0.92F;
constexpr float kTuskB = 0.85F;

constexpr float kToenailR = 0.35F;
constexpr float kToenailG = 0.32F;
constexpr float kToenailB = 0.28F;

constexpr float kWoodR = 0.45F;
constexpr float kWoodG = 0.32F;
constexpr float kWoodB = 0.22F;

constexpr uint32_t kSaltSkinVariation = 0x324U;
constexpr uint32_t kSaltHighlight = 0x435U;
constexpr uint32_t kSaltShadow = 0x546U;

} // namespace ElephantVariantConstants

auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant {
  using namespace ElephantVariantConstants;
  ElephantVariant v;

  float const skin_variation =
      rand_between(seed, kSaltSkinVariation, kSkinVariationMin, kSkinVariationMax);
  v.skin_color = QVector3D(kSkinBaseR * skin_variation,
                           kSkinBaseG * skin_variation,
                           kSkinBaseB * skin_variation);

  float const highlight_t = rand_between(seed, kSaltHighlight, 0.0F, kHighlightBlend);
  v.skin_highlight = lighten(v.skin_color, 1.0F + highlight_t);

  float const shadow_t = rand_between(seed, kSaltShadow, 0.0F, kShadowBlend);
  v.skin_shadow = darken(v.skin_color, 1.0F - shadow_t);

  v.ear_inner_color = QVector3D(kEarInnerR, kEarInnerG, kEarInnerB);
  v.tusk_color = QVector3D(kTuskR, kTuskG, kTuskB);
  v.toenail_color = QVector3D(kToenailR, kToenailG, kToenailB);

  v.howdah_wood_color = QVector3D(kWoodR, kWoodG, kWoodB);
  v.howdah_fabric_color = fabric_base;
  v.howdah_metal_color = metal_base;

  return v;
}

namespace ElephantGaitConstants {

// Slower cycle for ponderous elephant walk
constexpr float kCycleTimeMin = 2.20F;
constexpr float kCycleTimeMax = 2.80F;
constexpr float kFrontLegPhaseMin = 0.0F;
constexpr float kFrontLegPhaseMax = 0.10F;
constexpr float kDiagonalLeadMin = 0.48F;
constexpr float kDiagonalLeadMax = 0.52F;
// Much longer stride for visible leg movement
constexpr float kStrideSwingMin = 0.55F;
constexpr float kStrideSwingMax = 0.75F;
// Higher foot lift for clear swing arc
constexpr float kStrideLiftMin = 0.18F;
constexpr float kStrideLiftMax = 0.26F;

constexpr uint32_t kSaltCycleTime = 0x657U;
constexpr uint32_t kSaltFrontLegPhase = 0x768U;
constexpr uint32_t kSaltDiagonalLead = 0x879U;
constexpr uint32_t kSaltStrideSwing = 0x98AU;
constexpr uint32_t kSaltStrideLift = 0xA9BU;

} // namespace ElephantGaitConstants

auto make_elephant_profile(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantProfile {
  using namespace ElephantGaitConstants;
  ElephantProfile profile;
  profile.dims = make_elephant_dimensions(seed);
  profile.variant = make_elephant_variant(seed, fabric_base, metal_base);

  profile.gait.cycle_time =
      rand_between(seed, kSaltCycleTime, kCycleTimeMin, kCycleTimeMax);
  profile.gait.front_leg_phase =
      rand_between(seed, kSaltFrontLegPhase, kFrontLegPhaseMin, kFrontLegPhaseMax);
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

auto get_or_create_cached_elephant_profile(
    uint32_t seed, const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantProfile {
  ElephantProfileCacheKey cache_key =
      make_elephant_profile_cache_key(seed, fabric_base, metal_base);

  auto cache_it = s_elephant_profile_cache.find(cache_key);
  if (cache_it != s_elephant_profile_cache.end()) {
    CachedElephantProfileEntry &entry = cache_it->second;
    if ((entry.fabric_base - fabric_base).lengthSquared() <
            k_color_comparison_tolerance &&
        (entry.metal_base - metal_base).lengthSquared() <
            k_color_comparison_tolerance) {
      entry.frame_number = s_elephant_cache_frame;
      return entry.profile;
    }
  }

  ElephantProfile profile = make_elephant_profile(seed, fabric_base, metal_base);

  CachedElephantProfileEntry &new_entry = s_elephant_profile_cache[cache_key];
  new_entry.profile = profile;
  new_entry.fabric_base = fabric_base;
  new_entry.metal_base = metal_base;
  new_entry.frame_number = s_elephant_cache_frame;

  return profile;
}

void advance_elephant_profile_cache_frame() {
  ++s_elephant_cache_frame;

  if ((s_elephant_cache_frame & k_cache_cleanup_interval_mask) == 0) {
    auto it = s_elephant_profile_cache.begin();
    while (it != s_elephant_profile_cache.end()) {
      if (s_elephant_cache_frame - it->second.frame_number >
          k_elephant_profile_cache_max_age) {
        it = s_elephant_profile_cache.erase(it);
      } else {
        ++it;
      }
    }
  }
}

namespace HowdahFrameConstants {

constexpr float kHowdahBodyHeightOffset = 0.55F;
constexpr float kHowdahBodyLengthOffset = -0.10F;
constexpr float kSeatHeightOffset = 0.15F;
constexpr float kLegRevealLiftScale = 0.75F;

} // namespace HowdahFrameConstants

auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame {
  using namespace HowdahFrameConstants;
  const ElephantDimensions &d = profile.dims;
  HowdahAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);

  // Lift the whole rig up so legs/feet are clearly visible.
  frame.ground_offset = QVector3D(0.0F, -d.barrel_center_y + d.leg_length * kLegRevealLiftScale,
                                 0.0F);

  frame.howdah_center = QVector3D(
      0.0F, d.barrel_center_y + d.body_height * kHowdahBodyHeightOffset,
      d.body_length * kHowdahBodyLengthOffset);

  frame.seat_position =
      frame.howdah_center +
      QVector3D(0.0F, d.howdah_height * kSeatHeightOffset, 0.0F);

  return frame;
}

auto evaluate_elephant_motion(ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample {
  ElephantMotionSample sample{};
  const ElephantGait &g = profile.gait;
  const ElephantDimensions &d = profile.dims;

  sample.is_moving = anim.is_moving;

  if (sample.is_moving) {
    float const cycle_progress = std::fmod(anim.time / g.cycle_time, 1.0F);
    sample.phase = cycle_progress;
    sample.bob = std::sin(cycle_progress * 2.0F * k_pi) * d.move_bob_amplitude;
  } else {
    sample.phase = std::fmod(anim.time * 0.3F, 1.0F);
    sample.bob = std::sin(anim.time * 0.5F) * d.idle_bob_amplitude;
  }

  float const trunk_primary = std::sin(anim.time * 0.8F) * 0.15F;
  float const trunk_secondary = std::sin(anim.time * 1.3F + 0.5F) * 0.08F;
  sample.trunk_swing = trunk_primary + trunk_secondary;

  float const ear_base = std::sin(anim.time * 0.6F);
  sample.ear_flap = sample.is_moving ? ear_base * 0.25F : ear_base * 0.12F;

  return sample;
}

void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.howdah_center += offset;
  frame.seat_position += offset;
}

// ============================================================================
// Realistic Gait System: Stance/Swing with Foot Locking and 2-Bone IK
// ============================================================================

namespace GaitSystemConstants {

// Lateral sequence timing: elephants walk with a rolling pattern
// Phase offsets for each leg in the gait cycle [0,1)
// Order: FL lifts first, then RR, then FR, then RL (lateral sequence)
constexpr float kLegPhaseFL = 0.00F;   // Front-Left
constexpr float kLegPhaseFR = 0.50F;   // Front-Right (opposite side)
constexpr float kLegPhaseRL = 0.75F;   // Rear-Left
constexpr float kLegPhaseRR = 0.25F;   // Rear-Right

// Swing duration as fraction of cycle (rest is stance)
constexpr float kSwingDuration = 0.25F;

// Swing arc parameters - larger for visible, weighty movement
constexpr float kSwingLiftPeak = 0.22F;        // Max lift height as fraction of leg length
constexpr float kSwingForwardReach = 0.60F;    // How far ahead to place foot

// Weight transfer parameters
constexpr float kWeightShiftLateral = 0.025F;  // Side-to-side shift amplitude
constexpr float kWeightShiftForeAft = 0.015F;  // Front-back shift amplitude

// Secondary motion lag (0 = no lag, higher = more lag)
constexpr float kShoulderLagFactor = 0.08F;
constexpr float kHipLagFactor = 0.06F;

// Foot settle (compression on landing)
constexpr float kFootSettleDepth = 0.015F;
constexpr float kFootSettleDuration = 0.10F;  // As fraction of stance

} // namespace GaitSystemConstants

// Smooth ease-in/ease-out for swing trajectory
inline auto swing_ease(float t) -> float {
  // Hermite smoothstep for natural acceleration/deceleration
  return t * t * (3.0F - 2.0F * t);
}

// Vertical arc for swing (dome shape: 0 at ends, 1 at middle)
inline auto swing_arc(float t) -> float {
  // Parabola peaking at t=0.5
  return 4.0F * t * (1.0F - t);
}

auto evaluate_swing_position(const ElephantLegState &leg,
                             float lift_height) -> QVector3D {
  float const t = leg.swing_progress;
  float const eased_t = swing_ease(t);

  // Horizontal: interpolate from swing_start to swing_target
  QVector3D const horizontal = leg.swing_start * (1.0F - eased_t) +
                               leg.swing_target * eased_t;

  // Vertical: add arc lift
  float const lift = swing_arc(t) * lift_height;

  return QVector3D(horizontal.x(), horizontal.y() + lift, horizontal.z());
}

auto solve_elephant_leg_ik(const QVector3D &hip, const QVector3D &foot_target,
                           float upper_len, float lower_len,
                           float lateral_sign) -> ElephantLegPose {
  ElephantLegPose pose{};
  pose.hip = hip;

  // Vector from hip to foot
  QVector3D const to_foot = foot_target - hip;
  float const reach = to_foot.length();

  // Clamp reach to valid IK range
  float const max_reach = upper_len + lower_len - 0.01F;
  float const min_reach = std::abs(upper_len - lower_len) + 0.01F;
  float const clamped_reach = std::clamp(reach, min_reach, max_reach);

  // Direction from hip to foot
  QVector3D const reach_dir = reach > 0.001F ? to_foot / reach : QVector3D(0.0F, -1.0F, 0.0F);

  // Actual foot position (may be adjusted if reach was clamped)
  QVector3D const actual_foot = hip + reach_dir * clamped_reach;
  pose.foot = actual_foot;
  pose.ankle = actual_foot + QVector3D(0.0F, lower_len * 0.08F, 0.0F);

  // Law of cosines to find knee angle
  // a = upper_len, b = lower_len, c = clamped_reach
  float const a2 = upper_len * upper_len;
  float const b2 = lower_len * lower_len;
  float const c2 = clamped_reach * clamped_reach;

  // cos(angle at hip) = (a² + c² - b²) / (2ac)
  float const cos_hip_angle = (a2 + c2 - b2) / (2.0F * upper_len * clamped_reach);
  float const hip_angle = std::acos(std::clamp(cos_hip_angle, -1.0F, 1.0F));

  // Find knee position by rotating from hip-to-foot direction
  // Knee bends outward (laterally) and backward
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D bend_axis = QVector3D::crossProduct(reach_dir, up);
  if (bend_axis.lengthSquared() < 0.001F) {
    // Leg is nearly vertical, use lateral axis
    bend_axis = QVector3D(lateral_sign, 0.0F, 0.0F);
  }
  bend_axis.normalize();

  // Rotate the reach direction by hip_angle around bend_axis to get knee direction
  QMatrix4x4 rot;
  rot.setToIdentity();
  rot.rotate(hip_angle * 180.0F / 3.14159265F, bend_axis);

  QVector3D const knee_dir = rot.map(reach_dir);
  pose.knee = hip + knee_dir * upper_len;

  return pose;
}

// Get the leg phase offset for lateral sequence gait
inline auto get_leg_phase_offset(int leg_index) -> float {
  using namespace GaitSystemConstants;
  switch (leg_index) {
    case 0: return kLegPhaseFL;  // Front-Left
    case 1: return kLegPhaseFR;  // Front-Right
    case 2: return kLegPhaseRL;  // Rear-Left
    case 3: return kLegPhaseRR;  // Rear-Right
    default: return 0.0F;
  }
}

// Check if a leg is currently in swing phase based on cycle position
inline auto is_leg_in_swing(float cycle_phase, int leg_index) -> bool {
  using namespace GaitSystemConstants;
  float const leg_phase = std::fmod(cycle_phase - get_leg_phase_offset(leg_index) + 1.0F, 1.0F);
  return leg_phase < kSwingDuration;
}

// Get swing progress (0-1) if in swing, or -1 if in stance
inline auto get_swing_progress(float cycle_phase, int leg_index) -> float {
  using namespace GaitSystemConstants;
  float const leg_phase = std::fmod(cycle_phase - get_leg_phase_offset(leg_index) + 1.0F, 1.0F);
  if (leg_phase < kSwingDuration) {
    return leg_phase / kSwingDuration;
  }
  return -1.0F;  // In stance
}

// Calculate the default foot plant position for a leg (rest pose)
inline auto get_default_foot_position(const ElephantDimensions &d,
                                      int leg_index,
                                      const QVector3D &barrel_center) -> QVector3D {
  bool const is_front = (leg_index < 2);
  bool const is_left = (leg_index == 0 || leg_index == 2);
  float const lateral_sign = is_left ? 1.0F : -1.0F;
  // Wider fore-aft spread for more visible stride
  float const forward_offset = is_front ? d.body_length * 0.48F : -d.body_length * 0.48F;

  // Hip position - wider lateral stance for elephants
  QVector3D const hip = barrel_center +
      QVector3D(lateral_sign * d.body_width * 0.52F,
                -d.body_height * 0.42F,
                forward_offset);

  // Foot directly below hip at ground level
  return QVector3D(hip.x(), 0.0F, hip.z());
}

// Calculate swing target based on body velocity and current position
inline auto calculate_swing_target(const ElephantDimensions &d,
                                   int leg_index,
                                   const QVector3D &barrel_center,
                                   float stride_length) -> QVector3D {
  QVector3D const default_pos = get_default_foot_position(d, leg_index, barrel_center);
  // Place foot ahead by stride_length (forward direction is +Z)
  return default_pos + QVector3D(0.0F, 0.0F, stride_length);
}

void update_elephant_gait(ElephantGaitState &state,
                          const ElephantProfile &profile,
                          const AnimationInputs &anim,
                          const QVector3D &body_world_pos,
                          float body_forward_z) {
  using namespace GaitSystemConstants;
  const ElephantDimensions &d = profile.dims;
  const ElephantGait &g = profile.gait;

  // Initialize leg positions on first call
  if (!state.initialized) {
    QVector3D const barrel_center(0.0F, d.barrel_center_y, 0.0F);
    for (int i = 0; i < 4; ++i) {
      state.legs[i].planted_foot = get_default_foot_position(d, i, barrel_center);
      state.legs[i].swing_start = state.legs[i].planted_foot;
      state.legs[i].swing_target = state.legs[i].planted_foot;
      state.legs[i].in_swing = false;
      state.legs[i].swing_progress = 0.0F;
    }
    state.initialized = true;
  }

  // Update cycle phase
  if (anim.is_moving) {
    state.cycle_phase = std::fmod(anim.time / g.cycle_time, 1.0F);
  } else {
    // Idle: no gait cycle advancement, all feet planted
    state.cycle_phase = 0.0F;
    for (int i = 0; i < 4; ++i) {
      state.legs[i].in_swing = false;
    }
  }

  QVector3D const barrel_center(0.0F, d.barrel_center_y, 0.0F);
  // Use full stride swing value for meaningful leg travel
  float const stride_length = g.stride_swing * 1.8F;

  // Update each leg
  for (int i = 0; i < 4; ++i) {
    ElephantLegState &leg = state.legs[i];
    float const swing_progress = get_swing_progress(state.cycle_phase, i);

    if (swing_progress >= 0.0F && anim.is_moving) {
      // Leg is in swing phase
      if (!leg.in_swing) {
        // Just entered swing - record start position and calculate target
        leg.swing_start = leg.planted_foot;
        leg.swing_target = calculate_swing_target(d, i, barrel_center, stride_length);
        leg.in_swing = true;
      }
      leg.swing_progress = swing_progress;
    } else {
      // Leg is in stance phase
      if (leg.in_swing) {
        // Just landed - lock foot at target position
        leg.planted_foot = leg.swing_target;
        leg.in_swing = false;
      }
      leg.swing_progress = 0.0F;
    }
  }

  // Calculate weight transfer based on which feet are planted
  // Shift weight toward the center of the planted feet
  float total_x = 0.0F;
  float total_z = 0.0F;
  int planted_count = 0;

  for (int i = 0; i < 4; ++i) {
    if (!state.legs[i].in_swing) {
      total_x += state.legs[i].planted_foot.x();
      total_z += state.legs[i].planted_foot.z();
      ++planted_count;
    }
  }

  if (planted_count > 0) {
    float const center_x = total_x / static_cast<float>(planted_count);
    float const center_z = total_z / static_cast<float>(planted_count);
    // Shift body slightly toward planted feet center (subtle)
    state.weight_shift_x = -center_x * kWeightShiftLateral;
    state.weight_shift_z = -center_z * kWeightShiftForeAft * 0.5F;
  }

  // Secondary motion: lag for shoulders and hips
  if (anim.is_moving) {
    float const cycle_sin = std::sin(state.cycle_phase * 2.0F * k_pi);
    state.shoulder_lag = cycle_sin * kShoulderLagFactor;
    state.hip_lag = -cycle_sin * kHipLagFactor;
  } else {
    state.shoulder_lag *= 0.9F;  // Decay when stopped
    state.hip_lag *= 0.9F;
  }
}

void ElephantRendererBase::render_full(
    const DrawContext &ctx, const AnimationInputs &anim,
    ElephantProfile &profile, const HowdahAttachmentFrame *shared_howdah,
    const ElephantMotionSample *shared_motion, ISubmitter &out) const {
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;
  const ElephantGait &g = profile.gait;

  ElephantMotionSample const motion =
      shared_motion ? *shared_motion : evaluate_elephant_motion(profile, anim);
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;

  bool const is_fighting =
      anim.is_attacking || (anim.combat_phase != CombatAnimPhase::Idle);
  float const trunk_swing = motion.trunk_swing;
  float const ear_flap = motion.ear_flap;

  HowdahAttachmentFrame howdah =
      shared_howdah ? *shared_howdah : compute_howdah_frame(profile);
  if (!shared_howdah) {
    apply_howdah_vertical_offset(howdah, bob);
  }

  DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  uint32_t const vhash = color_hash(v.skin_color);
  float const skin_seed_a = hash01(vhash ^ 0x701U);
  float const skin_seed_b = hash01(vhash ^ 0x702U);

  float const body_sway = is_moving ? std::sin(phase * 2.0F * k_pi) * 0.015F
                                    : std::sin(anim.time * 0.3F) * 0.008F;

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob, 0.0F);

  {
    QMatrix4x4 body_main = elephant_ctx.model;
    body_main.translate(barrel_center);
    body_main.scale(d.body_width * 1.05F, d.body_height * 0.95F,
                    d.body_length * 0.55F);
    QVector3D const body_color =
        skin_gradient(v.skin_color, 0.60F, 0.0F, skin_seed_a);
    out.mesh(get_unit_sphere(), body_main, body_color, nullptr, 1.0F, 6);
  }

  QVector3D const chest_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.10F, d.body_length * 0.30F);
  {
    QMatrix4x4 chest = elephant_ctx.model;
    chest.translate(chest_center);
    chest.scale(d.body_width * 1.18F, d.body_height * 1.00F,
                d.body_length * 0.36F);
    out.mesh(get_unit_sphere(), chest,
             skin_gradient(v.skin_color, 0.70F, 0.15F, skin_seed_a), nullptr,
             1.0F, 6);
  }

  QVector3D const rump_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.02F, -d.body_length * 0.32F);
  {
    QMatrix4x4 rump = elephant_ctx.model;
    rump.translate(rump_center);
    rump.scale(d.body_width * 1.10F, d.body_height * 0.98F,
               d.body_length * 0.34F);
    out.mesh(get_unit_sphere(), rump,
             skin_gradient(v.skin_color, 0.55F, -0.20F, skin_seed_b), nullptr,
             1.0F, 6);
  }

  QVector3D const belly_center =
      barrel_center + QVector3D(0.0F, -d.body_height * 0.22F, d.body_length * 0.05F);
  {
    QMatrix4x4 belly = elephant_ctx.model;
    belly.translate(belly_center);
    belly.scale(d.body_width * 1.00F, d.body_height * 0.70F, d.body_length * 0.55F);
    out.mesh(get_unit_sphere(), belly, darken(v.skin_color, 0.92F), nullptr,
             1.0F, 6);
  }

  QVector3D const neck_base =
      chest_center + QVector3D(0.0F, d.body_height * 0.25F, d.body_length * 0.15F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, d.neck_length * 0.60F, d.neck_length * 0.50F);
  draw_cylinder(out, elephant_ctx.model, neck_base, neck_top, d.neck_width,
                skin_gradient(v.skin_color, 0.65F, 0.10F, skin_seed_a), 1.0F);

  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * 0.20F, d.head_length * 0.35F);
  {
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 1.0F, d.head_height * 0.90F,
               d.head_length * 0.80F);
    out.mesh(get_unit_sphere(), head, v.skin_color, nullptr, 1.0F);
  }

  {
    QMatrix4x4 forehead = elephant_ctx.model;
    forehead.translate(head_center + QVector3D(0.0F, d.head_height * 0.35F,
                                               d.head_length * 0.10F));
    forehead.scale(d.head_width * 0.85F, d.head_height * 0.45F,
                   d.head_length * 0.50F);
    out.mesh(get_unit_sphere(), forehead, lighten(v.skin_color, 1.05F), nullptr,
             1.0F);
  }

  QVector3D const trunk_base =
      head_center +
      QVector3D(0.0F, -d.head_height * 0.25F, d.head_length * 0.55F);

  constexpr int k_trunk_segments = 12;
  QVector3D prev_trunk = trunk_base;
  float prev_radius = d.trunk_base_radius;
  for (int i = 1; i <= k_trunk_segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_trunk_segments);

    float const segment_angle = t * k_pi * 0.6F;
    float const swing_offset = trunk_swing * t * t;
    float const curl_x = std::sin(anim.time * 0.5F + t * 2.0F) * 0.03F * t;

    QVector3D const segment_offset(
        curl_x + swing_offset,
        -d.trunk_length * t * std::cos(segment_angle) * 0.7F,
        d.trunk_length * t * std::sin(segment_angle) * 0.5F);

    QVector3D const curr_trunk = trunk_base + segment_offset;
    float const curr_radius =
        lerp(d.trunk_base_radius, d.trunk_tip_radius, t);

    draw_cylinder(out, elephant_ctx.model, prev_trunk, curr_trunk,
                  (prev_radius + curr_radius) * 0.5F,
                  skin_gradient(v.skin_color, 0.50F - t * 0.15F, 0.0F,
                                skin_seed_a * (1.0F - t * 0.3F)),
                  1.0F, 6);

    prev_trunk = curr_trunk;
    prev_radius = curr_radius;
  }

  {
    QMatrix4x4 trunk_tip_sphere = elephant_ctx.model;
    trunk_tip_sphere.translate(prev_trunk);
    trunk_tip_sphere.scale(d.trunk_tip_radius * 1.2F);
    out.mesh(get_unit_sphere(), trunk_tip_sphere, darken(v.skin_color, 0.85F),
             nullptr, 1.0F);
  }

  auto draw_ear = [&](float side) {
    float const flap_angle = ear_flap * side;
    QVector3D const ear_base =
        head_center +
        QVector3D(side * d.head_width * 0.75F, d.head_height * 0.10F,
                  -d.head_length * 0.15F);

    QVector3D const ear_tip =
        ear_base +
        QVector3D(side * d.ear_width * (0.85F + flap_angle * 0.3F),
                  -d.ear_height * 0.40F, -d.ear_width * 0.20F);

    QVector3D const ear_top =
        ear_base + QVector3D(side * d.ear_width * 0.50F, d.ear_height * 0.45F,
                             -d.ear_width * 0.10F);

    draw_cylinder(out, elephant_ctx.model, ear_base, ear_tip, d.ear_thickness,
                  v.skin_color, 1.0F, 6);
    draw_cylinder(out, elephant_ctx.model, ear_base, ear_top,
                  d.ear_thickness * 0.8F, v.skin_color, 1.0F, 6);

    {
      QMatrix4x4 ear_main = elephant_ctx.model;
      QVector3D const ear_center = (ear_base + ear_tip + ear_top) * 0.33F;
      ear_main.translate(ear_center);
      ear_main.rotate(side * (15.0F + flap_angle * 20.0F), 0.0F, 0.0F, 1.0F);
      ear_main.scale(d.ear_width * 0.70F, d.ear_height * 0.65F,
                     d.ear_thickness * 0.25F);
      out.mesh(get_unit_sphere(), ear_main, v.skin_color, nullptr, 1.0F, 6);
    }

    {
      QMatrix4x4 ear_inner = elephant_ctx.model;
      QVector3D const inner_center =
          (ear_base + ear_tip + ear_top) * 0.33F +
          QVector3D(side * d.ear_thickness * 0.5F, 0.0F, d.ear_thickness);
      ear_inner.translate(inner_center);
      ear_inner.rotate(side * (15.0F + flap_angle * 20.0F), 0.0F, 0.0F, 1.0F);
      ear_inner.scale(d.ear_width * 0.62F, d.ear_height * 0.55F,
              d.ear_thickness * 0.10F);
      out.mesh(get_unit_sphere(), ear_inner, v.ear_inner_color, nullptr, 1.0F,
               6);
    }
  };

  draw_ear(1.0F);
  draw_ear(-1.0F);

  auto draw_tusk = [&](float side) {
    QVector3D const tusk_base =
        head_center +
        QVector3D(side * d.head_width * 0.35F, -d.head_height * 0.30F,
                  d.head_length * 0.45F);
    QVector3D const tusk_tip =
        tusk_base +
        QVector3D(side * d.tusk_length * 0.25F, -d.tusk_length * 0.15F,
                  d.tusk_length * 0.90F);
    QVector3D const tusk_ctrl =
        (tusk_base + tusk_tip) * 0.5F +
        QVector3D(side * d.tusk_length * 0.08F, -d.tusk_length * 0.10F, 0.0F);

    constexpr int k_tusk_segments = 6;
    QVector3D prev_tusk = tusk_base;
    for (int i = 1; i <= k_tusk_segments; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(k_tusk_segments);
      QVector3D const curr_tusk = bezier(tusk_base, tusk_ctrl, tusk_tip, t);
      float const seg_radius = d.tusk_radius * (1.0F - t * 0.6F);
      draw_cylinder(out, elephant_ctx.model, prev_tusk, curr_tusk, seg_radius,
                    v.tusk_color, 1.0F, 8);
      prev_tusk = curr_tusk;
    }
  };

  draw_tusk(1.0F);
  draw_tusk(-1.0F);

  QVector3D const eye_left =
      head_center + QVector3D(d.head_width * 0.45F, d.head_height * 0.15F,
                              d.head_length * 0.25F);
  QVector3D const eye_right =
      head_center + QVector3D(-d.head_width * 0.45F, d.head_height * 0.15F,
                              d.head_length * 0.25F);
  float const eye_radius = d.head_width * 0.08F;
  QVector3D const eye_color(0.08F, 0.06F, 0.05F);

  {
    QMatrix4x4 eye_l = elephant_ctx.model;
    eye_l.translate(eye_left);
    eye_l.scale(eye_radius);
    out.mesh(get_unit_sphere(), eye_l, eye_color, nullptr, 1.0F);
  }
  {
    QMatrix4x4 eye_r = elephant_ctx.model;
    eye_r.translate(eye_right);
    eye_r.scale(eye_radius);
    out.mesh(get_unit_sphere(), eye_r, eye_color, nullptr, 1.0F);
  }

  // ========================================================================
  // Simplified Phase-Driven Leg Animation (stateless, smooth motion)
  // ========================================================================

  // Leg segment lengths
  float const upper_len = d.leg_length * 0.55F;
  float const lower_len = d.leg_length * 0.45F;

  // Full stride distance (how far each foot travels front-to-back)
  float const full_stride = g.stride_swing * 1.2F;
  // Lift height during swing
  float const lift_height = d.leg_length * 0.18F;

  bool const is_fighting =
      anim.is_attacking || (anim.combat_phase != CombatAnimPhase::Idle);

  auto draw_leg_phase = [&](int leg_index) {
    bool const is_front = (leg_index < 2);
    bool const is_left = (leg_index == 0 || leg_index == 2);
    float const lateral_sign = is_left ? 1.0F : -1.0F;

    // Wider stance - front legs slightly forward, rear slightly back
    float const base_forward = is_front ? d.body_length * 0.42F : -d.body_length * 0.42F;

    // Phase offset for lateral sequence gait (one leg at a time, rolling pattern)
    // FL=0.0, RR=0.25, FR=0.5, RL=0.75
    float phase_offset = 0.0F;
    switch (leg_index) {
      case 0: phase_offset = 0.00F; break;  // Front-Left
      case 1: phase_offset = 0.50F; break;  // Front-Right
      case 2: phase_offset = 0.75F; break;  // Rear-Left
      case 3: phase_offset = 0.25F; break;  // Rear-Right
    }

    float stride_offset = 0.0F;
    float lift = 0.0F;
    float forward_bias = 0.0F;

    if (is_fighting) {
      // Combat: stomp/brace cycle (stationary)
      float const stomp_period = 0.95F;
      float const t = std::fmod(anim.time / stomp_period + phase_offset, 1.0F);

      float intensity = 0.55F;
      switch (anim.combat_phase) {
      case CombatAnimPhase::WindUp:
        intensity = 0.65F;
        break;
      case CombatAnimPhase::Strike:
      case CombatAnimPhase::Impact:
        intensity = 1.0F;
        break;
      case CombatAnimPhase::Recover:
        intensity = 0.75F;
        break;
      default:
        break;
      }

      bool const stomp_is_left = (t < 0.5F);
      bool const stomp_leg = is_front && (is_left == stomp_is_left);
      float const local = (t < 0.5F) ? (t * 2.0F) : ((t - 0.5F) * 2.0F);

      float const stomp_height = d.leg_length * (0.10F + 0.06F * intensity);
      float const stomp_stride = full_stride * (0.10F + 0.05F * intensity);

      if (stomp_leg) {
        if (local < 0.35F) {
          float const u = local / 0.35F;
          lift = std::sin(u * k_pi * 0.5F) * stomp_height;
          stride_offset = stomp_stride * u;
        } else {
          float const u = (local - 0.35F) / 0.65F;
          lift = std::cos(u * k_pi * 0.5F) * stomp_height;
          stride_offset = stomp_stride * (1.0F - u);
        }
        forward_bias = (local < 0.5F) ? 1.0F : -1.0F;
      } else {
        float const wobble =
            std::sin((t + (is_left ? 0.0F : 0.5F)) * 2.0F * k_pi);
        stride_offset =
            -wobble * stomp_stride * (is_front ? 0.20F : 0.15F);
        forward_bias = wobble;
      }

    } else {
      // Walk gait
      float const leg_phase = std::fmod(phase + phase_offset, 1.0F);

      float const stride_angle = leg_phase * 2.0F * k_pi;
      stride_offset = std::sin(stride_angle) * full_stride * 0.5F;

      float const forward_velocity = std::cos(stride_angle);
      forward_bias = forward_velocity;

      if (is_moving && forward_velocity > 0.0F) {
        if (leg_phase < 0.25F) {
          float const u = leg_phase / 0.25F;
          lift = std::sin(u * k_pi * 0.5F) * lift_height;
        } else if (leg_phase < 0.50F) {
          float const u = (leg_phase - 0.25F) / 0.25F;
          lift = std::cos(u * k_pi * 0.5F) * lift_height;
        }
      }
    }

    // Hip position (wider lateral stance)
    QVector3D const hip =
        barrel_center +
        QVector3D(lateral_sign * d.body_width * 0.48F,
                  -d.body_height * 0.40F,
                  base_forward);

    // Foot position with stride and lift
    QVector3D const foot_target(
        hip.x(),
        lift,  // Ground level + lift
      hip.z() + ((is_moving || is_fighting) ? stride_offset : 0.0F));

    // Solve IK to find knee position
    ElephantLegPose pose = solve_elephant_leg_ik(hip, foot_target, upper_len, lower_len, lateral_sign);

    float const upper_radius = d.leg_radius * (is_front ? 1.05F : 1.10F);
    float const lower_radius = d.leg_radius * (is_front ? 0.80F : 0.85F);

    // Draw upper leg (hip to knee)
    draw_cylinder(out, elephant_ctx.model, pose.hip, pose.knee, upper_radius,
                  skin_gradient(v.skin_color, 0.45F,
                                forward_bias > 0.0F ? 0.1F : -0.1F,
                                skin_seed_a),
                  1.0F, 6);

    // Knee joint sphere
    {
      QMatrix4x4 knee_joint = elephant_ctx.model;
      knee_joint.translate(pose.knee);
      knee_joint.scale(lower_radius * 1.15F);
      out.mesh(get_unit_sphere(), knee_joint, darken(v.skin_color, 0.92F),
               nullptr, 1.0F, 6);
    }

    // Draw lower leg (knee to foot)
    draw_cylinder(out, elephant_ctx.model, pose.knee, pose.foot, lower_radius,
                  skin_gradient(v.skin_color, 0.40F, 0.0F, skin_seed_b), 1.0F,
                  6);

    // Ankle bulge
    {
      QVector3D const ankle = pose.foot + QVector3D(0.0F, d.foot_radius * 0.15F, 0.0F);
      QMatrix4x4 ankle_joint = elephant_ctx.model;
      ankle_joint.translate(ankle);
      ankle_joint.scale(lower_radius * 1.10F);
      out.mesh(get_unit_sphere(), ankle_joint, darken(v.skin_color, 0.90F),
               nullptr, 1.0F, 6);
    }

    // Foot pad
    {
      QMatrix4x4 foot_pad = elephant_ctx.model;
      foot_pad.translate(pose.foot + QVector3D(0.0F, -d.foot_radius * 0.18F, 0.0F));
      foot_pad.scale(d.foot_radius * 1.10F, d.foot_radius * 0.70F,
                     d.foot_radius * 1.20F);
      out.mesh(get_unit_sphere(), foot_pad, darken(v.skin_color, 0.80F),
               nullptr, 1.0F, 8);
    }

    // Toenails
    constexpr int k_toenails = 4;
    for (int t = 0; t < k_toenails; ++t) {
      float const toe_angle =
          (static_cast<float>(t) / static_cast<float>(k_toenails - 1) - 0.5F) *
          k_pi * 0.6F;
      QVector3D const nail_pos =
          pose.foot + QVector3D(std::sin(toe_angle) * d.foot_radius * 0.8F,
                           -d.foot_radius * 0.35F,
                           std::cos(toe_angle) * d.foot_radius * 0.9F);
      {
        QMatrix4x4 nail = elephant_ctx.model;
        nail.translate(nail_pos);
        nail.scale(d.foot_radius * 0.18F, d.foot_radius * 0.25F,
                   d.foot_radius * 0.22F);
        out.mesh(get_unit_sphere(), nail, v.toenail_color, nullptr, 1.0F, 8);
      }
    }
  };

  // Draw all 4 legs using phase-driven animation
  draw_leg_phase(0);  // Front-Left
  draw_leg_phase(1);  // Front-Right
  draw_leg_phase(2);  // Rear-Left
  draw_leg_phase(3);  // Rear-Right

  QVector3D const tail_base =
      rump_center +
      QVector3D(0.0F, d.body_height * 0.15F, -d.body_length * 0.32F);
  float const tail_sway =
      is_moving ? std::sin(phase * 4.0F * k_pi) * 0.08F
                : std::sin(anim.time * 0.7F) * 0.04F;

  constexpr int k_tail_segments = 8;
  QVector3D prev_tail = tail_base;
  for (int i = 1; i <= k_tail_segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_tail_segments);
    QVector3D const curr_tail =
        tail_base +
        QVector3D(tail_sway * t, -d.tail_length * t * 0.85F,
                  -d.tail_length * t * 0.35F);
    float const seg_radius = d.leg_radius * 0.25F * (1.0F - t * 0.6F);
    draw_cylinder(out, elephant_ctx.model, prev_tail, curr_tail, seg_radius,
                  darken(v.skin_color, 0.85F), 1.0F, 6);
    prev_tail = curr_tail;
  }

  {
    QMatrix4x4 tail_tuft = elephant_ctx.model;
    tail_tuft.translate(prev_tail);
    tail_tuft.scale(d.leg_radius * 0.20F, d.leg_radius * 0.35F,
                    d.leg_radius * 0.15F);
    out.mesh(get_unit_sphere(), tail_tuft, darken(v.skin_color, 0.70F), nullptr,
             1.0F);
  }

  ElephantBodyFrames body_frames;
  QVector3D const forward(0.0F, 0.0F, 1.0F);
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const right(1.0F, 0.0F, 0.0F);

  body_frames.head.origin = head_center;
  body_frames.head.right = right;
  body_frames.head.up = up;
  body_frames.head.forward = forward;

  body_frames.back_center.origin = howdah.howdah_center;
  body_frames.back_center.right = right;
  body_frames.back_center.up = up;
  body_frames.back_center.forward = forward;

  body_frames.howdah.origin = howdah.seat_position;
  body_frames.howdah.right = right;
  body_frames.howdah.up = up;
  body_frames.howdah.forward = forward;

  draw_howdah(elephant_ctx, anim, profile, howdah, phase, bob, body_frames, out);
}

void ElephantRendererBase::render_simplified(
    const DrawContext &ctx, const AnimationInputs &anim,
    ElephantProfile &profile, const HowdahAttachmentFrame *shared_howdah,
    const ElephantMotionSample *shared_motion, ISubmitter &out) const {
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;
  const ElephantGait &g = profile.gait;

  ElephantMotionSample const motion =
      shared_motion ? *shared_motion : evaluate_elephant_motion(profile, anim);
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;

  HowdahAttachmentFrame howdah =
      shared_howdah ? *shared_howdah : compute_howdah_frame(profile);
  if (!shared_howdah) {
    apply_howdah_vertical_offset(howdah, bob);
  }

  DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  QVector3D const barrel_center(0.0F, d.barrel_center_y + bob, 0.0F);

  {
    QMatrix4x4 body = elephant_ctx.model;
    body.translate(barrel_center);
    body.scale(d.body_width * 1.0F, d.body_height * 0.90F,
               d.body_length * 0.75F);
    out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, 6);
  }

  QVector3D const neck_base =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.20F, d.body_length * 0.45F);
  QVector3D const head_center =
      neck_base + QVector3D(0.0F, d.neck_length * 0.50F, d.head_length * 0.60F);
  draw_cylinder(out, elephant_ctx.model, neck_base, head_center,
                d.neck_width * 0.85F, v.skin_color, 1.0F);

  {
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 0.85F, d.head_height * 0.80F,
               d.head_length * 0.70F);
    out.mesh(get_unit_sphere(), head, v.skin_color, nullptr, 1.0F);
  }

  QVector3D const trunk_end =
      head_center + QVector3D(0.0F, -d.trunk_length * 0.50F,
                              d.trunk_length * 0.40F);
  draw_cone(out, elephant_ctx.model, trunk_end, head_center,
            d.trunk_base_radius * 0.8F, darken(v.skin_color, 0.90F), 1.0F);

  auto draw_simple_leg = [&](float lateral_sign, float forward_bias,
                             float phase_offset) {
    float const leg_phase =
        is_fighting ? std::fmod(anim.time / 0.95F + phase_offset, 1.0F)
                    : std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_fighting) {
      bool const is_front = (forward_bias > 0.0F);
      bool const is_left = (lateral_sign > 0.0F);
      bool const stomp_is_left = (leg_phase < 0.5F);
      bool const stomp_leg = is_front && (is_left == stomp_is_left);
      float const local = (leg_phase < 0.5F) ? (leg_phase * 2.0F)
                                             : ((leg_phase - 0.5F) * 2.0F);

      float intensity = 0.55F;
      switch (anim.combat_phase) {
      case CombatAnimPhase::WindUp:
        intensity = 0.65F;
        break;
      case CombatAnimPhase::Strike:
      case CombatAnimPhase::Impact:
        intensity = 1.0F;
        break;
      case CombatAnimPhase::Recover:
        intensity = 0.75F;
        break;
      default:
        break;
      }

      float const stomp_height = d.leg_length * (0.08F + 0.05F * intensity);
      float const stomp_stride = g.stride_swing * (0.08F + 0.04F * intensity);

      if (stomp_leg) {
        if (local < 0.35F) {
          float const u = local / 0.35F;
          lift = std::sin(u * k_pi * 0.5F) * stomp_height;
          stride = stomp_stride * u;
        } else {
          float const u = (local - 0.35F) / 0.65F;
          lift = std::cos(u * k_pi * 0.5F) * stomp_height;
          stride = stomp_stride * (1.0F - u);
        }
      } else {
        float const wobble = std::sin(leg_phase * 2.0F * k_pi);
        stride = -wobble * stomp_stride * 0.20F;
        lift = 0.0F;
      }

    } else if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.stride_swing * 0.6F;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * 0.8F : 0.0F;
    }

    QVector3D const hip =
        barrel_center +
        QVector3D(lateral_sign * d.body_width * 0.40F, -d.body_height * 0.30F,
                  forward_bias + stride);
    QVector3D const foot =
        hip + QVector3D(0.0F, -d.leg_length * 0.85F + lift, stride * 0.3F);

    draw_cylinder(out, elephant_ctx.model, hip, foot, d.leg_radius * 0.85F,
                  darken(v.skin_color, 0.88F), 1.0F, 6);

    {
      QMatrix4x4 foot_pad = elephant_ctx.model;
      foot_pad.translate(foot + QVector3D(0.0F, -d.foot_radius * 0.18F, 0.0F));
      foot_pad.scale(d.foot_radius * 1.00F, d.foot_radius * 0.65F,
                     d.foot_radius * 1.10F);
      out.mesh(get_unit_sphere(), foot_pad, darken(v.skin_color, 0.75F),
               nullptr, 1.0F, 8);
    }
  };

  float const front_forward = d.body_length * 0.35F;
  float const rear_forward = -d.body_length * 0.35F;

  draw_simple_leg(1.0F, front_forward, g.front_leg_phase);
  draw_simple_leg(-1.0F, front_forward, g.front_leg_phase + 0.50F);
  draw_simple_leg(1.0F, rear_forward, g.rear_leg_phase);
  draw_simple_leg(-1.0F, rear_forward, g.rear_leg_phase + 0.50F);
}

void ElephantRendererBase::render_minimal(
    const DrawContext &ctx, ElephantProfile &profile,
    const ElephantMotionSample *shared_motion, ISubmitter &out) const {
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;

  HowdahAttachmentFrame howdah = compute_howdah_frame(profile);
  apply_howdah_vertical_offset(howdah, bob);

  DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  QVector3D const center(0.0F, d.barrel_center_y + bob, 0.0F);

  QMatrix4x4 body = elephant_ctx.model;
  body.translate(center);
  body.scale(d.body_width * 1.2F, d.body_height + d.neck_length * 0.3F,
             d.body_length + d.head_length * 0.3F);
  out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, 6);

  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.body_length * 0.30F : -d.body_length * 0.30F;

    QVector3D const top = center + QVector3D(x_sign * d.body_width * 0.38F,
                                             -d.body_height * 0.25F, z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.leg_length * 0.70F, 0.0F);

    draw_cylinder(out, elephant_ctx.model, top, bottom, d.leg_radius * 0.70F,
                  darken(v.skin_color, 0.80F), 1.0F, 6);
  }
}

void ElephantRendererBase::render(
    const DrawContext &ctx, const AnimationInputs &anim,
    ElephantProfile &profile, const HowdahAttachmentFrame *shared_howdah,
    const ElephantMotionSample *shared_motion, ISubmitter &out,
    HorseLOD lod) const {

  ++s_elephantRenderStats.elephants_total;

  if (lod == HorseLOD::Billboard) {
    ++s_elephantRenderStats.elephants_skipped_lod;
    return;
  }

  ++s_elephantRenderStats.elephants_rendered;

  switch (lod) {
  case HorseLOD::Full:
    ++s_elephantRenderStats.lod_full;
    render_full(ctx, anim, profile, shared_howdah, shared_motion, out);
    break;

  case HorseLOD::Reduced:
    ++s_elephantRenderStats.lod_reduced;
    render_simplified(ctx, anim, profile, shared_howdah, shared_motion, out);
    break;

  case HorseLOD::Minimal:
    ++s_elephantRenderStats.lod_minimal;
    render_minimal(ctx, profile, shared_motion, out);
    break;

  case HorseLOD::Billboard:
    break;
  }
}

void ElephantRendererBase::render(
    const DrawContext &ctx, const AnimationInputs &anim,
    ElephantProfile &profile, const HowdahAttachmentFrame *shared_howdah,
    const ElephantMotionSample *shared_motion, ISubmitter &out) const {
  render(ctx, anim, profile, shared_howdah, shared_motion, out, HorseLOD::Full);
}

} // namespace Render::GL
