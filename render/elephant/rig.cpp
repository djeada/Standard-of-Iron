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

// Issue #8: Material IDs for varying surface roughness (wrinkle normals)
// Higher material IDs = rougher surfaces with more visible wrinkles
constexpr int kMaterialElephantBody = 6;         // Base elephant skin
constexpr int kMaterialElephantTrunk = 7;        // Rougher trunk with wrinkles
constexpr int kMaterialElephantLegs = 8;         // Very rough legs
constexpr int kMaterialElephantEars = 9;         // Thin membrane, less rough
constexpr int kMaterialElephantTusk = 10;        // Smooth ivory

} // namespace

namespace ElephantDimensionRange {

// Issue #: Flatten and lengthen the torso
constexpr float kBodyLengthMin = 2.60F;
constexpr float kBodyLengthMax = 3.00F;
constexpr float kBodyWidthMin = 0.85F;
constexpr float kBodyWidthMax = 1.00F;
constexpr float kBodyHeightMin = 1.00F;
constexpr float kBodyHeightMax = 1.25F;

// Issue #: Carve a neck transition
constexpr float kNeckLengthMin = 0.35F;
constexpr float kNeckLengthMax = 0.50F;
constexpr float kNeckWidthMin = 0.35F;
constexpr float kNeckWidthMax = 0.45F;

constexpr float kHeadLengthMin = 0.50F;
constexpr float kHeadLengthMax = 0.65F;
constexpr float kHeadWidthMin = 0.55F;
constexpr float kHeadWidthMax = 0.70F;
constexpr float kHeadHeightMin = 0.50F;
constexpr float kHeadHeightMax = 0.65F;

// Issue #: Fix trunk proportions - thicker base, more taper
constexpr float kTrunkLengthMin = 1.50F;
constexpr float kTrunkLengthMax = 1.90F;
constexpr float kTrunkBaseRadiusMin = 0.18F;
constexpr float kTrunkBaseRadiusMax = 0.25F;
constexpr float kTrunkTipRadiusMin = 0.03F;
constexpr float kTrunkTipRadiusMax = 0.05F;

// Issue #: Make ears bigger and thinner
constexpr float kEarWidthMin = 0.75F;
constexpr float kEarWidthMax = 1.00F;
constexpr float kEarHeightMin = 0.80F;
constexpr float kEarHeightMax = 1.05F;
constexpr float kEarThicknessMin = 0.02F;
constexpr float kEarThicknessMax = 0.035F;

// Issue #: Rebuild legs with joints - add knee/ankle dimensions
constexpr float kLegLengthMin = 1.20F;
constexpr float kLegLengthMax = 1.50F;
constexpr float kLegRadiusMin = 0.16F;
constexpr float kLegRadiusMax = 0.22F;
// Issue #: Replace cuff-feet with real feet - wider foot base
constexpr float kFootRadiusMin = 0.26F;
constexpr float kFootRadiusMax = 0.34F;

constexpr float kTailLengthMin = 0.60F;
constexpr float kTailLengthMax = 0.85F;

constexpr float kTuskLengthMin = 0.50F;
constexpr float kTuskLengthMax = 0.80F;
constexpr float kTuskRadiusMin = 0.04F;
constexpr float kTuskRadiusMax = 0.07F;

constexpr float kHowdahWidthMin = 0.75F;
constexpr float kHowdahWidthMax = 0.95F;
constexpr float kHowdahLengthMin = 0.90F;
constexpr float kHowdahLengthMax = 1.20F;
constexpr float kHowdahHeightMin = 0.45F;
constexpr float kHowdahHeightMax = 0.60F;

constexpr float kIdleBobAmplitudeMin = 0.008F;
constexpr float kIdleBobAmplitudeMax = 0.015F;
constexpr float kMoveBobAmplitudeMin = 0.035F;
constexpr float kMoveBobAmplitudeMax = 0.055F;

constexpr float kBarrelHeightScale = 0.55F;

constexpr uint32_t kSaltBodyLength = 0x12U;
constexpr uint32_t kSaltBodyWidth = 0x34U;
constexpr uint32_t kSaltBodyHeight = 0x56U;
constexpr uint32_t kSaltNeckLength = 0x9AU;
constexpr uint32_t kSaltNeckWidth = 0xBCU;
constexpr uint32_t kSaltHeadLength = 0xDEU;
constexpr uint32_t kSaltHeadWidth = 0xF1U;
constexpr uint32_t kSaltHeadHeight = 0x1357U;
constexpr uint32_t kSaltTrunkLength = 0x2468U;
constexpr uint32_t kSaltTrunkBaseRadius = 0x369CU;
constexpr uint32_t kSaltTrunkTipRadius = 0x48AEU;
constexpr uint32_t kSaltEarWidth = 0x5ABCU;
constexpr uint32_t kSaltEarHeight = 0x6CDEU;
constexpr uint32_t kSaltEarThickness = 0x7531U;
constexpr uint32_t kSaltLegLength = 0x8642U;
constexpr uint32_t kSaltLegRadius = 0x9753U;
constexpr uint32_t kSaltFootRadius = 0xA864U;
constexpr uint32_t kSaltTailLength = 0xB975U;
constexpr uint32_t kSaltTuskLength = 0xCA86U;
constexpr uint32_t kSaltTuskRadius = 0xDB97U;
constexpr uint32_t kSaltHowdahWidth = 0xECA8U;
constexpr uint32_t kSaltHowdahLength = 0xFDB9U;
constexpr uint32_t kSaltHowdahHeight = 0x0ECAU;
constexpr uint32_t kSaltIdleBob = 0x1FDBU;
constexpr uint32_t kSaltMoveBob = 0x20ECU;

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

  d.ear_width =
      rand_between(seed, kSaltEarWidth, kEarWidthMin, kEarWidthMax);
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
  d.howdah_length =
      rand_between(seed, kSaltHowdahLength, kHowdahLengthMin, kHowdahLengthMax);
  d.howdah_height =
      rand_between(seed, kSaltHowdahHeight, kHowdahHeightMin, kHowdahHeightMax);

  d.idle_bob_amplitude = rand_between(seed, kSaltIdleBob, kIdleBobAmplitudeMin,
                                      kIdleBobAmplitudeMax);
  d.move_bob_amplitude = rand_between(seed, kSaltMoveBob, kMoveBobAmplitudeMin,
                                      kMoveBobAmplitudeMax);

  d.barrel_center_y = d.leg_length + d.body_height * kBarrelHeightScale;

  return d;
}

namespace ElephantVariantConstants {

constexpr float kSkinBaseR = 0.45F;
constexpr float kSkinBaseG = 0.42F;
constexpr float kSkinBaseB = 0.40F;

constexpr float kSkinHighlightR = 0.55F;
constexpr float kSkinHighlightG = 0.52F;
constexpr float kSkinHighlightB = 0.48F;

constexpr float kSkinShadowR = 0.35F;
constexpr float kSkinShadowG = 0.32F;
constexpr float kSkinShadowB = 0.30F;

constexpr float kEarInnerR = 0.65F;
constexpr float kEarInnerG = 0.52F;
constexpr float kEarInnerB = 0.48F;

constexpr float kTuskR = 0.95F;
constexpr float kTuskG = 0.92F;
constexpr float kTuskB = 0.85F;

constexpr float kToenailR = 0.70F;
constexpr float kToenailG = 0.65F;
constexpr float kToenailB = 0.58F;

constexpr float kWoodR = 0.45F;
constexpr float kWoodG = 0.32F;
constexpr float kWoodB = 0.20F;

constexpr float kSkinVarianceMin = 0.90F;
constexpr float kSkinVarianceMax = 1.10F;

constexpr uint32_t kSaltSkinVariance = 0x3412U;
constexpr uint32_t kSaltFabricTint = 0x5634U;
constexpr uint32_t kSaltMetalTint = 0x7856U;

} // namespace ElephantVariantConstants

auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant {
  using namespace ElephantVariantConstants;
  ElephantVariant v;

  float const skin_variance =
      rand_between(seed, kSaltSkinVariance, kSkinVarianceMin, kSkinVarianceMax);

  v.skin_color = QVector3D(kSkinBaseR * skin_variance,
                           kSkinBaseG * skin_variance,
                           kSkinBaseB * skin_variance);
  v.skin_highlight = QVector3D(kSkinHighlightR * skin_variance,
                               kSkinHighlightG * skin_variance,
                               kSkinHighlightB * skin_variance);
  v.skin_shadow = QVector3D(kSkinShadowR * skin_variance,
                            kSkinShadowG * skin_variance,
                            kSkinShadowB * skin_variance);

  v.ear_inner_color = QVector3D(kEarInnerR, kEarInnerG, kEarInnerB);
  v.tusk_color = QVector3D(kTuskR, kTuskG, kTuskB);
  v.toenail_color = QVector3D(kToenailR, kToenailG, kToenailB);

  v.howdah_wood_color = QVector3D(kWoodR, kWoodG, kWoodB);
  v.howdah_fabric_color = fabric_base;
  v.howdah_metal_color = metal_base;

  return v;
}

namespace ElephantGaitConstants {

constexpr float kCycleTimeMin = 1.20F;
constexpr float kCycleTimeMax = 1.50F;
constexpr float kFrontLegPhaseMin = 0.10F;
constexpr float kFrontLegPhaseMax = 0.18F;
constexpr float kDiagonalLeadMin = 0.45F;
constexpr float kDiagonalLeadMax = 0.55F;
constexpr float kStrideSwingMin = 0.18F;
constexpr float kStrideSwingMax = 0.25F;
constexpr float kStrideLiftMin = 0.08F;
constexpr float kStrideLiftMax = 0.12F;

constexpr uint32_t kSaltCycleTime = 0xAA12U;
constexpr uint32_t kSaltFrontLegPhase = 0xBB34U;
constexpr uint32_t kSaltDiagonalLead = 0xCC56U;
constexpr uint32_t kSaltStrideSwing = 0xDD78U;
constexpr uint32_t kSaltStrideLift = 0xEE9AU;

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
      QVector3D(0.0F, d.barrel_center_y + d.body_height * 0.55F,
                -d.body_length * 0.15F);

  frame.seat_position =
      frame.howdah_center + QVector3D(0.0F, d.howdah_height * 0.5F, 0.0F);

  return frame;
}

auto evaluate_elephant_motion(ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample {
  ElephantMotionSample sample{};
  const ElephantDimensions &d = profile.dims;

  sample.is_moving = anim.is_moving;

  if (sample.is_moving) {
    float const walk_cycle = anim.time / profile.gait.cycle_time;
    sample.phase = std::fmod(walk_cycle, 1.0F);
    sample.bob = std::sin(sample.phase * 2.0F * k_pi) * d.move_bob_amplitude;
  } else {
    sample.phase = 0.0F;
    sample.bob = std::sin(anim.time * 0.5F) * d.idle_bob_amplitude;
  }

  sample.trunk_swing = std::sin(anim.time * 1.2F) * 0.15F;
  sample.ear_flap = std::sin(anim.time * 2.5F) * 0.08F;

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

  DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  QVector3D const barrel_center(0.0F, d.barrel_center_y + bob, 0.0F);

  // Issue #1: Flatten and lengthen the torso with shoulder hump
  // Issue #8: Using material IDs for surface roughness variation
  // Main body - flatter, longer torso
  {
    QMatrix4x4 body = elephant_ctx.model;
    body.translate(barrel_center);
    body.scale(d.body_width, d.body_height * 0.85F, d.body_length * 0.55F);
    out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Shoulder hump - raised area at shoulders
  {
    QVector3D const hump_pos =
        barrel_center + QVector3D(0.0F, d.body_height * 0.25F,
                                  d.body_length * 0.20F);
    QMatrix4x4 hump = elephant_ctx.model;
    hump.translate(hump_pos);
    hump.scale(d.body_width * 0.85F, d.body_height * 0.35F,
               d.body_length * 0.25F);
    out.mesh(get_unit_sphere(), hump, v.skin_highlight, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Rump - slopes down from shoulders toward hips
  {
    QVector3D const rump_pos =
        barrel_center + QVector3D(0.0F, -d.body_height * 0.08F, -d.body_length * 0.35F);
    QMatrix4x4 rump = elephant_ctx.model;
    rump.translate(rump_pos);
    rump.scale(d.body_width * 0.90F, d.body_height * 0.78F,
               d.body_length * 0.32F);
    out.mesh(get_unit_sphere(), rump, v.skin_shadow, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Shoulders/chest
  {
    QVector3D const chest_pos =
        barrel_center + QVector3D(0.0F, d.body_height * 0.15F,
                                  d.body_length * 0.30F);
    QMatrix4x4 chest = elephant_ctx.model;
    chest.translate(chest_pos);
    chest.scale(d.body_width * 0.90F, d.body_height * 0.85F,
                d.body_length * 0.25F);
    out.mesh(get_unit_sphere(), chest, v.skin_highlight, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Issue #2: Carve a neck transition - narrowing behind the head
  // Neck base (thicker, connects to shoulders)
  QVector3D const neck_base =
      barrel_center + QVector3D(0.0F, d.body_height * 0.40F,
                                d.body_length * 0.40F);
  // Neck mid (narrower transition zone)
  QVector3D const neck_mid =
      neck_base + QVector3D(0.0F, d.neck_length * 0.35F, d.neck_length * 0.40F);
  QVector3D const neck_top =
      neck_mid + QVector3D(0.0F, d.neck_length * 0.25F, d.neck_length * 0.25F);

  // Draw neck with taper: base thicker, narrowing toward head
  draw_cylinder(out, elephant_ctx.model, neck_base, neck_mid, d.neck_width * 1.1F,
                v.skin_color, 1.0F, kMaterialElephantBody);
  draw_cylinder(out, elephant_ctx.model, neck_mid, neck_top, d.neck_width * 0.85F,
                v.skin_color, 1.0F, kMaterialElephantBody);

  // Issue #5: Shape skull planes - brow ridge, cheek bulge, forehead-to-trunk break
  // Head
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * 0.35F, d.head_length * 0.25F);
  {
    // Main skull
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width, d.head_height * 0.90F, d.head_length * 0.95F);
    out.mesh(get_unit_sphere(), head, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Brow ridge - pronounced ridge above eyes
  {
    QVector3D const brow_pos =
        head_center + QVector3D(0.0F, d.head_height * 0.35F, d.head_length * 0.15F);
    QMatrix4x4 brow = elephant_ctx.model;
    brow.translate(brow_pos);
    brow.scale(d.head_width * 0.95F, d.head_height * 0.18F, d.head_length * 0.25F);
    out.mesh(get_unit_sphere(), brow, v.skin_highlight, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Cheek bulges - sides of face
  for (int side = -1; side <= 1; side += 2) {
    float const side_f = static_cast<float>(side);
    QVector3D const cheek_pos =
        head_center + QVector3D(side_f * d.head_width * 0.55F,
                                -d.head_height * 0.05F, d.head_length * 0.20F);
    QMatrix4x4 cheek = elephant_ctx.model;
    cheek.translate(cheek_pos);
    cheek.scale(d.head_width * 0.30F, d.head_height * 0.40F, d.head_length * 0.30F);
    out.mesh(get_unit_sphere(), cheek, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Forehead-to-trunk break - depression between forehead and trunk base
  {
    QVector3D const break_pos =
        head_center + QVector3D(0.0F, d.head_height * 0.10F, d.head_length * 0.50F);
    QMatrix4x4 forehead_break = elephant_ctx.model;
    forehead_break.translate(break_pos);
    forehead_break.scale(d.head_width * 0.45F, d.head_height * 0.22F, d.head_length * 0.20F);
    out.mesh(get_unit_sphere(), forehead_break, v.skin_shadow, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Issue #7: Fix trunk proportions and flow - thicker base, natural downward curve
  // Issue #8: Trunk uses rougher material for wrinkle effect
  // Trunk with muscular base and gradual taper
  {
    QVector3D const trunk_base =
        head_center + QVector3D(0.0F, -d.head_height * 0.25F,
                                d.head_length * 0.65F);
    // Natural downward curve - trunk hangs more vertically with slight forward reach
    QVector3D trunk_tip = trunk_base + QVector3D(trunk_swing * 0.25F,
                                                  -d.trunk_length * 0.85F,
                                                  d.trunk_length * 0.25F);

    constexpr int k_trunk_segments = 10;
    QVector3D prev = trunk_base;
    for (int i = 1; i <= k_trunk_segments; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_trunk_segments);
      // Non-linear taper: thick at base, gradual narrow
      float const taper = 1.0F - std::pow(t, 0.7F);
      float const radius =
          d.trunk_tip_radius + (d.trunk_base_radius - d.trunk_tip_radius) * taper;
      // Natural S-curve: forward at base, down in middle, slight curl at tip
      float const curve_x = std::sin(t * k_pi * 1.5F + trunk_swing * 2.5F) * 0.08F;
      float const curve_z = std::sin(t * k_pi * 0.8F) * d.trunk_length * 0.05F;
      QVector3D const curve_offset = QVector3D(curve_x, 0.0F, curve_z);
      QVector3D const next = lerp3(trunk_base, trunk_tip, t) + curve_offset;
      draw_cylinder(out, elephant_ctx.model, prev, next, radius, v.skin_color,
                    1.0F, kMaterialElephantTrunk);
      prev = next;
    }
  }

  // Issue #6: Make ears bigger and thinner - like flexible membranes with thin rim
  // Issue #8: Ears use thinner, less rough material
  for (int side = -1; side <= 1; side += 2) {
    float const side_f = static_cast<float>(side);
    float const flap_angle = ear_flap * side_f;

    QVector3D const ear_base =
        head_center + QVector3D(side_f * d.head_width * 0.85F,
                                d.head_height * 0.15F, -d.head_length * 0.18F);
    QVector3D ear_outer =
        ear_base + QVector3D(side_f * d.ear_width * (0.95F + flap_angle),
                             d.ear_height * 0.35F, -d.ear_width * 0.1F);

    // Main ear membrane - larger and thinner
    QMatrix4x4 ear = elephant_ctx.model;
    ear.translate((ear_base + ear_outer) * 0.5F);
    ear.scale(d.ear_width * 0.60F, d.ear_height * 0.58F, d.ear_thickness * 0.7F);
    out.mesh(get_unit_sphere(), ear, v.skin_shadow, nullptr, 1.0F, kMaterialElephantEars);

    // Thin outer rim of ear
    QVector3D const rim_center = (ear_base + ear_outer) * 0.5F +
        QVector3D(side_f * d.ear_width * 0.30F, d.ear_height * 0.05F, 0.0F);
    QMatrix4x4 rim = elephant_ctx.model;
    rim.translate(rim_center);
    rim.scale(d.ear_width * 0.15F, d.ear_height * 0.55F, d.ear_thickness * 0.4F);
    out.mesh(get_unit_sphere(), rim, v.skin_color, nullptr, 1.0F, kMaterialElephantEars);

    // Gentle fold in ear
    QVector3D const fold_pos = (ear_base + ear_outer) * 0.5F +
        QVector3D(side_f * d.ear_width * -0.10F, d.ear_height * 0.08F, d.ear_thickness * 0.3F);
    QMatrix4x4 fold = elephant_ctx.model;
    fold.translate(fold_pos);
    fold.scale(d.ear_width * 0.25F, d.ear_height * 0.20F, d.ear_thickness * 0.3F);
    out.mesh(get_unit_sphere(), fold, v.skin_highlight, nullptr, 1.0F, kMaterialElephantEars);

    // Inner ear coloring - larger
    QMatrix4x4 ear_inner = elephant_ctx.model;
    ear_inner.translate((ear_base + ear_outer) * 0.5F +
                        QVector3D(side_f * d.ear_thickness * 0.3F, 0.0F,
                                  d.ear_thickness * 0.15F));
    ear_inner.scale(d.ear_width * 0.42F, d.ear_height * 0.40F,
                    d.ear_thickness * 0.35F);
    out.mesh(get_unit_sphere(), ear_inner, v.ear_inner_color, nullptr, 1.0F, kMaterialElephantEars);
  }

  // Tusks - smooth ivory material
  for (int side = -1; side <= 1; side += 2) {
    float const side_f = static_cast<float>(side);
    QVector3D const tusk_base =
        head_center + QVector3D(side_f * d.head_width * 0.35F,
                                -d.head_height * 0.35F, d.head_length * 0.5F);
    QVector3D const tusk_tip =
        tusk_base + QVector3D(side_f * d.tusk_length * 0.2F,
                              -d.tusk_length * 0.3F, d.tusk_length * 0.7F);
    draw_cone(out, elephant_ctx.model, tusk_tip, tusk_base, d.tusk_radius,
              v.tusk_color, 1.0F, kMaterialElephantTusk);
  }

  // Eyes
  for (int side = -1; side <= 1; side += 2) {
    float const side_f = static_cast<float>(side);
    QVector3D const eye_pos =
        head_center + QVector3D(side_f * d.head_width * 0.45F,
                                d.head_height * 0.15F, d.head_length * 0.3F);
    QMatrix4x4 eye = elephant_ctx.model;
    eye.translate(eye_pos);
    eye.scale(d.head_width * 0.08F);
    out.mesh(get_unit_sphere(), eye, QVector3D(0.1F, 0.08F, 0.06F), nullptr,
             1.0F, kMaterialElephantBody);
  }

  // Issue #3, #4, #10: Rebuild legs with joints, real feet, and realistic movement
  // Issue #8: Legs use rougher material for stronger wrinkle effect
  auto draw_leg = [&](float x_offset, float z_offset, float phase_offset, bool is_front) {
    float const leg_phase = std::fmod(phase + phase_offset, 1.0F);
    float stride = 0.0F;
    float lift = 0.0F;
    float knee_bend = 0.0F;
    float ankle_bend = 0.0F;

    if (is_moving) {
      float const angle = leg_phase * 2.0F * k_pi;
      stride = std::sin(angle) * profile.gait.stride_swing;
      float const lift_raw = std::sin(angle);
      lift = lift_raw > 0.0F ? lift_raw * profile.gait.stride_lift : 0.0F;

      // Issue #10: Realistic leg movement with joint articulation
      knee_bend = lift_raw > 0.0F ? lift_raw * 0.12F : -0.03F;
      ankle_bend = lift_raw > 0.0F ? -lift_raw * 0.08F : 0.02F;
    }

    float const upper_leg_length = d.leg_length * 0.45F;
    float const lower_leg_length = d.leg_length * 0.40F;
    float const ankle_length = d.leg_length * 0.15F;

    // Shoulder/hip joint
    QVector3D const shoulder =
        barrel_center + QVector3D(x_offset, -d.body_height * 0.38F,
                                  z_offset + stride * 0.5F);

    // Issue #3: Knee with forward bulge (S-like silhouette)
    QVector3D const knee = shoulder + QVector3D(
        stride * 0.3F,
        -upper_leg_length + lift * 0.4F,
        (is_front ? 0.08F : -0.06F) + knee_bend);
    // Knee bulge
    QMatrix4x4 knee_bulge = elephant_ctx.model;
    knee_bulge.translate(knee);
    knee_bulge.scale(d.leg_radius * 1.15F, d.leg_radius * 0.9F, d.leg_radius * 1.1F);
    out.mesh(get_unit_sphere(), knee_bulge, v.skin_color, nullptr, 1.0F, kMaterialElephantLegs);

    // Issue #3: Ankle with backward mass (heel)
    QVector3D const ankle = knee + QVector3D(
        stride * 0.4F,
        -lower_leg_length + lift * 0.5F,
        (is_front ? -0.05F : 0.04F) + ankle_bend);
    // Ankle/heel bulge
    QMatrix4x4 ankle_bulge = elephant_ctx.model;
    ankle_bulge.translate(ankle + QVector3D(0.0F, 0.0F, is_front ? -0.03F : 0.04F));
    ankle_bulge.scale(d.leg_radius * 0.95F, d.leg_radius * 0.75F, d.leg_radius * 1.0F);
    out.mesh(get_unit_sphere(), ankle_bulge, v.skin_shadow, nullptr, 1.0F, kMaterialElephantLegs);

    // Foot position
    QVector3D const foot = ankle + QVector3D(stride * 0.2F, -ankle_length + lift * 0.1F, 0.0F);

    // Draw leg segments with varying radii
    draw_cylinder(out, elephant_ctx.model, shoulder, knee, d.leg_radius * 1.05F,
                  v.skin_color, 1.0F, kMaterialElephantLegs);
    draw_cylinder(out, elephant_ctx.model, knee, ankle, d.leg_radius * 0.92F,
                  v.skin_color, 1.0F, kMaterialElephantLegs);
    draw_cylinder(out, elephant_ctx.model, ankle, foot, d.leg_radius * 0.85F,
                  v.skin_shadow, 1.0F, kMaterialElephantLegs);

    // Issue #4: Replace cuff-feet with real feet - wide base with thick pad
    // Foot pad - wider and flatter
    QMatrix4x4 foot_mesh = elephant_ctx.model;
    foot_mesh.translate(foot);
    foot_mesh.scale(d.foot_radius * 1.1F, d.foot_radius * 0.30F, d.foot_radius * 1.05F);
    out.mesh(get_unit_cylinder(), foot_mesh, v.skin_shadow, nullptr, 1.0F, kMaterialElephantLegs);

    // Thick pad cushion underneath
    QMatrix4x4 pad = elephant_ctx.model;
    pad.translate(foot + QVector3D(0.0F, -d.foot_radius * 0.15F, 0.0F));
    pad.scale(d.foot_radius * 0.95F, d.foot_radius * 0.18F, d.foot_radius * 0.90F);
    out.mesh(get_unit_cylinder(), pad, darken(v.skin_shadow, 0.85F), nullptr, 1.0F, kMaterialElephantLegs);

    // Issue #4: 4 toenail bumps arranged in an arc at the front
    constexpr int num_toenails = 4;
    for (int i = 0; i < num_toenails; ++i) {
      float const angle_offset = (static_cast<float>(i) - 1.5F) * 0.35F;
      QVector3D const nail_pos =
          foot + QVector3D(d.foot_radius * 0.80F * std::sin(angle_offset),
                           d.foot_radius * 0.08F,
                           d.foot_radius * 0.75F * std::cos(angle_offset));
      QMatrix4x4 nail = elephant_ctx.model;
      nail.translate(nail_pos);
      nail.scale(d.foot_radius * 0.14F, d.foot_radius * 0.12F, d.foot_radius * 0.16F);
      out.mesh(get_unit_sphere(), nail, v.toenail_color, nullptr, 1.0F, kMaterialElephantTusk);
    }
  };

  // Front legs
  draw_leg(d.body_width * 0.45F, d.body_length * 0.30F,
           profile.gait.front_leg_phase, true);
  draw_leg(-d.body_width * 0.45F, d.body_length * 0.30F,
           profile.gait.front_leg_phase + 0.5F, true);

  // Rear legs
  draw_leg(d.body_width * 0.45F, -d.body_length * 0.30F,
           profile.gait.rear_leg_phase, false);
  draw_leg(-d.body_width * 0.45F, -d.body_length * 0.30F,
           profile.gait.rear_leg_phase + 0.5F, false);

  // Tail
  {
    QVector3D const tail_base =
        barrel_center + QVector3D(0.0F, d.body_height * 0.2F,
                                  -d.body_length * 0.50F);
    float const tail_swing = std::sin(anim.time * 1.5F) * 0.1F;
    QVector3D const tail_tip =
        tail_base + QVector3D(tail_swing, -d.tail_length * 0.8F,
                              -d.tail_length * 0.3F);

    constexpr int k_tail_segments = 5;
    QVector3D prev = tail_base;
    for (int i = 1; i <= k_tail_segments; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_tail_segments);
      float const radius = d.leg_radius * 0.15F * (1.0F - t * 0.6F);
      QVector3D const next = lerp3(tail_base, tail_tip, t);
      draw_cylinder(out, elephant_ctx.model, prev, next, radius, v.skin_shadow,
                    1.0F, kMaterialElephantBody);
      prev = next;
    }

    // Tail tuft
    QMatrix4x4 tuft = elephant_ctx.model;
    tuft.translate(tail_tip);
    tuft.scale(d.leg_radius * 0.25F, d.leg_radius * 0.35F, d.leg_radius * 0.25F);
    out.mesh(get_unit_sphere(), tuft, darken(v.skin_shadow, 0.6F), nullptr,
             1.0F, kMaterialElephantBody);
  }

  // Howdah (war platform)
  ElephantBodyFrames body_frames;
  body_frames.back_center.origin = howdah.howdah_center;
  body_frames.back_center.right = QVector3D(1.0F, 0.0F, 0.0F);
  body_frames.back_center.up = QVector3D(0.0F, 1.0F, 0.0F);
  body_frames.back_center.forward = QVector3D(0.0F, 0.0F, 1.0F);

  draw_howdah(elephant_ctx, anim, profile, howdah, phase, bob, body_frames, out);
}

void ElephantRendererBase::render_simplified(
    const DrawContext &ctx, const AnimationInputs &anim,
    ElephantProfile &profile, const HowdahAttachmentFrame *shared_howdah,
    const ElephantMotionSample *shared_motion, ISubmitter &out) const {

  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;

  ElephantMotionSample const motion =
      shared_motion ? *shared_motion : evaluate_elephant_motion(profile, anim);
  float const bob = motion.bob;

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
    body.scale(d.body_width * 1.1F, d.body_height * 0.95F,
               d.body_length * 0.65F);
    out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Head
  QVector3D const head_center =
      barrel_center + QVector3D(0.0F, d.body_height * 0.6F,
                                d.body_length * 0.55F);
  {
    QMatrix4x4 head = elephant_ctx.model;
    head.translate(head_center);
    head.scale(d.head_width * 0.9F, d.head_height * 0.85F,
               d.head_length * 0.75F);
    out.mesh(get_unit_sphere(), head, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);
  }

  // Simple legs
  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.body_length * 0.28F : -d.body_length * 0.28F;

    QVector3D const top = barrel_center + QVector3D(x_sign * d.body_width * 0.42F,
                                                     -d.body_height * 0.35F,
                                                     z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.leg_length * 0.90F, 0.0F);

    draw_cylinder(out, elephant_ctx.model, top, bottom, d.leg_radius * 0.9F,
                  v.skin_color, 1.0F, kMaterialElephantLegs);
  }
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
  body.scale(d.body_width * 1.3F, d.body_height * 1.2F, d.body_length * 0.8F);
  out.mesh(get_unit_sphere(), body, v.skin_color, nullptr, 1.0F, kMaterialElephantBody);

  // Minimal legs
  for (int i = 0; i < 4; ++i) {
    float const x_sign = (i % 2 == 0) ? 1.0F : -1.0F;
    float const z_offset =
        (i < 2) ? d.body_length * 0.25F : -d.body_length * 0.25F;

    QVector3D const top = center + QVector3D(x_sign * d.body_width * 0.38F,
                                              -d.body_height * 0.4F, z_offset);
    QVector3D const bottom = top + QVector3D(0.0F, -d.leg_length * 0.70F, 0.0F);

    draw_cylinder(out, elephant_ctx.model, top, bottom, d.leg_radius * 0.7F,
                  v.skin_color * 0.85F, 1.0F, kMaterialElephantLegs);
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
