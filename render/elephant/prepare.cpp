#include "prepare.h"

#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "elephant_renderer_base.h"
#include "elephant_spec.h"
#include "lod.h"
#include "render_stats.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <unordered_map>

namespace Render::Elephant {

auto make_elephant_prepared_row(
    const Render::GL::ElephantRendererBase &owner,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow {
  return Render::Creature::Pipeline::make_prepared_elephant_row(
      owner.visual_spec(), pose, variant, world_from_unit, seed, lod,
      /*entity_id*/ 0, pass);
}

void submit_prepared_elephant_body(
    const Render::GL::ElephantRendererBase &owner,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, Render::GL::ISubmitter &out) noexcept {
  Render::Creature::Pipeline::PreparedCreatureSubmitBatch batch;
  batch.reserve(1);
  batch.add(make_elephant_prepared_row(owner, pose, variant, world_from_unit,
                                       seed, lod));
  (void)batch.submit(out);
}

} // namespace Render::Elephant

namespace Render::GL {

static ElephantRenderStats s_elephantRenderStats;

auto get_elephant_render_stats() -> const ElephantRenderStats & {
  return s_elephantRenderStats;
}

void reset_elephant_render_stats() { s_elephantRenderStats.reset(); }

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

inline auto saturate(float x) -> float {
  return std::min(1.0F, std::max(0.0F, x));
}

inline auto hash01(uint32_t x) -> float {
  x ^= x >> k_hash_shift_16;
  x *= k_hash_mult_1;
  x ^= x >> k_hash_shift_15;
  x *= k_hash_mult_2;
  x ^= x >> k_hash_shift_16;
  return (x & k_hash_mask_24bit) / k_hash_divisor;
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

  ElephantProfile profile =
      make_elephant_profile(seed, fabric_base, metal_base);

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

void ElephantRendererBase::render(const DrawContext &ctx,
                                  const AnimationInputs &anim,
                                  ElephantProfile &profile,
                                  const HowdahAttachmentFrame *shared_howdah,
                                  const ElephantMotionSample *shared_motion,
                                  ISubmitter &out, HorseLOD lod) const {
  HorseLOD effective_lod = lod;
  if (ctx.force_horse_lod) {
    effective_lod = ctx.forced_horse_lod;
  }

  if (ctx.template_prewarm) {
    // Prewarm contexts are tagged RenderPassIntent::Shadow and filtered out by
    // PreparedCreatureSubmitBatch::submit, but the per-LOD render_full /
    // render_simplified / render_minimal helpers below emit primitives directly
    // through ISubmitter (not via the prepared batch) and also bump per-frame
    // stats counters. Keep this guard so prewarm doesn't record those draws.
    return;
  }

  ++s_elephantRenderStats.elephants_total;

  if (effective_lod == HorseLOD::Billboard) {
    ++s_elephantRenderStats.elephants_skipped_lod;
    return;
  }

  ++s_elephantRenderStats.elephants_rendered;

  switch (effective_lod) {
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

void ElephantRendererBase::render(const DrawContext &ctx,
                                  const AnimationInputs &anim,
                                  ElephantProfile &profile,
                                  const HowdahAttachmentFrame *shared_howdah,
                                  const ElephantMotionSample *shared_motion,
                                  ISubmitter &out) const {
  render(ctx, anim, profile, shared_howdah, shared_motion, out, HorseLOD::Full);
}

} // namespace Render::GL

namespace Render::Elephant {

namespace {
constexpr float k_pi = std::numbers::pi_v<float>;
} // namespace

void prepare_elephant_full(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx,
    const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out) {
  using Render::GL::ElephantDimensions;
  using Render::GL::ElephantGait;
  using Render::GL::ElephantMotionSample;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const auto pass_intent =
      Render::Creature::Pipeline::pass_intent_from_ctx(ctx);
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;
  const ElephantGait &g = profile.gait;

  ElephantMotionSample const motion =
      shared_motion ? *shared_motion : evaluate_elephant_motion(profile, anim);
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;

  bool const is_fighting =
      anim.is_attacking ||
      (anim.combat_phase != Render::GL::CombatAnimPhase::Idle);
  float const trunk_swing = motion.trunk_swing;
  float const ear_flap = motion.ear_flap;

  HowdahAttachmentFrame howdah =
      shared_howdah ? *shared_howdah : compute_howdah_frame(profile);
  if (!shared_howdah) {
    apply_howdah_vertical_offset(howdah, bob);
  }

  Render::GL::DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);

  uint32_t const vhash = Render::GL::color_hash(v.skin_color);
  float const skin_seed_a = Render::GL::hash01(vhash ^ 0x701U);
  float const skin_seed_b = Render::GL::hash01(vhash ^ 0x702U);

  float const body_sway = is_moving ? std::sin(phase * 2.0F * k_pi) * 0.015F
                                    : std::sin(anim.time * 0.3F) * 0.008F;

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob, 0.0F);

  QVector3D const chest_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.10F, d.body_length * 0.30F);
  QVector3D const rump_center =
      barrel_center +
      QVector3D(0.0F, d.body_height * 0.02F, -d.body_length * 0.32F);
  QVector3D const belly_center =
      barrel_center +
      QVector3D(0.0F, -d.body_height * 0.22F, d.body_length * 0.05F);
  QVector3D const neck_base =
      chest_center +
      QVector3D(0.0F, d.body_height * 0.25F, d.body_length * 0.15F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, d.neck_length * 0.60F, d.neck_length * 0.50F);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * 0.20F, d.head_length * 0.35F);
  QVector3D const forehead_center =
      head_center +
      QVector3D(0.0F, d.head_height * 0.35F, d.head_length * 0.10F);
  (void)forehead_center;
  (void)rump_center;
  (void)belly_center;

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion rm{
      phase, bob, is_moving, is_fighting, anim.time, anim.combat_phase};
  Render::Elephant::make_elephant_spec_pose_reduced(d, g, rm, pose);
  pose.barrel_center = barrel_center;
  pose.head_center = head_center;
  (void)trunk_swing;
  (void)ear_flap;
  (void)skin_seed_a;
  (void)skin_seed_b;

  out.rows.emplace_back(make_elephant_prepared_row(
      owner, pose, v, elephant_ctx.model, 0,
      Render::Creature::CreatureLOD::Full, pass_intent));
}

void prepare_elephant_simplified(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx,
    const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out) {
  using Render::GL::ElephantDimensions;
  using Render::GL::ElephantGait;
  using Render::GL::ElephantMotionSample;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const auto pass_intent =
      Render::Creature::Pipeline::pass_intent_from_ctx(ctx);
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;
  const ElephantGait &g = profile.gait;

  ElephantMotionSample const motion =
      shared_motion ? *shared_motion : evaluate_elephant_motion(profile, anim);

  bool const is_fighting =
      anim.is_attacking ||
      (anim.combat_phase != Render::GL::CombatAnimPhase::Idle);

  HowdahAttachmentFrame howdah =
      shared_howdah ? *shared_howdah : compute_howdah_frame(profile);
  if (!shared_howdah) {
    apply_howdah_vertical_offset(howdah, motion.bob);
  }

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(howdah.ground_offset);

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion rm{
      motion.phase, motion.bob, motion.is_moving,
      is_fighting,  anim.time,  anim.combat_phase};
  Render::Elephant::make_elephant_spec_pose_reduced(d, g, rm, pose);
  out.rows.emplace_back(make_elephant_prepared_row(
      owner, pose, v, world_from_unit, 0,
      Render::Creature::CreatureLOD::Reduced, pass_intent));
}

void prepare_elephant_minimal(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::ElephantProfile &profile,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out) {
  using Render::GL::ElephantDimensions;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const auto pass_intent =
      Render::Creature::Pipeline::pass_intent_from_ctx(ctx);
  const ElephantDimensions &d = profile.dims;
  const ElephantVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;

  HowdahAttachmentFrame howdah = compute_howdah_frame(profile);
  apply_howdah_vertical_offset(howdah, bob);

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(howdah.ground_offset);

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(d, bob, pose);

  out.rows.emplace_back(make_elephant_prepared_row(
      owner, pose, v, world_from_unit, 0,
      Render::Creature::CreatureLOD::Minimal, pass_intent));
}

} // namespace Render::Elephant
