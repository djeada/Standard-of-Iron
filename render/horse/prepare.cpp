#include "prepare.h"

#include "../creature/animation_state_components.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../equipment/equipment_submit.h"
#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "horse_motion.h"
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

namespace {

void ground_horse_model(QMatrix4x4 &model) {
  QVector3D const origin =
      Render::Creature::Pipeline::model_world_origin(model);
  float const grounded_y =
      Render::Creature::Pipeline::sample_terrain_height_or_fallback(
          origin.x(), origin.z(), origin.y());
  Render::Creature::Pipeline::set_model_world_y(model, grounded_y);
}

} // namespace

auto make_horse_prepared_row(
    const Render::GL::HorseRendererBase &owner,
    const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow {
  return Render::Creature::Pipeline::make_prepared_horse_row(
      owner.visual_spec(), pose, variant, world_from_unit, seed, lod, 0, pass);
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

constexpr float k_pi = std::numbers::pi_v<float>;

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
    break;
  case HorseLOD::Reduced:
    ++s_horseRenderStats.lod_reduced;
    break;
  case HorseLOD::Minimal:
    ++s_horseRenderStats.lod_minimal;
    break;
  case HorseLOD::Billboard:
    break;
  }

  Render::Horse::HorsePreparation prep;
  Render::Horse::prepare_horse_render(
      *this, ctx, anim, rider_ctx, profile, shared_mount, shared_reins,
      shared_motion, lod, horse_loadout, sub_ctx_template, prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
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
}

void prepare_horse_full(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
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
                    : evaluate_horse_motion(
                          profile, anim, rider_ctx,
                          Engine::Core::get_or_add_component<
                              Render::Creature::HorseAnimationStateComponent>(
                              ctx.entity));
  const HorseGait &g = motion.gait;
  float const phase = motion.phase;
  float const bob = motion.bob;
  const bool is_moving = motion.is_moving;
  const float rider_intensity = motion.rider_intensity;
  float const turn_amount = motion.turn_amount;
  float const stop_intent = motion.stop_intent;
  float const body_sway = motion.body_sway;
  float const body_pitch = motion.body_pitch;
  float const head_nod = motion.head_nod;
  float const head_lateral = motion.head_lateral;
  float const spine_flex = motion.spine_flex;

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
  ground_horse_model(horse_ctx.model);

  ReinState const rein_state =
      shared_reins ? *shared_reins : compute_rein_state(horse_seed, rider_ctx);
  float const rein_slack = rein_state.slack;
  (void)rein_state.tension;

  QVector3D const barrel_center(body_sway, d.barrel_center_y + bob + body_pitch,
                                spine_flex);

  QVector3D const chest_center =
      barrel_center +
      QVector3D(-turn_amount * d.body_width * 0.08F + body_sway * 0.2F,
                d.body_height * (0.12F + stop_intent * 0.04F),
                d.body_length * 0.34F);
  QVector3D const rump_center =
      barrel_center +
      QVector3D(turn_amount * d.body_width * 0.06F - body_sway * 0.1F,
                d.body_height * (0.08F - stop_intent * 0.02F),
                -d.body_length * 0.36F);

  QVector3D const neck_base =
      chest_center +
      QVector3D(head_lateral * 0.3F + turn_amount * d.head_width * 0.30F,
                d.body_height * (0.42F + stop_intent * 0.05F),
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

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &horse_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Full;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = horse_seed;
  out.bodies.add_horse(graph_output, pose, v);

  QVector3D const bit_left =
      muzzle_center + QVector3D(d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
  QVector3D const bit_right =
      muzzle_center + QVector3D(-d.head_width * 0.55F, -d.head_height * 0.08F,
                                d.muzzle_length * 0.10F);
  mount.rein_bit_left = bit_left;
  mount.rein_bit_right = bit_right;

  Render::GL::HorseBodyFrames body_frames;
  auto set_frame = [](Render::GL::HorseAttachmentFrame &frame,
                      const QVector3D &origin, const QVector3D &right,
                      const QVector3D &up, const QVector3D &forward) {
    frame.origin = origin;
    frame.right = right;
    frame.up = up;
    frame.forward = forward;
  };
  QVector3D const forward(0.0F, 0.0F, 1.0F);
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const right(1.0F, 0.0F, 0.0F);

  set_frame(body_frames.head, head_center, right, up, forward);
  set_frame(body_frames.neck_base, neck_base, right, up, forward);

  QVector3D const withers_pos =
      chest_center +
      QVector3D(0.0F, d.body_height * 0.55F, -d.body_length * 0.06F);
  set_frame(body_frames.withers, withers_pos, right, up, forward);
  set_frame(body_frames.back_center, mount.saddle_center, right, up, forward);

  QVector3D const croup_pos =
      rump_center +
      QVector3D(0.0F, d.body_height * 0.46F, -d.body_length * 0.18F);
  set_frame(body_frames.croup, croup_pos, right, up, forward);
  set_frame(body_frames.chest, chest_center, right, up, forward);
  set_frame(body_frames.barrel, barrel_center, right, up, forward);
  set_frame(body_frames.rump, rump_center, right, up, forward);

  QVector3D const tail_base_pos =
      rump_center +
      QVector3D(0.0F, d.body_height * 0.20F, -d.body_length * 0.46F);
  set_frame(body_frames.tail_base, tail_base_pos, right, up, forward);
  set_frame(body_frames.muzzle, muzzle_center, right, up, forward);

  using Render::Creature::Pipeline::LegacySlotMask;
  using Render::Creature::Pipeline::owns_slot;
  bool const draw_attachments_enabled = !owns_slot(
      owner.visual_spec().owned_legacy_slots, LegacySlotMask::HorseAttachments);
  bool const has_equipment =
      !horse_loadout.empty() && sub_ctx_template != nullptr;

  if (!draw_attachments_enabled && !has_equipment) {
    return;
  }

  out.add_post_body_draw(
      graph_output.pass_intent,
      [&owner, anim, rider_ctx, profile, horse_ctx, mount, body_frames, phase,
       bob, rein_slack, rein_state, motion, is_moving, rider_intensity,
       horse_loadout, sub_ctx_template, draw_attachments_enabled,
       has_equipment](Render::GL::ISubmitter &out_sub) mutable {
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
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out) {
  using Render::GL::HorseDimensions;
  using Render::GL::HorseGait;
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;

  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(
                          profile, anim, rider_ctx,
                          Engine::Core::get_or_add_component<
                              Render::Creature::HorseAnimationStateComponent>(
                              ctx.entity));
  const HorseGait &g = motion.gait;

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);
  if (!shared_mount) {
    apply_mount_vertical_offset(mount, motion.bob);
  }

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);
  ground_horse_model(world_from_unit);

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      d, g,
      Render::Horse::HorseReducedMotion{motion.phase, motion.bob,
                                        motion.is_moving},
      pose);
  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = world_from_unit;
  graph_inputs.ctx = &horse_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Reduced;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = 0U;
  out.bodies.add_horse(graph_output, pose, v);
}

void prepare_horse_minimal(const Render::GL::HorseRendererBase &owner,
                           const Render::GL::DrawContext &ctx,
                           Render::GL::HorseProfile &profile,
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
  ground_horse_model(world_from_unit);

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(d, bob, pose);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = world_from_unit;
  graph_inputs.ctx = &horse_ctx;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Minimal;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = 0U;
  out.bodies.add_horse(graph_output, pose, v);
}

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::ReinState *shared_reins,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
    const Render::Creature::Pipeline::EquipmentSubmitContext *sub_ctx_template,
    HorsePreparation &out) {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    prepare_horse_full(owner, ctx, anim, rider_ctx, profile, shared_mount,
                       shared_reins, shared_motion, horse_loadout,
                       sub_ctx_template, out);
    break;
  case Render::Creature::CreatureLOD::Reduced:
    prepare_horse_simplified(owner, ctx, anim, rider_ctx, profile, shared_mount,
                             shared_motion, out);
    break;
  case Render::Creature::CreatureLOD::Minimal:
    prepare_horse_minimal(owner, ctx, profile, shared_motion, out);
    break;
  case Render::Creature::CreatureLOD::Billboard:
    break;
  }
}

} // namespace Render::Horse
