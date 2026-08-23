#include "prepare.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "game/core/component.h"
#include "horse_motion.h"
#include "horse_renderer_base.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/quadruped/clip_set.h"
#include "render/creature/quadruped/mount_scale.h"
#include "render/creature/quadruped/quadruped_prepare.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/quadruped/runtime_context.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/math/creature_math_utils.h"
#include "render/submitter.h"
#include "scene/camera.h"

namespace Render::Horse {

namespace {

constexpr Render::Creature::Quadruped::ClipSet k_horse_clips{0U, 1U, 2U, 3U, 4U, 5U};
constexpr float k_ground_clearance_epsilon = 1.0e-5F;

auto default_full_horse_request_seed(const Render::GL::DrawContext& ctx) noexcept
    -> std::uint32_t {
  if (ctx.entity == nullptr) {
    return 0U;
  }
  return Render::Creature::stable_entity_seed(ctx.entity->get_id());
}

auto horse_state_for_motion(const Render::GL::HorseMotionSample& motion,
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
  return Render::Creature::animation_state_for_movement(
      Render::GL::movement_animation_for_horse_gait(motion.playback_gait_type));
}

auto horse_clip_for_motion(const Render::GL::HorseMotionSample& motion) noexcept
    -> std::uint16_t {
  return Render::Creature::Quadruped::clip_for_motion(
      k_horse_clips, motion.playback_gait_type, motion.is_fighting);
}

void ground_horse_model(const Game::Map::TerrainService& terrain,
                        QMatrix4x4& model,
                        std::uint16_t clip_id,
                        float phase) noexcept {
  float const y_scale = model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  float const contact_y =
      Render::Creature::Pipeline::horse_clip_contact_y(clip_id, phase).value_or(0.0F);
  Render::Creature::Pipeline::ground_model_contact_to_surface(
      terrain, model, contact_y, y_scale);
  auto const grounded_origin = Render::Creature::Pipeline::model_world_origin(model);
  Render::Creature::Pipeline::set_model_world_y(
      model, grounded_origin.y() + k_ground_clearance_epsilon);
}

} // namespace

auto grounded_horse_world(const Render::GL::DrawContext& ctx,
                          const Render::GL::HorseMotionSample& motion) noexcept
    -> QMatrix4x4 {
  QMatrix4x4 world = ctx.model;
  ground_horse_model(ctx.world_view.terrain_or_empty(),
                     world,
                     horse_clip_for_motion(motion),
                     motion.phase);
  return world;
}

} // namespace Render::Horse

namespace Render::GL {

namespace {

auto horse_stats() noexcept -> HorseRenderStats& {
  return Render::Creature::Quadruped::current_quadruped_runtime_context().horse;
}

} // namespace

auto get_horse_render_stats() -> const HorseRenderStats& {
  return horse_stats();
}

void reset_horse_render_stats() {
  horse_stats().reset();
}

void HorseRendererBase::render(const DrawContext& ctx,
                               const AnimationInputs& anim,
                               const HumanoidAnimationContext& rider_ctx,
                               HorseProfile& profile,
                               const HorseMotionSample* shared_motion,
                               ISubmitter& out,
                               Render::Creature::CreatureLOD lod) const {

  const bool prewarming = ctx.template_prewarm;
  DrawContext const render_ctx =
      prewarming ? Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx) : ctx;

  Render::Creature::CreatureLOD effective_lod = lod;
  if (render_ctx.force_quadruped_lod) {
    effective_lod = render_ctx.forced_quadruped_lod;
  } else if (prewarming) {
    effective_lod = Render::Creature::CreatureLOD::Minimal;
  }

  ++horse_stats().total;

  if (effective_lod == Render::Creature::CreatureLOD::Culled) {
    ++horse_stats().skipped_lod;
    return;
  }

  ++horse_stats().rendered;
  switch (effective_lod) {
  case Render::Creature::CreatureLOD::Full:
    ++horse_stats().lod_full;
    break;
  case Render::Creature::CreatureLOD::Minimal:
    ++horse_stats().lod_minimal;
    break;
  case Render::Creature::CreatureLOD::Culled:
    break;
  }

  Render::Horse::HorsePreparation prep;
  Render::Horse::prepare_horse_render(
      *this, render_ctx, anim, rider_ctx, profile, shared_motion, effective_lod, prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
}

void HorseRendererBase::render(const DrawContext& ctx,
                               const AnimationInputs& anim,
                               const HumanoidAnimationContext& rider_ctx,
                               HorseProfile& profile,
                               const HorseMotionSample* shared_motion,
                               ISubmitter& out) const {
  render(ctx,
         anim,
         rider_ctx,
         profile,
         shared_motion,
         out,
         Render::Creature::CreatureLOD::Full);
}

} // namespace Render::GL

namespace Render::Horse {

void prepare_horse_impl(const Render::GL::HorseRendererBase& owner,
                        const Render::GL::DrawContext& ctx,
                        const Render::GL::AnimationInputs& anim,
                        const Render::GL::HumanoidAnimationContext& rider_ctx,
                        Render::GL::HorseProfile& profile,
                        const Render::GL::HorseMotionSample* shared_motion,
                        HorsePreparation& out,
                        Render::Creature::CreatureLOD lod,
                        std::uint32_t request_seed,
                        const QMatrix4x4* shared_grounded_world) {
  using Render::GL::HorseMotionSample;
  using Render::GL::HorseVariant;
  const HorseVariant& v = profile.variant;

  HorseMotionSample const motion =
      (shared_motion != nullptr)
          ? *shared_motion
          : evaluate_horse_motion(
                profile,
                anim,
                rider_ctx,
                Engine::Core::get_or_add_component<
                    Render::Creature::HorseAnimationStateComponent>(ctx.entity),
                Render::Creature::Quadruped::mount_model_scale(ctx.world, ctx.entity));
  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = (shared_grounded_world != nullptr)
                        ? *shared_grounded_world
                        : grounded_horse_world(ctx, motion);

  namespace RCP = Render::Creature::Pipeline;
  namespace RCQ = Render::Creature::Quadruped;

  QVector3D const horse_world_pos = RCP::model_world_origin(horse_ctx.model);
  const float horse_y_scale =
      horse_ctx.model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  const float horse_contact_y =
      RCP::horse_clip_contact_y(horse_clip_for_motion(motion), motion.phase)
          .value_or(0.0F);

  RCQ::QuadrupedRuntimeInput input{};
  input.ctx = &horse_ctx;
  input.anim = &anim;
  input.spec = &owner.visual_spec();
  input.kind = RCP::CreatureKind::Horse;
  input.lod = lod;
  input.animation = horse_state_for_motion(motion, anim);
  input.phase = (anim.is_dying || anim.is_dead) ? anim.death_progress : motion.phase;
  input.world = horse_ctx.model;
  input.seed = request_seed;
  input.surface_world_y = horse_world_pos.y() - k_ground_clearance_epsilon +
                          horse_contact_y * horse_y_scale;
  input.surface_height_valid = true;
  input.shadow_intensity_scale = (anim.is_dying || anim.is_dead) ? 0.45F : 1.0F;

  auto const body = RCQ::build_quadruped_body(input);

  RCP::PreparedHorseBodyState body_state;
  body_state.graph = body.graph;
  body_state.variant = v;
  body_state.animation_state = input.animation;
  body_state.phase = input.phase;
  out.bodies.add_quadruped(body_state);

  RCQ::add_quadruped_shadow(input, body, out);
}

void prepare_horse_render(const Render::GL::HorseRendererBase& owner,
                          const Render::GL::DrawContext& ctx,
                          const Render::GL::AnimationInputs& anim,
                          const Render::GL::HumanoidAnimationContext& rider_ctx,
                          Render::GL::HorseProfile& profile,
                          const Render::GL::HorseMotionSample* shared_motion,
                          Render::Creature::CreatureLOD lod,
                          HorsePreparation& out,
                          std::optional<std::uint32_t> request_seed,
                          const QMatrix4x4* shared_grounded_world) {
  if (lod == Render::Creature::CreatureLOD::Culled) {
    return;
  }
  prepare_horse_impl(owner,
                     ctx,
                     anim,
                     rider_ctx,
                     profile,
                     shared_motion,
                     out,
                     lod,
                     request_seed.value_or(default_full_horse_request_seed(ctx)),
                     shared_grounded_world);
}

} // namespace Render::Horse
