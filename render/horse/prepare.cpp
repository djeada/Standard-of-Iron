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

auto default_full_horse_request_seed(
    const Render::GL::DrawContext &ctx) noexcept -> std::uint32_t {
  if (ctx.entity == nullptr) {
    return 0U;
  }
  return static_cast<std::uint32_t>(
      reinterpret_cast<std::uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
}

auto make_grounding_pose(const Render::GL::HorseProfile &profile, float phase,
                         float bob, bool is_moving) noexcept
    -> Render::Horse::HorseSpecPose {
  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_reduced(
      profile.dims, profile.gait,
      Render::Horse::HorseReducedMotion{phase, bob, is_moving}, pose);
  return pose;
}

void ground_horse_model(QMatrix4x4 &model,
                        const Render::Horse::HorseSpecPose &pose,
                        std::uint16_t clip_id, float phase) noexcept {
  float const y_scale = model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  Render::Creature::Pipeline::ground_model_contact_to_surface(
      model,
      Render::Creature::Pipeline::grounded_horse_contact_y(pose, clip_id,
                                                           phase),
      y_scale);
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
                                      shared_mount, shared_motion, lod, prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
}

void HorseRendererBase::render(const DrawContext &ctx,
                               const AnimationInputs &anim,
                               const HumanoidAnimationContext &rider_ctx,
                               HorseProfile &profile,
                               const MountedAttachmentFrame *shared_mount,
                               const HorseMotionSample *shared_motion,
                               ISubmitter &out) const {
  render(ctx, anim, rider_ctx, profile, shared_mount, shared_motion, out,
         HorseLOD::Full);
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

void prepare_horse_full_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed);

void prepare_horse_simplified_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed);

void prepare_horse_minimal_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::HorseProfile &profile,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed);

void prepare_horse_full(const Render::GL::HorseRendererBase &owner,
                        const Render::GL::DrawContext &ctx,
                        const Render::GL::AnimationInputs &anim,
                        const Render::GL::HumanoidAnimationContext &rider_ctx,
                        Render::GL::HorseProfile &profile,
                        const Render::GL::MountedAttachmentFrame *shared_mount,
                        const Render::GL::HorseMotionSample *shared_motion,
                        HorsePreparation &out) {
  prepare_horse_full_impl(owner, ctx, anim, rider_ctx, profile, shared_mount,
                          shared_motion, out,
                          default_full_horse_request_seed(ctx));
}

void prepare_horse_full_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed) {
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  const HorseVariant &v = profile.variant;

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(
                          profile, anim, rider_ctx,
                          Engine::Core::get_or_add_component<
                              Render::Creature::HorseAnimationStateComponent>(
                              ctx.entity));
  float const phase = motion.phase;
  (void)shared_mount;
  Render::Horse::HorseSpecPose const pose =
      make_grounding_pose(profile, motion.phase, motion.bob, motion.is_moving);
  std::uint16_t const horse_clip = horse_clip_for_gait(motion.gait_type);

  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  ground_horse_model(horse_ctx.model, pose, horse_clip, motion.phase);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &horse_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Full;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = request_seed;
  out.bodies.add_horse(graph_output, pose, v, horse_clip, phase);
}

void prepare_horse_simplified(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out) {
  prepare_horse_simplified_impl(owner, ctx, anim, rider_ctx, profile,
                                shared_mount, shared_motion, out, 0U);
}

void prepare_horse_simplified_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed) {
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;

  const HorseVariant &v = profile.variant;

  HorseMotionSample const motion =
      shared_motion ? *shared_motion
                    : evaluate_horse_motion(
                          profile, anim, rider_ctx,
                          Engine::Core::get_or_add_component<
                              Render::Creature::HorseAnimationStateComponent>(
                              ctx.entity));
  (void)shared_mount;
  Render::Horse::HorseSpecPose const pose =
      make_grounding_pose(profile, motion.phase, motion.bob, motion.is_moving);
  std::uint16_t const horse_clip_reduced =
      horse_clip_for_gait(motion.gait_type);

  QMatrix4x4 world_from_unit = ctx.model;
  ground_horse_model(world_from_unit, pose, horse_clip_reduced, motion.phase);
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
  graph_output.seed = request_seed;
  out.bodies.add_horse(graph_output, pose, v, horse_clip_reduced, motion.phase);
}

void prepare_horse_minimal(const Render::GL::HorseRendererBase &owner,
                           const Render::GL::DrawContext &ctx,
                           Render::GL::HorseProfile &profile,
                           const Render::GL::HorseMotionSample *shared_motion,
                           HorsePreparation &out) {
  prepare_horse_minimal_impl(owner, ctx, profile, shared_motion, out, 0U);
}

void prepare_horse_minimal_impl(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::HorseProfile &profile,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out,
    std::uint32_t request_seed) {
  using Render::GL::HorseVariant;

  const HorseVariant &v = profile.variant;
  (void)shared_motion;

  Render::Horse::HorseSpecPose const pose =
      make_grounding_pose(profile, 0.0F, 0.0F, false);

  QMatrix4x4 world_from_unit = ctx.model;
  ground_horse_model(world_from_unit, pose, 0U, 0.0F);

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
  graph_output.seed = request_seed;

  out.bodies.add_horse(graph_output, pose, v, 0U, 0.0F);
}

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, HorsePreparation &out) {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    prepare_horse_full_impl(owner, ctx, anim, rider_ctx, profile, shared_mount,
                            shared_motion, out,
                            default_full_horse_request_seed(ctx));
    break;
  case Render::Creature::CreatureLOD::Reduced:
    prepare_horse_simplified_impl(owner, ctx, anim, rider_ctx, profile,
                                  shared_mount, shared_motion, out, 0U);
    break;
  case Render::Creature::CreatureLOD::Minimal:
    prepare_horse_minimal_impl(owner, ctx, profile, shared_motion, out, 0U);
    break;
  case Render::Creature::CreatureLOD::Billboard:
    break;
  }
}

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, HorsePreparation &out,
    std::uint32_t request_seed) {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    prepare_horse_full_impl(owner, ctx, anim, rider_ctx, profile, shared_mount,
                            shared_motion, out, request_seed);
    break;
  case Render::Creature::CreatureLOD::Reduced:
    prepare_horse_simplified_impl(owner, ctx, anim, rider_ctx, profile,
                                  shared_mount, shared_motion, out,
                                  request_seed);
    break;
  case Render::Creature::CreatureLOD::Minimal:
    prepare_horse_minimal_impl(owner, ctx, profile, shared_motion, out,
                               request_seed);
    break;
  case Render::Creature::CreatureLOD::Billboard:
    break;
  }
}

} // namespace Render::Horse
