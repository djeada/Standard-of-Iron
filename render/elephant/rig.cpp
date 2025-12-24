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

inline auto skin_gradient(const QVector3D &skin_base, float vertical_factor,
                          float wrinkle_factor, float seed) -> QVector3D {
  float const highlight = saturate(
      0.45F + vertical_factor * 0.25F - wrinkle_factor * 0.15F + seed * 0.05F);
  QVector3D const bright = lighten(skin_base, 1.05F);
  QVector3D const shadow = darken(skin_base, 0.88F);
  return shadow * (1.0F - highlight) + bright * highlight;
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

inline auto bezier(const QVector3D &p0, const QVector3D &p1,
                   const QVector3D &p2, float t) -> QVector3D {
  float const u = 1.0F - t;
  return p0 * (u * u) + p1 * (2.0F * u * t) + p2 * (t * t);
}

inline auto bezier_cubic(const QVector3D &p0, const QVector3D &p1,
                         const QVector3D &p2, const QVector3D &p3,
                         float t) -> QVector3D {
  float const u = 1.0F - t;
  float const u2 = u * u;
  float const u3 = u2 * u;
  float const t2 = t * t;
  float const t3 = t2 * t;
  return p0 * u3 + p1 * (3.0F * u2 * t) + p2 * (3.0F * u * t2) + p3 * t3;
}

} // namespace

namespace ElephantDimensionRange {

// Elephants are much larger than horses - scaling body dimensions accordingly
constexpr float kBodyLengthMin = 2.2F;
constexpr float kBodyLengthMax = 2.8F;
constexpr float kBodyWidthMin = 1.4F;
constexpr float kBodyWidthMax = 1.8F;
constexpr float kBodyHeightMin = 1.6F;
constexpr float kBodyHeightMax = 2.0F;

constexpr float kNeckLengthMin = 0.6F;
constexpr float kNeckLengthMax = 0.8F;
constexpr float kNeckWidthMin = 0.9F;
constexpr float kNeckWidthMax = 1.2F;

constexpr float kHeadLengthMin = 0.8F;
constexpr float kHeadLengthMax = 1.0F;
constexpr float kHeadWidthMin = 0.7F;
constexpr float kHeadWidthMax = 0.9F;
constexpr float kHeadHeightMin = 0.9F;
constexpr float kHeadHeightMax = 1.1F;

// Trunk dimensions - key feature of elephants
constexpr float kTrunkLengthMin = 1.4F;
constexpr float kTrunkLengthMax = 1.8F;
constexpr float kTrunkBaseRadiusMin = 0.18F;
constexpr float kTrunkBaseRadiusMax = 0.24F;
constexpr float kTrunkTipRadiusMin = 0.08F;
constexpr float kTrunkTipRadiusMax = 0.12F;

// Large elephant ears
constexpr float kEarWidthMin = 0.7F;
constexpr float kEarWidthMax = 0.9F;
constexpr float kEarHeightMin = 0.8F;
constexpr float kEarHeightMax = 1.0F;
constexpr float kEarThicknessMin = 0.04F;
constexpr float kEarThicknessMax = 0.06F;

// Pillar-like legs
constexpr float kLegLengthMin = 1.6F;
constexpr float kLegLengthMax = 2.0F;
constexpr float kLegRadiusMin = 0.28F;
constexpr float kLegRadiusMax = 0.36F;
constexpr float kFootRadiusMin = 0.35F;
constexpr float kFootRadiusMax = 0.45F;

constexpr float kTailLengthMin = 0.8F;
constexpr float kTailLengthMax = 1.1F;

// Tusks
constexpr float kTuskLengthMin = 0.6F;
constexpr float kTuskLengthMax = 1.2F;
constexpr float kTuskRadiusMin = 0.06F;
constexpr float kTuskRadiusMax = 0.10F;

// Howdah (rider platform)
constexpr float kHowdahWidthMin = 1.0F;
constexpr float kHowdahWidthMax = 1.3F;
constexpr float kHowdahLengthMin = 1.2F;
constexpr float kHowdahLengthMax = 1.6F;
constexpr float kHowdahHeightMin = 0.5F;
constexpr float kHowdahHeightMax = 0.7F;

constexpr float kIdleBobAmplitudeMin = 0.008F;
constexpr float kIdleBobAmplitudeMax = 0.012F;
constexpr float kMoveBobAmplitudeMin = 0.035F;
constexpr float kMoveBobAmplitudeMax = 0.045F;

constexpr float kBarrelCenterScale = 0.52F;

constexpr uint32_t kSaltBodyLength = 0x21U;
constexpr uint32_t kSaltBodyWidth = 0x43U;
constexpr uint32_t kSaltBodyHeight = 0x65U;
constexpr uint32_t kSaltNeckLength = 0xA9U;
constexpr uint32_t kSaltNeckWidth = 0xCBU;
constexpr uint32_t kSaltHeadLength = 0xEDU;
constexpr uint32_t kSaltHeadWidth = 0x1FU;
constexpr uint32_t kSaltHeadHeight = 0x3571U;
constexpr uint32_t kSaltTrunkLength = 0x7531U;
constexpr uint32_t kSaltTrunkBaseRadius = 0x9753U;
constexpr uint32_t kSaltTrunkTipRadius = 0x1357U;
constexpr uint32_t kSaltEarWidth = 0x2468U;
constexpr uint32_t kSaltEarHeight = 0x369CU;
constexpr uint32_t kSaltEarThickness = 0x48AEU;
constexpr uint32_t kSaltLegLength = 0x5ABCU;
constexpr uint32_t kSaltLegRadius = 0x6CDEU;
constexpr uint32_t kSaltFootRadius = 0x8642U;
constexpr uint32_t kSaltTailLength = 0xA864U;
constexpr uint32_t kSaltTuskLength = 0xB975U;
constexpr uint32_t kSaltTuskRadius = 0xC086U;
constexpr uint32_t kSaltHowdahWidth = 0xD197U;
constexpr uint32_t kSaltHowdahLength = 0xE2A8U;
constexpr uint32_t kSaltHowdahHeight = 0xF3B9U;
constexpr uint32_t kSaltIdleBob = 0x04CAU;
constexpr uint32_t kSaltMoveBob = 0x15DBU;

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
  d.ear_thickness = rand_between(seed, kSaltEarThickness, kEarThicknessMin,
                                 kEarThicknessMax);

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
  d.howdah_length = rand_between(seed, kSaltHowdahLength, kHowdahLengthMin,
                                 kHowdahLengthMax);
  d.howdah_height = rand_between(seed, kSaltHowdahHeight, kHowdahHeightMin,
                                 kHowdahHeightMax);

  d.idle_bob_amplitude = rand_between(seed, kSaltIdleBob, kIdleBobAmplitudeMin,
                                      kIdleBobAmplitudeMax);
  d.move_bob_amplitude = rand_between(seed, kSaltMoveBob, kMoveBobAmplitudeMin,
                                      kMoveBobAmplitudeMax);

  d.barrel_center_y = d.leg_length + d.body_height * kBarrelCenterScale;

  return d;
}

namespace ElephantVariantConstants {

constexpr float kGraySkinThreshold = 0.40F;
constexpr float kBrownSkinThreshold = 0.75F;

constexpr float kGraySkinR = 0.48F;
constexpr float kGraySkinG = 0.46F;
constexpr float kGraySkinB = 0.44F;
constexpr float kBrownSkinR = 0.38F;
constexpr float kBrownSkinG = 0.32F;
constexpr float kBrownSkinB = 0.28F;
constexpr float kDarkSkinR = 0.28F;
constexpr float kDarkSkinG = 0.26F;
constexpr float kDarkSkinB = 0.24F;

constexpr float kEarInnerLighten = 1.15F;
constexpr float kTuskColorR = 0.92F;
constexpr float kTuskColorG = 0.88F;
constexpr float kTuskColorB = 0.82F;
constexpr float kToenailColorR = 0.35F;
constexpr float kToenailColorG = 0.32F;
constexpr float kToenailColorB = 0.30F;

constexpr float kWoodToneMin = 0.65F;
constexpr float kWoodToneMax = 0.85F;
constexpr float kMetalToneMin = 0.75F;
constexpr float kMetalToneMax = 0.95F;
constexpr float kFabricTintMin = 0.88F;
constexpr float kFabricTintMax = 1.08F;

constexpr uint32_t kSaltSkinHue = 0x34567U;
constexpr uint32_t kSaltWoodTone = 0x8899U;
constexpr uint32_t kSaltMetalTone = 0x99AAU;
constexpr uint32_t kSaltFabricTint = 0xAAB0U;

} // namespace ElephantVariantConstants

auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant {
  using namespace ElephantVariantConstants;
  ElephantVariant v;

  float const skin_hue = hash01(seed ^ kSaltSkinHue);
  if (skin_hue < kGraySkinThreshold) {
    v.skin_color = QVector3D(kGraySkinR, kGraySkinG, kGraySkinB);
  } else if (skin_hue < kBrownSkinThreshold) {
    v.skin_color = QVector3D(kBrownSkinR, kBrownSkinG, kBrownSkinB);
  } else {
    v.skin_color = QVector3D(kDarkSkinR, kDarkSkinG, kDarkSkinB);
  }

  v.skin_highlight = lighten(v.skin_color, 1.12F);
  v.skin_shadow = darken(v.skin_color, 0.85F);
  v.ear_inner_color = lighten(v.skin_color, kEarInnerLighten);
  v.tusk_color = QVector3D(kTuskColorR, kTuskColorG, kTuskColorB);
  v.toenail_color = QVector3D(kToenailColorR, kToenailColorG, kToenailColorB);

  float const wood_tone =
      rand_between(seed, kSaltWoodTone, kWoodToneMin, kWoodToneMax);
  v.howdah_wood_color = QVector3D(0.45F, 0.32F, 0.22F) * wood_tone;

  float const metal_tone =
      rand_between(seed, kSaltMetalTone, kMetalToneMin, kMetalToneMax);
  v.howdah_metal_color = metal_base * metal_tone;

  v.howdah_fabric_color = fabric_base * rand_between(seed, kSaltFabricTint,
                                                      kFabricTintMin, kFabricTintMax);

  return v;
}

namespace ElephantGaitConstants {

// Elephants walk slower than horses
constexpr float kCycleTimeMin = 1.2F;
constexpr float kCycleTimeMax = 1.6F;
constexpr float kFrontLegPhaseMin = 0.05F;
constexpr float kFrontLegPhaseMax = 0.12F;
constexpr float kDiagonalLeadMin = 0.48F;
constexpr float kDiagonalLeadMax = 0.52F;
constexpr float kStrideSwingMin = 0.18F;
constexpr float kStrideSwingMax = 0.24F;
constexpr float kStrideLiftMin = 0.06F;
constexpr float kStrideLiftMax = 0.10F;

constexpr uint32_t kSaltCycleTime = 0xBA12U;
constexpr uint32_t kSaltFrontLegPhase = 0xCB34U;
constexpr uint32_t kSaltDiagonalLead = 0xDC56U;
constexpr uint32_t kSaltStrideSwing = 0xED78U;
constexpr uint32_t kSaltStrideLift = 0xFE9AU;

} // namespace ElephantGaitConstants

auto make_elephant_profile(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantProfile {
  using namespace ElephantGaitConstants;
  ElephantProfile profile;
  profile.dims = make_elephant_dimensions(seed);
  profile.variant = make_elephant_variant(seed, fabric_base, metal_base);

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

auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame {
  const ElephantDimensions &d = profile.dims;
  HowdahAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.ground_offset = QVector3D(0.0F, -d.barrel_center_y, 0.0F);

  frame.howdah_center =
      QVector3D(0.0F, d.barrel_center_y + d.body_height * 0.65F,
                -d.body_length * 0.08F);

  frame.seat_position =
      frame.howdah_center + QVector3D(0.0F, d.howdah_height * 0.45F, 0.0F);

  return frame;
}

auto evaluate_elephant_motion(ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample {
  ElephantMotionSample sample{};
  sample.is_moving = anim.is_moving;

  constexpr float kIdleSpeed = 0.3F;
  constexpr float kWalkSpeed = 2.0F;
  
  float const time_scale = sample.is_moving ? 1.0F / profile.gait.cycle_time : kIdleSpeed;
  sample.phase = std::fmod(anim.time * time_scale, 1.0F);

  if (sample.is_moving) {
    sample.bob = std::sin(sample.phase * 2.0F * k_pi) *
                 profile.dims.move_bob_amplitude;
  } else {
    sample.bob =
        std::sin(anim.time * 0.35F) * profile.dims.idle_bob_amplitude;
  }

  // Trunk swaying with sinusoidal movement
  sample.trunk_swing = std::sin(anim.time * 0.8F) * 0.25F +
                       std::sin(anim.time * 1.5F) * 0.12F;

  // Ear flapping animation
  sample.ear_flap = std::sin(anim.time * 1.2F) * 0.35F;

  return sample;
}

void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.howdah_center += offset;
  frame.seat_position += offset;
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
  float const trunk_swing = motion.trunk_swing;
  float const ear_flap = motion.ear_flap;

  HowdahAttachmentFrame howdah =
      shared_howdah ? *shared_howdah : compute_howdah_frame(profile);
  if (!shared_howdah) {
    apply_howdah_vertical_offset(howdah, bob);
  }

  uint32_t elephant_seed = 0U;
  if (ctx.entity != nullptr) {
    elephant_seed = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  float const body_sway =
      is_moving ? std::sin(phase * 2.0F * k_pi) * 0.008F
                : std::sin(anim.time * 0.3F) * 0.003F;

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob, 0.0F);

  // Render massive body
  {
    QMatrix4x4 body = elephant_ctx.model;
    body.translate(barrel_center);
    body.scale(d.body_width, d.body_height, d.body_length);
    QVector3D const body_color = skin_gradient(v.skin_color, 0.6F, 0.0F,
                                               hash01(elephant_seed ^ 0x101U));
    out.mesh(get_unit_sphere(), body, body_color, nullptr, 1.0F, 6);
  }

  // Render back/rump
  {
    QVector3D const rump_center =
        barrel_center +
        QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.42F);
    QMatrix4x4 rump = elephant_ctx.model;
    rump.translate(rump_center);
    rump.scale(d.body_width * 1.05F, d.body_height * 0.95F,
               d.body_length * 0.40F);
    QVector3D const rump_color = skin_gradient(v.skin_color, 0.55F, 0.05F,
                                               hash01(elephant_seed ^ 0x202U));
    out.mesh(get_unit_sphere(), rump, rump_color, nullptr, 1.0F, 6);
  }

  // Render chest
  {
    QVector3D const chest_center =
        barrel_center +
        QVector3D(0.0F, d.body_height * 0.05F, d.body_length * 0.38F);
    QMatrix4x4 chest = elephant_ctx.model;
    chest.translate(chest_center);
    chest.scale(d.body_width * 0.95F, d.body_height * 0.92F,
                d.body_length * 0.35F);
    QVector3D const chest_color = skin_gradient(v.skin_color, 0.58F, 0.08F,
                                                hash01(elephant_seed ^ 0x303U));
    out.mesh(get_unit_sphere(), chest, chest_color, nullptr, 1.0F, 6);
  }

  // Render neck
  QVector3D const neck_base =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.28F, d.body_length * 0.48F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, -d.neck_length * 0.15F, d.neck_length * 0.65F);
  {
    float const neck_radius = d.neck_width * 0.48F;
    draw_cylinder(out, elephant_ctx.model, neck_base, neck_top, neck_radius,
                  skin_gradient(v.skin_color, 0.62F, 0.12F,
                                hash01(elephant_seed ^ 0x404U)),
                  1.0F, 6);
  }

  // Render head
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, -d.head_height * 0.15F, d.head_length * 0.28F);
  {
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 0.95F, d.head_height * 0.90F,
               d.head_length * 0.85F);
    QVector3D const head_color = skin_gradient(v.skin_color, 0.68F, 0.15F,
                                               hash01(elephant_seed ^ 0x505U));
    out.mesh(get_unit_sphere(), head, head_color, nullptr, 1.0F, 6);
  }

  // Render trunk with sinusoidal animation
  {
    QVector3D const trunk_base =
        head_center +
        QVector3D(0.0F, -d.head_height * 0.35F, d.head_length * 0.42F);
    
    int const trunk_segments = 12;
    QVector3D prev_point = trunk_base;
    
    for (int i = 1; i <= trunk_segments; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(trunk_segments);
      
      // Sinusoidal trunk movement
      float const swing = trunk_swing * std::sin(t * k_pi);
      float const lateral_offset = swing * 0.15F;
      
      // Bezier curve for natural trunk droop
      QVector3D const ctrl1 = trunk_base + QVector3D(0.0F, -d.trunk_length * 0.25F,
                                                      d.trunk_length * 0.15F);
      QVector3D const ctrl2 = trunk_base + QVector3D(0.0F, -d.trunk_length * 0.70F,
                                                      d.trunk_length * 0.25F);
      QVector3D const end = trunk_base + QVector3D(0.0F, -d.trunk_length,
                                                     d.trunk_length * 0.18F);
      
      QVector3D point = bezier_cubic(trunk_base, ctrl1, ctrl2, end, t);
      point.setX(point.x() + lateral_offset);
      
      float const radius = lerp(d.trunk_base_radius, d.trunk_tip_radius, t);
      QVector3D const trunk_color = skin_gradient(v.skin_color, 0.50F - t * 0.15F,
                                                   0.20F + t * 0.10F,
                                                   hash01(elephant_seed ^ (0x606U + i)));
      
      draw_cylinder(out, elephant_ctx.model, prev_point, point, radius,
                    trunk_color, 1.0F, 6);
      
      prev_point = point;
    }
    
    // Trunk tip
    {
      QMatrix4x4 trunk_tip = elephant_ctx.model;
      trunk_tip.translate(prev_point);
      trunk_tip.scale(d.trunk_tip_radius * 1.2F);
      out.mesh(get_unit_sphere(), trunk_tip, darken(v.skin_color, 0.85F),
               nullptr, 1.0F, 6);
    }
  }

  // Render large ears with flapping animation
  for (int side = 0; side < 2; ++side) {
    float const side_sign = (side == 0) ? 1.0F : -1.0F;
    float const ear_flap_offset = ear_flap * side_sign * 0.18F;
    
    QVector3D const ear_base =
        head_center + QVector3D(side_sign * d.head_width * 0.48F,
                                d.head_height * 0.32F, -d.head_length * 0.12F);
    
    // Main ear shape - large and flat
    {
      QMatrix4x4 ear = elephant_ctx.model;
      ear.translate(ear_base);
      ear.rotate(ear_flap_offset * 20.0F, QVector3D(0.0F, 0.0F, 1.0F));
      ear.scale(d.ear_thickness, d.ear_height, d.ear_width);
      QVector3D const ear_color = skin_gradient(v.skin_color, 0.72F, 0.08F,
                                                hash01(elephant_seed ^ (0x707U + side)));
      out.mesh(get_unit_sphere(), ear, ear_color, nullptr, 1.0F, 6);
    }
    
    // Inner ear
    {
      QMatrix4x4 ear_inner = elephant_ctx.model;
      ear_inner.translate(ear_base + QVector3D(side_sign * d.ear_thickness * 0.8F,
                                                 0.0F, 0.0F));
      ear_inner.rotate(ear_flap_offset * 20.0F, QVector3D(0.0F, 0.0F, 1.0F));
      ear_inner.scale(d.ear_thickness * 0.4F, d.ear_height * 0.85F,
                      d.ear_width * 0.88F);
      out.mesh(get_unit_sphere(), ear_inner, v.ear_inner_color, nullptr, 1.0F, 6);
    }
  }

  // Render eyes
  {
    float const eye_y = d.head_height * 0.22F;
    float const eye_z = d.head_length * 0.08F;
    
    for (int side = 0; side < 2; ++side) {
      float const side_sign = (side == 0) ? 1.0F : -1.0F;
      QVector3D const eye_pos =
          head_center + QVector3D(side_sign * d.head_width * 0.42F, eye_y, eye_z);
      
      QMatrix4x4 eye = elephant_ctx.model;
      eye.translate(eye_pos);
      eye.scale(d.head_width * 0.08F);
      out.mesh(get_unit_sphere(), eye, QVector3D(0.12F, 0.10F, 0.08F), nullptr,
               1.0F, 6);
    }
  }

  // Render tusks
  for (int side = 0; side < 2; ++side) {
    float const side_sign = (side == 0) ? 1.0F : -1.0F;
    
    QVector3D const tusk_base =
        head_center + QVector3D(side_sign * d.head_width * 0.28F,
                                -d.head_height * 0.32F, d.head_length * 0.35F);
    QVector3D const tusk_tip =
        tusk_base + QVector3D(side_sign * d.tusk_length * 0.15F,
                              -d.tusk_length * 0.22F, d.tusk_length * 0.85F);
    
    draw_cone(out, elephant_ctx.model, tusk_tip, tusk_base, d.tusk_radius,
              v.tusk_color, 1.0F, 9);
  }

  // Render pillar-like legs with proper weight shifting
  auto render_leg = [&](const QVector3D &anchor, float lateral_sign,
                        float forward_bias, float phase_offset, bool is_rear) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.stride_swing * 0.5F + forward_bias;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.stride_lift : 0.0F;
    } else {
      // Weight shifting when idle
      float const weight_time = anim.time * 0.12F + lateral_sign * k_pi * 0.25F;
      stride = std::sin(weight_time) * 0.02F + forward_bias;
    }

    QVector3D const shoulder =
        anchor + QVector3D(lateral_sign * d.body_width * 0.45F, lift * 0.08F,
                           stride);

    // Pillar-like leg - mostly straight down
    QVector3D const foot = shoulder + QVector3D(0.0F, -d.leg_length + lift, 0.0F);

    // Render leg cylinder
    QVector3D const leg_color = skin_gradient(v.skin_color, 0.45F - lift * 0.15F,
                                              0.18F, hash01(elephant_seed ^ 0x808U));
    draw_cylinder(out, elephant_ctx.model, shoulder, foot, d.leg_radius,
                  leg_color, 1.0F, 6);

    // Render knee joint
    {
      QVector3D const knee = lerp(shoulder, foot, 0.48F);
      QMatrix4x4 knee_joint = elephant_ctx.model;
      knee_joint.translate(knee);
      knee_joint.scale(d.leg_radius * 1.15F);
      out.mesh(get_unit_sphere(), knee_joint, darken(leg_color, 0.92F), nullptr,
               1.0F, 6);
    }

    // Render foot with toenails
    {
      QMatrix4x4 foot_cylinder = elephant_ctx.model;
      foot_cylinder.translate(foot);
      foot_cylinder.scale(d.foot_radius, d.leg_length * 0.08F, d.foot_radius);
      out.mesh(get_unit_cylinder(), foot_cylinder, darken(v.skin_color, 0.88F),
               nullptr, 1.0F, 8);

      // Toenails - 3 per foot
      for (int toe = 0; toe < 3; ++toe) {
        float const toe_angle = (toe - 1) * 0.8F;
        QVector3D const toe_offset(
            std::sin(toe_angle) * d.foot_radius * 0.75F, -d.leg_length * 0.06F,
            std::cos(toe_angle) * d.foot_radius * 0.75F);
        QVector3D const toenail_pos = foot + toe_offset;

        QMatrix4x4 toenail = elephant_ctx.model;
        toenail.translate(toenail_pos);
        toenail.scale(d.foot_radius * 0.18F, d.leg_length * 0.03F,
                      d.foot_radius * 0.22F);
        out.mesh(get_unit_sphere(), toenail, v.toenail_color, nullptr, 1.0F, 8);
      }
    }
  };

  // Front legs
  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.35F);
  render_leg(front_anchor, 1.0F, d.body_length * 0.08F, g.front_leg_phase, false);
  render_leg(front_anchor, -1.0F, d.body_length * 0.08F,
             g.front_leg_phase + 0.50F, false);

  // Rear legs
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.32F);
  render_leg(rear_anchor, 1.0F, -d.body_length * 0.08F, g.rear_leg_phase, true);
  render_leg(rear_anchor, -1.0F, -d.body_length * 0.08F,
             g.rear_leg_phase + 0.50F, true);

  // Render tail with swaying
  {
    QVector3D const tail_base =
        barrel_center +
        QVector3D(0.0F, d.body_height * 0.42F, -d.body_length * 0.48F);
    
    float const tail_sway = std::sin(anim.time * 1.1F) * 0.28F +
                            std::sin(anim.time * 2.3F) * 0.12F;
    
    QVector3D const tail_mid =
        tail_base + QVector3D(tail_sway * 0.15F, -d.tail_length * 0.42F, 0.0F);
    QVector3D const tail_end =
        tail_mid + QVector3D(tail_sway * 0.25F, -d.tail_length * 0.48F,
                              -d.tail_length * 0.08F);
    
    draw_cylinder(out, elephant_ctx.model, tail_base, tail_mid,
                  d.body_width * 0.08F, darken(v.skin_color, 0.90F), 1.0F, 6);
    draw_cylinder(out, elephant_ctx.model, tail_mid, tail_end,
                  d.body_width * 0.05F, darken(v.skin_color, 0.88F), 1.0F, 6);
    
    // Tail tuft
    {
      QMatrix4x4 tuft = elephant_ctx.model;
      tuft.translate(tail_end);
      tuft.scale(d.body_width * 0.12F, d.tail_length * 0.15F,
                 d.body_width * 0.12F);
      out.mesh(get_unit_sphere(), tuft, darken(v.skin_color, 0.75F), nullptr,
               0.85F, 7);
    }
  }

  // Render howdah (rider platform)
  {
    QVector3D const howdah_pos = howdah.howdah_center;
    
    // Howdah base/platform
    {
      QMatrix4x4 platform = elephant_ctx.model;
      platform.translate(howdah_pos);
      platform.scale(d.howdah_width * 0.48F, d.howdah_height * 0.15F,
                     d.howdah_length * 0.48F);
      out.mesh(get_unit_cylinder(), platform, v.howdah_wood_color, nullptr,
               1.0F, 10);
    }
    
    // Howdah walls
    for (int wall = 0; wall < 4; ++wall) {
      float const angle = wall * k_pi * 0.5F;
      QVector3D const offset(std::sin(angle) * d.howdah_width * 0.42F, 0.0F,
                             std::cos(angle) * d.howdah_length * 0.42F);
      
      QMatrix4x4 wall_piece = elephant_ctx.model;
      wall_piece.translate(howdah_pos + offset +
                           QVector3D(0.0F, d.howdah_height * 0.28F, 0.0F));
      if (wall % 2 == 0) {
        wall_piece.scale(d.howdah_width * 0.05F, d.howdah_height * 0.35F,
                         d.howdah_length * 0.48F);
      } else {
        wall_piece.scale(d.howdah_width * 0.48F, d.howdah_height * 0.35F,
                         d.howdah_width * 0.05F);
      }
      out.mesh(get_unit_cylinder(), wall_piece, v.howdah_wood_color, nullptr,
               1.0F, 10);
    }
    
    // Howdah fabric/canopy
    {
      QMatrix4x4 canopy = elephant_ctx.model;
      canopy.translate(howdah_pos + QVector3D(0.0F, d.howdah_height * 0.58F, 0.0F));
      canopy.scale(d.howdah_width * 0.52F, d.howdah_height * 0.08F,
                   d.howdah_length * 0.52F);
      out.mesh(get_unit_cylinder(), canopy, v.howdah_fabric_color, nullptr,
               1.0F, 10);
    }
    
    // Decorative metal corner pieces
    for (int corner = 0; corner < 4; ++corner) {
      float const angle = corner * k_pi * 0.5F + k_pi * 0.25F;
      QVector3D const corner_offset(
          std::sin(angle) * d.howdah_width * 0.48F, d.howdah_height * 0.12F,
          std::cos(angle) * d.howdah_length * 0.48F);
      
      QMatrix4x4 metal_piece = elephant_ctx.model;
      metal_piece.translate(howdah_pos + corner_offset);
      metal_piece.scale(d.howdah_width * 0.06F, d.howdah_height * 0.42F,
                        d.howdah_width * 0.06F);
      out.mesh(get_unit_cylinder(), metal_piece, v.howdah_metal_color, nullptr,
               1.0F, 11);
    }
  }

  // Setup body frames for attachments
  ElephantBodyFrames body_frames;
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

  body_frames.back_center.origin = barrel_center;
  body_frames.back_center.right = right;
  body_frames.back_center.up = up;
  body_frames.back_center.forward = forward;

  body_frames.howdah.origin = howdah.howdah_center;
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

  // Simplified body
  {
    QMatrix4x4 body = elephant_ctx.model;
    body.translate(barrel_center);
    body.scale(d.body_width * 0.95F, d.body_height * 0.90F,
               d.body_length * 0.95F);
    out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, 6);
  }

  // Simplified head
  QVector3D const head_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.18F, d.body_length * 0.55F);
  {
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 0.85F, d.head_height * 0.80F,
               d.head_length * 0.75F);
    out.mesh(get_unit_sphere(), head, v.skin_color, nullptr, 1.0F, 6);
  }

  // Simplified trunk
  {
    QVector3D const trunk_base =
        head_center + QVector3D(0.0F, -d.head_height * 0.30F,
                                d.head_length * 0.35F);
    QVector3D const trunk_end =
        trunk_base + QVector3D(0.0F, -d.trunk_length * 0.85F,
                                d.trunk_length * 0.15F);
    draw_cylinder(out, elephant_ctx.model, trunk_base, trunk_end,
                  d.trunk_base_radius * 0.75F, darken(v.skin_color, 0.95F),
                  1.0F, 6);
  }

  // Simplified ears
  for (int side = 0; side < 2; ++side) {
    float const side_sign = (side == 0) ? 1.0F : -1.0F;
    QVector3D const ear_pos =
        head_center + QVector3D(side_sign * d.head_width * 0.45F,
                                d.head_height * 0.28F, -d.head_length * 0.10F);
    QMatrix4x4 ear = elephant_ctx.model;
    ear.translate(ear_pos);
    ear.scale(d.ear_thickness * 0.6F, d.ear_height * 0.75F, d.ear_width * 0.75F);
    out.mesh(get_unit_sphere(), ear, lighten(v.skin_color, 1.05F), nullptr,
             1.0F, 6);
  }

  // Simplified legs
  auto draw_simple_leg = [&](const QVector3D &anchor, float lateral_sign,
                             float forward_bias, float phase_offset) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * g.stride_swing * 0.4F + forward_bias;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * 0.6F : 0.0F;
    }

    QVector3D const shoulder =
        anchor + QVector3D(lateral_sign * d.body_width * 0.42F, lift * 0.05F,
                           stride);
    QVector3D const foot = shoulder + QVector3D(0.0F, -d.leg_length + lift, 0.0F);

    draw_cylinder(out, elephant_ctx.model, shoulder, foot, d.leg_radius * 0.85F,
                  darken(v.skin_color, 0.90F), 1.0F, 6);

    QMatrix4x4 foot_mesh = elephant_ctx.model;
    foot_mesh.translate(foot);
    foot_mesh.scale(d.foot_radius * 0.90F, d.leg_length * 0.06F,
                    d.foot_radius * 0.90F);
    out.mesh(get_unit_cylinder(), foot_mesh, darken(v.skin_color, 0.85F),
             nullptr, 1.0F, 8);
  };

  QVector3D const front_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.10F, d.body_length * 0.32F);
  QVector3D const rear_anchor =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.30F);

  draw_simple_leg(front_anchor, 1.0F, d.body_length * 0.08F, g.front_leg_phase);
  draw_simple_leg(front_anchor, -1.0F, d.body_length * 0.08F,
                  g.front_leg_phase + 0.48F);
  draw_simple_leg(rear_anchor, 1.0F, -d.body_length * 0.08F, g.rear_leg_phase);
  draw_simple_leg(rear_anchor, -1.0F, -d.body_length * 0.08F,
                  g.rear_leg_phase + 0.52F);
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

  // Single large sphere for body
  {
    QMatrix4x4 body = elephant_ctx.model;
    body.translate(center);
    body.scale(d.body_width * 1.1F, d.body_height + d.head_height * 0.35F,
               d.body_length + d.head_length * 0.40F);
    out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, 6);
  }

  // Minimal legs - just cylinders
  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.body_length * 0.28F : -d.body_length * 0.28F;

    QVector3D const top = center + QVector3D(x_sign * d.body_width * 0.38F,
                                             -d.body_height * 0.25F, z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.leg_length * 0.65F, 0.0F);

    draw_cylinder(out, elephant_ctx.model, top, bottom, d.leg_radius * 0.70F,
                  darken(v.skin_color, 0.85F), 1.0F, 6);
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