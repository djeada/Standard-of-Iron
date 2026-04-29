#include "prepare.h"

#include "../creature/animation_state_components.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/quadruped/clip_set.h"
#include "../creature/quadruped/render_stats.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "elephant_motion.h"
#include "elephant_renderer_base.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::Elephant {

namespace {

constexpr Render::Creature::Quadruped::ClipSet k_elephant_clips{
    0U,
    1U,
    2U,
    Render::Creature::Quadruped::kInvalidClip,
    Render::Creature::Quadruped::kInvalidClip,
    3U};

auto elephant_state_for_motion(const Render::GL::ElephantMotionSample &motion,
                               const Render::GL::AnimationInputs &anim) noexcept
    -> Render::Creature::AnimationStateId {
  if (motion.is_fighting) {
    return Render::Creature::AnimationStateId::AttackMelee;
  }
  if (!motion.is_moving) {
    return Render::Creature::AnimationStateId::Idle;
  }
  return anim.is_running ? Render::Creature::AnimationStateId::Run
                         : Render::Creature::AnimationStateId::Walk;
}

auto elephant_clip_for_motion(const Render::GL::ElephantMotionSample &motion,
                              const Render::GL::AnimationInputs &anim) noexcept
    -> std::uint16_t {
  return Render::Creature::Quadruped::clip_for_motion(
      k_elephant_clips, motion.is_moving, anim.is_running, motion.is_fighting);
}

} // namespace

} // namespace Render::Elephant

namespace {}

namespace Render::GL {

static ElephantRenderStats s_elephantRenderStats;

auto get_elephant_render_stats() -> const ElephantRenderStats & {
  return s_elephantRenderStats;
}

void reset_elephant_render_stats() { s_elephantRenderStats.reset(); }

void ElephantRendererBase::render(const DrawContext &ctx,
                                  const AnimationInputs &anim,
                                  ElephantProfile &profile,
                                  const HowdahAttachmentFrame *shared_howdah,
                                  const ElephantMotionSample *shared_motion,
                                  ISubmitter &out, HorseLOD lod) const {
  DrawContext render_ctx =
      ctx.template_prewarm
          ? Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx)
          : ctx;

  HorseLOD effective_lod = lod;
  if (render_ctx.force_horse_lod) {
    effective_lod = render_ctx.forced_horse_lod;
  }

  ++s_elephantRenderStats.total;

  if (effective_lod == HorseLOD::Billboard) {
    ++s_elephantRenderStats.skipped_lod;
    return;
  }

  ++s_elephantRenderStats.rendered;

  switch (effective_lod) {
  case HorseLOD::Full:
    ++s_elephantRenderStats.lod_full;
    break;
  case HorseLOD::Minimal:
    ++s_elephantRenderStats.lod_minimal;
    break;
  case HorseLOD::Billboard:
    break;
  }

  Render::Elephant::ElephantPreparation prep;
  Render::Elephant::prepare_elephant_render(*this, render_ctx, anim, profile,
                                            shared_howdah, shared_motion,
                                            effective_lod, prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
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

void prepare_elephant_render(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, ElephantPreparation &out) {
  if (lod == Render::Creature::CreatureLOD::Billboard) {
    return;
  }

  using Render::GL::ElephantMotionSample;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const ElephantVariant &v = profile.variant;
  ElephantMotionSample const motion =
      shared_motion
          ? *shared_motion
          : evaluate_elephant_motion(
                profile, anim,
                Engine::Core::get_or_add_component<
                    Render::Creature::ElephantAnimationStateComponent>(
                    ctx.entity));

  HowdahAttachmentFrame const howdah =
      shared_howdah ? *shared_howdah : motion.howdah;

  Render::GL::DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);
  Render::Creature::Pipeline::ground_model_to_terrain(elephant_ctx.model);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &elephant_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = lod;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = 0U;
  std::uint16_t const elephant_clip = elephant_clip_for_motion(motion, anim);
  out.bodies.add_quadruped(
      graph_output, v, elephant_state_for_motion(motion, anim), motion.phase);
}

} // namespace Render::Elephant
