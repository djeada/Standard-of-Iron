#include "prepare.h"

#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../equipment/equipment_submit.h"
#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "horse_renderer_base.h"
#include "horse_spec.h"
#include "lod.h"
#include "render_stats.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <numbers>
#include <unordered_map>

namespace Render::Horse {

auto make_horse_prepared_row(
    const Render::GL::HorseRendererBase &owner,
    const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow {
  return Render::Creature::Pipeline::make_prepared_horse_row(
      owner.visual_spec(), pose, variant, world_from_unit, seed, lod,
      /*entity_id*/ 0, pass);
}

void submit_prepared_horse_body(const Render::GL::HorseRendererBase &owner,
                                const Render::Horse::HorseSpecPose &pose,
                                const Render::GL::HorseVariant &variant,
                                const QMatrix4x4 &world_from_unit,
                                std::uint32_t seed,
                                Render::Creature::CreatureLOD lod,
                                Render::GL::ISubmitter &out) noexcept {
  Render::Creature::Pipeline::PreparedCreatureSubmitBatch batch;
  batch.reserve(1);
  batch.add(make_horse_prepared_row(owner, pose, variant, world_from_unit, seed,
                                    lod));
  (void)batch.submit(out);
}

} // namespace Render::Horse

namespace Render::GL {

static HorseRenderStats s_horseRenderStats;

auto get_horse_render_stats() -> const HorseRenderStats & {
  return s_horseRenderStats;
}

void reset_horse_render_stats() { s_horseRenderStats.reset(); }

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

} // namespace

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

void HorseRendererBase::render(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
    const MountedAttachmentFrame *shared_mount, const ReinState *shared_reins,
    const HorseMotionSample *shared_motion, ISubmitter &out, HorseLOD lod,
    Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
    const Render::Creature::Pipeline::EquipmentSubmitContext *sub_ctx_template)
    const {

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
                shared_motion, out, horse_loadout, sub_ctx_template);
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

namespace Render::Horse {

namespace {
constexpr float k_pi = std::numbers::pi_v<float>;
} // namespace

void prepare_horse_full(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx,
    const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::ReinState *shared_reins,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
    const Render::Creature::Pipeline::EquipmentSubmitContext *sub_ctx_template,
    HorsePreparation &out) {
  using Render::GL::HorseDimensions;
  using Render::GL::HorseGait;
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;
  using Render::GL::ReinState;

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

  Render::GL::DrawContext horse_ctx = ctx;
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

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      d, g, Render::Horse::HorseReducedMotion{phase, bob, is_moving}, pose);
  pose.barrel_center = barrel_center;
  pose.neck_base = neck_base;
  pose.neck_top = neck_top;
  pose.head_center = head_center;

  out.rows.emplace_back(make_horse_prepared_row(
      owner, pose, v, horse_ctx.model, horse_seed,
      Render::Creature::CreatureLOD::Full));

  QVector3D const bit_left =
      muzzle_center + QVector3D(d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
  QVector3D const bit_right =
      muzzle_center + QVector3D(-d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
  mount.rein_bit_left = bit_left;
  mount.rein_bit_right = bit_right;

  Render::GL::HorseBodyFrames body_frames;
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

  using Render::Creature::Pipeline::LegacySlotMask;
  using Render::Creature::Pipeline::owns_slot;
  bool const draw_attachments_enabled =
      !owns_slot(owner.visual_spec().owned_legacy_slots,
                 LegacySlotMask::HorseAttachments);
  bool const has_equipment =
      !horse_loadout.empty() && sub_ctx_template != nullptr;

  if (!draw_attachments_enabled && !has_equipment) {
    return;
  }

  out.post_body_draws.emplace_back(
      [&owner, &anim, &rider_ctx, &profile, horse_ctx, mount, body_frames,
       phase, bob, rein_slack, rein_state, motion, is_moving, rider_intensity,
       horse_loadout, sub_ctx_template, draw_attachments_enabled,
       has_equipment](Render::GL::ISubmitter &out_sub) {
        if (draw_attachments_enabled) {
          owner.draw_attachments(horse_ctx, anim, rider_ctx, profile, mount,
                                 phase, bob, rein_slack, body_frames, out_sub);
        }

        if (has_equipment) {
          Render::GL::HorseAnimationContext horse_anim_ctx{};
          horse_anim_ctx.time = anim.time;
          horse_anim_ctx.phase = phase;
          horse_anim_ctx.bob = bob;
          horse_anim_ctx.is_moving = is_moving;
          horse_anim_ctx.rider_intensity = rider_intensity;

          Render::Creature::Pipeline::EquipmentSubmitContext sub_ctx =
              *sub_ctx_template;
          sub_ctx.ctx = &horse_ctx;
          sub_ctx.horse_frames = &body_frames;
          sub_ctx.horse_variant = &profile.variant;
          sub_ctx.horse_anim = &horse_anim_ctx;
          sub_ctx.horse_profile = &profile;
          sub_ctx.mount_frame = &mount;
          sub_ctx.rein_state = &rein_state;
          sub_ctx.horse_motion = &motion;

          for (const auto &record : horse_loadout) {
            if (!record.dispatch) {
              continue;
            }
            Render::GL::EquipmentBatch batch;
            record.dispatch(sub_ctx, batch);
            Render::GL::submit_equipment_batch(batch, out_sub);
          }
        }
      });
}

void prepare_horse_simplified(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx,
    const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion,
    HorsePreparation &out) {
  using Render::GL::HorseDimensions;
  using Render::GL::HorseGait;
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;

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
  out.rows.emplace_back(make_horse_prepared_row(
      owner, pose, v, world_from_unit, 0,
      Render::Creature::CreatureLOD::Reduced));
}

void prepare_horse_minimal(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::HorseProfile &profile,
    const Render::GL::HorseMotionSample *shared_motion,
    HorsePreparation &out) {
  using Render::GL::HorseDimensions;
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;

  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;

  MountedAttachmentFrame mount = compute_mount_frame(profile);
  apply_mount_vertical_offset(mount, bob);

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(d, bob, pose);

  out.rows.emplace_back(make_horse_prepared_row(
      owner, pose, v, world_from_unit, 0,
      Render::Creature::CreatureLOD::Minimal));
}

} // namespace Render::Horse
