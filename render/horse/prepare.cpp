#include "prepare.h"

#include "../creature/animation_state_components.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "horse_motion.h"
#include "horse_renderer_base.h"
#include "horse_spec.h"
#include "lod.h"
#include "render_stats.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

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
  Render::Horse::prepare_horse_render(*this, ctx, anim, rider_ctx, profile,
                                      shared_mount, shared_reins, shared_motion,
                                      lod, prep);
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
[[nodiscard]] inline auto
horse_clip_for_gait(Render::GL::GaitType gait) noexcept -> std::uint16_t {
  using Render::GL::GaitType;
  switch (gait) {
  case GaitType::IDLE:
    return 0U;
  case GaitType::WALK:
    return 1U;
  case GaitType::TROT:
    return 2U;
  case GaitType::CANTER:
    return 3U;
  case GaitType::GALLOP:
    return 4U;
  }
  return 0U;
}
} // namespace

void prepare_horse_full(const Render::GL::HorseRendererBase &owner,
                        const Render::GL::DrawContext &ctx,
                        const Render::GL::AnimationInputs &anim,
                        const Render::GL::HumanoidAnimationContext &rider_ctx,
                        Render::GL::HorseProfile &profile,
                        const Render::GL::MountedAttachmentFrame *shared_mount,
                        const Render::GL::ReinState *shared_reins,
                        const Render::GL::HorseMotionSample *shared_motion,
                        HorsePreparation &out) {
  using Render::GL::HorseDimensions;
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
  float const phase = motion.phase;
  float const bob = motion.bob;

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);

  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  horse_ctx.model.translate(mount.ground_offset);
  ground_horse_model(horse_ctx.model);

  Render::Horse::HorseSpecPose const pose{};

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
  std::uint16_t const horse_clip = horse_clip_for_gait(motion.gait_type);
  out.bodies.add_horse(graph_output, pose, v, horse_clip, phase);

  using Render::Creature::Pipeline::LegacySlotMask;
  using Render::Creature::Pipeline::owns_slot;
  bool const draw_attachments_enabled = !owns_slot(
      owner.visual_spec().owned_legacy_slots, LegacySlotMask::HorseAttachments);
  if (!draw_attachments_enabled) {
    return;
  }

  ReinState const rein_state =
      shared_reins ? *shared_reins : compute_rein_state(horse_seed, rider_ctx);
  float const rein_slack = rein_state.slack;

  float const turn_amount = motion.turn_amount;
  float const stop_intent = motion.stop_intent;
  float const body_sway = motion.body_sway;
  float const body_pitch = motion.body_pitch;
  float const head_nod = motion.head_nod;
  float const head_lateral = motion.head_lateral;
  float const spine_flex = motion.spine_flex;

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

  out.add_post_body_draw(
      graph_output.pass_intent,
      [&owner, anim, rider_ctx, profile, horse_ctx, mount, body_frames, phase,
       bob, rein_slack](Render::GL::ISubmitter &out_sub) mutable {
        owner.draw_attachments(horse_ctx, anim, rider_ctx, profile, mount,
                               phase, bob, rein_slack, body_frames, out_sub);
      });
}

void prepare_horse_simplified(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out) {
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;

  const HorseVariant &v = profile.variant;

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(
                          profile, anim, rider_ctx,
                          Engine::Core::get_or_add_component<
                              Render::Creature::HorseAnimationStateComponent>(
                              ctx.entity));

  MountedAttachmentFrame mount =
      shared_mount ? *shared_mount : compute_mount_frame(profile);

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);
  ground_horse_model(world_from_unit);

  Render::Horse::HorseSpecPose const pose{};
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
  std::uint16_t const horse_clip_reduced =
      horse_clip_for_gait(motion.gait_type);
  out.bodies.add_horse(graph_output, pose, v, horse_clip_reduced, motion.phase);
}

void prepare_horse_minimal(const Render::GL::HorseRendererBase &owner,
                           const Render::GL::DrawContext &ctx,
                           Render::GL::HorseProfile &profile,
                           const Render::GL::HorseMotionSample *shared_motion,
                           HorsePreparation &out) {
  using Render::GL::HorseVariant;
  using Render::GL::MountedAttachmentFrame;

  const HorseVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;
  (void)bob;

  MountedAttachmentFrame mount = compute_mount_frame(profile);

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(mount.ground_offset);
  ground_horse_model(world_from_unit);

  Render::Horse::HorseSpecPose const pose{};

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

  out.bodies.add_horse(graph_output, pose, v, 0U, 0.0F);
}

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::ReinState *shared_reins,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, HorsePreparation &out) {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    prepare_horse_full(owner, ctx, anim, rider_ctx, profile, shared_mount,
                       shared_reins, shared_motion, out);
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
