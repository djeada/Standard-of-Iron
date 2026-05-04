#include "prepare.h"

#include "../../game/core/component.h"
#include "../creature/animation_state_components.h"
#include "../creature/pipeline/creature_prepared_state.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/quadruped/render_stats.h"
#include "../gl/camera.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "elephant_motion.h"
#include "elephant_renderer_base.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::Elephant {

namespace {

constexpr float k_shadow_size_elephant = 0.56F;
constexpr float k_shadow_width_scale_elephant = 1.35F;
constexpr float k_shadow_depth_scale_elephant = 1.85F;

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
  namespace RCP = Render::Creature::Pipeline;
  RCP::ElephantMotionStateInputs motion_inputs{};
  motion_inputs.ctx = &ctx;
  motion_inputs.anim = &anim;
  motion_inputs.profile = &profile;
  motion_inputs.shared_howdah = shared_howdah;
  motion_inputs.shared_motion = shared_motion;
  const auto motion_state = RCP::resolve_elephant_motion_state(motion_inputs);

  RCP::QuadrupedFrameStateInputs frame_inputs{};
  frame_inputs.ctx = &ctx;
  frame_inputs.anim = &anim;
  frame_inputs.spec = owner.visual_spec();
  frame_inputs.lod = lod;
  frame_inputs.seed = 0U;
  frame_inputs.pre_ground_translation = motion_state.howdah.ground_offset;
  const auto frame_state = RCP::prepare_quadruped_frame_state(frame_inputs);
  auto *unit_comp =
      ctx.entity != nullptr
          ? ctx.entity->get_component<Engine::Core::UnitComponent>()
          : nullptr;
  QVector3D const world_pos =
      RCP::model_world_origin(frame_state.graph.world_matrix);
  float camera_distance = 0.0F;
  if (frame_state.ctx.camera != nullptr) {
    camera_distance =
        (world_pos - frame_state.ctx.camera->get_position()).length();
  }
  RCP::GroundShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = &frame_state.ctx;
  shadow_inputs.graph = &frame_state.graph;
  shadow_inputs.unit = unit_comp;
  shadow_inputs.world_pos = world_pos;
  shadow_inputs.lod = lod;
  shadow_inputs.camera_distance = camera_distance;
  shadow_inputs.shadow_size = k_shadow_size_elephant;
  shadow_inputs.width_scale = k_shadow_width_scale_elephant;
  shadow_inputs.depth_scale = k_shadow_depth_scale_elephant;
  const auto shadow_state = RCP::prepare_ground_shadow_state(shadow_inputs);
  if (shadow_state.enabled) {
    out.add_post_body_draw(
        shadow_state.pass, [shadow_state](Render::GL::ISubmitter &submitter) {
          submitter.part(shadow_state.mesh,
                         Render::GL::MaterialRegistry::instance().shadow(),
                         shadow_state.model, QVector3D(0.0F, 0.0F, 0.0F),
                         nullptr, shadow_state.alpha, 0);
        });
  }

  RCP::PreparedElephantBodyState body_state;
  body_state.graph = frame_state.graph;
  body_state.variant = v;
  body_state.animation_state = motion_state.animation_state;
  body_state.phase = motion_state.motion.phase;
  out.bodies.add_quadruped(body_state);
}

} // namespace Render::Elephant
