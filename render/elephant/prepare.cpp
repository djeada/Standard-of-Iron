#include "prepare.h"

#include "../creature/animation_state_components.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "elephant_motion.h"
#include "elephant_renderer_base.h"
#include "elephant_spec.h"
#include "lod.h"
#include "render_stats.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::Elephant {

namespace {

[[nodiscard]] inline auto
elephant_clip_for_motion(bool is_moving,
                         bool is_running) noexcept -> std::uint16_t {
  if (!is_moving) {
    return 0U;
  }
  return is_running ? 2U : 1U;
}

} // namespace

} // namespace Render::Elephant

namespace {

[[nodiscard]] auto make_runtime_prewarm_ctx(const Render::GL::DrawContext &ctx)
    -> Render::GL::DrawContext {
  Render::GL::DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.allow_template_cache = false;
  return runtime_ctx;
}

} // namespace

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
      ctx.template_prewarm ? make_runtime_prewarm_ctx(ctx) : ctx;

  HorseLOD effective_lod = lod;
  if (render_ctx.force_horse_lod) {
    effective_lod = render_ctx.forced_horse_lod;
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

void prepare_elephant_full(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out) {
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

  Render::Elephant::ElephantPoseMotion const rm =
      build_elephant_pose_motion(motion, anim);
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(profile.dims, profile.gait,
                                                     rm, pose);
  pose.barrel_center = motion.barrel_center;
  pose.head_center = motion.head_center;

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &elephant_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Full;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = 0U;
  std::uint16_t const eleph_clip_full =
      elephant_clip_for_motion(rm.is_moving, anim.is_running);
  out.bodies.add_elephant(graph_output, pose, v, eleph_clip_full, rm.phase);
}

void prepare_elephant_minimal(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::ElephantProfile &profile,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out) {
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const ElephantVariant &v = profile.variant;

  float const bob = shared_motion ? shared_motion->bob : 0.0F;
  HowdahAttachmentFrame howdah =
      shared_motion ? shared_motion->howdah : compute_howdah_frame(profile);
  if (!shared_motion) {
    apply_howdah_vertical_offset(howdah, bob);
  }

  QMatrix4x4 world_from_unit = ctx.model;
  world_from_unit.translate(howdah.ground_offset);
  Render::Creature::Pipeline::ground_model_to_terrain(world_from_unit);

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose(profile.dims, bob, pose);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  Render::GL::DrawContext elephant_ctx = ctx;
  elephant_ctx.model = world_from_unit;
  graph_inputs.ctx = &elephant_ctx;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = Render::Creature::CreatureLOD::Minimal;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = 0U;

  out.bodies.add_elephant(graph_output, pose, v, 0U, 0.0F);
}

void prepare_elephant_render(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, ElephantPreparation &out) {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    prepare_elephant_full(owner, ctx, anim, profile, shared_howdah,
                          shared_motion, out);
    break;
  case Render::Creature::CreatureLOD::Minimal:
    prepare_elephant_minimal(owner, ctx, profile, shared_motion, out);
    break;
  case Render::Creature::CreatureLOD::Billboard:
    break;
  }
}

} // namespace Render::Elephant
