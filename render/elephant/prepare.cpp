#include "prepare.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "elephant_motion.h"
#include "elephant_renderer_base.h"
#include "game/core/component.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/movement_animation.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/quadruped/mount_scale.h"
#include "render/creature/quadruped/quadruped_prepare.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/quadruped/runtime_context.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/math/creature_math_utils.h"
#include "render/submitter.h"
#include "scene/camera.h"

namespace Render::Elephant {

namespace {

auto elephant_state_for_motion(const Render::GL::ElephantMotionSample& motion,
                               const Render::GL::AnimationInputs& presentation) noexcept
    -> Render::Creature::AnimationStateId {
  if (presentation.is_dying) {
    return Render::Creature::AnimationStateId::Die;
  }
  if (presentation.is_dead) {
    return Render::Creature::AnimationStateId::Dead;
  }
  if (motion.is_fighting) {
    return Render::Creature::AnimationStateId::AttackMelee;
  }
  return Render::Creature::animation_state_for_movement(motion.movement_state);
}

} // namespace

} // namespace Render::Elephant

namespace {}

namespace Render::GL {

namespace {

auto elephant_stats() noexcept -> ElephantRenderStats& {
  return Render::Creature::Quadruped::current_quadruped_runtime_context().elephant;
}

} // namespace

auto get_elephant_render_stats() -> const ElephantRenderStats& {
  return elephant_stats();
}

void reset_elephant_render_stats() {
  elephant_stats().reset();
}

void ElephantRendererBase::render(const DrawContext& ctx,
                                  const AnimationInputs& anim,
                                  ElephantProfile& profile,
                                  const HowdahAttachmentFrame* shared_howdah,
                                  const ElephantMotionSample* shared_motion,
                                  ISubmitter& out,
                                  Render::Creature::CreatureLOD lod) const {

  DrawContext const render_ctx =
      ctx.template_prewarm ? Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx)
                           : ctx;

  Render::Creature::CreatureLOD effective_lod = lod;
  if (render_ctx.force_quadruped_lod) {
    effective_lod = render_ctx.forced_quadruped_lod;
  }

  ++elephant_stats().total;

  if (effective_lod == Render::Creature::CreatureLOD::Culled) {
    ++elephant_stats().skipped_lod;
    return;
  }

  ++elephant_stats().rendered;

  switch (effective_lod) {
  case Render::Creature::CreatureLOD::Full:
    ++elephant_stats().lod_full;
    break;
  case Render::Creature::CreatureLOD::Minimal:
    ++elephant_stats().lod_minimal;
    break;
  case Render::Creature::CreatureLOD::Culled:
    break;
  }

  Render::Elephant::ElephantPreparation prep;
  Render::Elephant::prepare_elephant_render(*this,
                                            render_ctx,
                                            anim,
                                            profile,
                                            shared_howdah,
                                            shared_motion,
                                            effective_lod,
                                            prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
}

void ElephantRendererBase::render(const DrawContext& ctx,
                                  const AnimationInputs& anim,
                                  ElephantProfile& profile,
                                  const HowdahAttachmentFrame* shared_howdah,
                                  const ElephantMotionSample* shared_motion,
                                  ISubmitter& out) const {
  render(ctx,
         anim,
         profile,
         shared_howdah,
         shared_motion,
         out,
         Render::Creature::CreatureLOD::Full);
}

} // namespace Render::GL

namespace Render::Elephant {

void prepare_elephant_render(const Render::GL::ElephantRendererBase& owner,
                             const Render::GL::DrawContext& ctx,
                             const Render::GL::AnimationInputs& anim,
                             Render::GL::ElephantProfile& profile,
                             const Render::GL::HowdahAttachmentFrame* shared_howdah,
                             const Render::GL::ElephantMotionSample* shared_motion,
                             Render::Creature::CreatureLOD lod,
                             ElephantPreparation& out) {
  if (lod == Render::Creature::CreatureLOD::Culled) {
    return;
  }

  using Render::GL::ElephantMotionSample;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const ElephantVariant& v = profile.variant;
  ElephantMotionSample const motion =
      (shared_motion != nullptr)
          ? *shared_motion
          : evaluate_elephant_motion(
                profile,
                anim,
                Engine::Core::get_or_add_component<
                    Render::Creature::ElephantAnimationStateComponent>(ctx.entity),
                Render::Creature::Quadruped::mount_model_scale(ctx.world, ctx.entity),
                Animation::resolve_soldier_individuality({
                    .soldier_seed =
                        ctx.entity != nullptr
                            ? Render::Creature::stable_entity_seed(ctx.entity->get_id())
                            : 0U,
                }));

  HowdahAttachmentFrame const howdah =
      (shared_howdah != nullptr) ? *shared_howdah : motion.howdah;
  Render::GL::DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);
  const float elephant_surface_world_y =
      Render::Creature::Pipeline::ground_model_to_terrain(
          ctx.world_view.terrain_or_empty(), elephant_ctx.model);

  namespace RCP = Render::Creature::Pipeline;
  namespace RCQ = Render::Creature::Quadruped;

  RCQ::QuadrupedRuntimeInput input{};
  input.ctx = &elephant_ctx;
  input.anim = &anim;
  input.spec = &owner.visual_spec();
  input.kind = RCP::CreatureKind::Elephant;
  input.lod = lod;
  input.animation = elephant_state_for_motion(motion, anim);
  input.phase = (anim.is_dying || anim.is_dead) ? anim.death_progress : motion.phase;
  input.world = elephant_ctx.model;
  input.seed = 0U;
  input.surface_world_y = elephant_surface_world_y;
  input.surface_height_valid = true;
  input.shadow_intensity_scale = (anim.is_dying || anim.is_dead) ? 0.45F : 1.0F;

  auto const body = RCQ::build_quadruped_body(input);

  RCP::PreparedElephantBodyState body_state;
  body_state.graph = body.graph;
  body_state.variant = v;
  body_state.animation_state = input.animation;
  body_state.phase = input.phase;
  out.bodies.add_quadruped(body_state);

  RCQ::add_quadruped_shadow(input, body, out);
}

} // namespace Render::Elephant
