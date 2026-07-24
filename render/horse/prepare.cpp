#include "prepare.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <optional>

#include "../../game/core/component.h"
#include "../creature/animation_state_components.h"
#include "../creature/pipeline/creature_prepared_state.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/quadruped/clip_set.h"
#include "../creature/quadruped/render_stats.h"
#include "../gl/camera.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../submitter.h"
#include "horse_motion.h"
#include "horse_renderer_base.h"

namespace Render::Horse {

namespace {

constexpr Render::Creature::Quadruped::ClipSet k_horse_clips{0U, 1U, 2U, 3U, 4U, 5U};
constexpr float k_ground_clearance_epsilon = 1.0e-5F;

auto default_full_horse_request_seed(const Render::GL::DrawContext& ctx) noexcept
    -> std::uint32_t {
  if (ctx.entity == nullptr) {
    return 0U;
  }
  return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ctx.entity) &
                                    0xFFFFFFFFU);
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

void ground_horse_model(QMatrix4x4& model,
                        std::uint16_t clip_id,
                        float phase) noexcept {
  float const y_scale = model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  float const contact_y =
      Render::Creature::Pipeline::horse_clip_contact_y(clip_id, phase).value_or(0.0F);
  Render::Creature::Pipeline::ground_model_contact_to_surface(
      model, contact_y, y_scale);
  auto const grounded_origin = Render::Creature::Pipeline::model_world_origin(model);
  Render::Creature::Pipeline::set_model_world_y(
      model, grounded_origin.y() + k_ground_clearance_epsilon);
}

} // namespace

auto grounded_horse_world(const Render::GL::DrawContext& ctx,
                          const Render::GL::HorseMotionSample& motion) noexcept
    -> QMatrix4x4 {
  QMatrix4x4 world = ctx.model;
  ground_horse_model(world, horse_clip_for_motion(motion), motion.phase);
  return world;
}

} // namespace Render::Horse

namespace Render::GL {

static HorseRenderStats s_horseRenderStats;

auto get_horse_render_stats() -> const HorseRenderStats& {
  return s_horseRenderStats;
}

void reset_horse_render_stats() {
  s_horseRenderStats.reset();
}

void HorseRendererBase::render(const DrawContext& ctx,
                               const AnimationInputs& anim,
                               const HumanoidAnimationContext& rider_ctx,
                               HorseProfile& profile,
                               const MountedAttachmentFrame* shared_mount,
                               const HorseMotionSample* shared_motion,
                               ISubmitter& out,
                               HorseLOD lod) const {
  DrawContext const render_ctx =
      ctx.template_prewarm ? Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx)
                           : ctx;

  HorseLOD effective_lod = lod;
  if (ctx.template_prewarm && !render_ctx.force_horse_lod) {
    effective_lod = HorseLOD::Minimal;
  } else if (render_ctx.force_horse_lod) {
    effective_lod = render_ctx.forced_horse_lod;
  }

  ++s_horseRenderStats.total;

  if (effective_lod == HorseLOD::Billboard) {
    ++s_horseRenderStats.skipped_lod;
    return;
  }

  ++s_horseRenderStats.rendered;
  switch (effective_lod) {
  case HorseLOD::Full:
    ++s_horseRenderStats.lod_full;
    break;
  case HorseLOD::Minimal:
    ++s_horseRenderStats.lod_minimal;
    break;
  case HorseLOD::Billboard:
    break;
  }

  Render::Horse::HorsePreparation prep;
  Render::Horse::prepare_horse_render(*this,
                                      render_ctx,
                                      anim,
                                      rider_ctx,
                                      profile,
                                      shared_mount,
                                      shared_motion,
                                      effective_lod,
                                      prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
}

void HorseRendererBase::render(const DrawContext& ctx,
                               const AnimationInputs& anim,
                               const HumanoidAnimationContext& rider_ctx,
                               HorseProfile& profile,
                               const MountedAttachmentFrame* shared_mount,
                               const HorseMotionSample* shared_motion,
                               ISubmitter& out) const {
  render(
      ctx, anim, rider_ctx, profile, shared_mount, shared_motion, out, HorseLOD::Full);
}

} // namespace Render::GL

namespace Render::Horse {

void prepare_horse_impl(const Render::GL::HorseRendererBase& owner,
                        const Render::GL::DrawContext& ctx,
                        const Render::GL::AnimationInputs& anim,
                        const Render::GL::HumanoidAnimationContext& rider_ctx,
                        Render::GL::HorseProfile& profile,
                        const Render::GL::MountedAttachmentFrame* shared_mount,
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
                    Render::Creature::HorseAnimationStateComponent>(ctx.entity));
  (void)shared_mount;
  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = (shared_grounded_world != nullptr)
                        ? *shared_grounded_world
                        : grounded_horse_world(ctx, motion);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &horse_ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = lod;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = owner.visual_spec();
  graph_output.seed = request_seed;
  RCP::PreparedHorseBodyState body_state;
  body_state.graph = graph_output;
  body_state.variant = v;
  body_state.animation_state = horse_state_for_motion(motion, anim);
  body_state.phase =
      (anim.is_dying || anim.is_dead) ? anim.death_progress : motion.phase;
  out.bodies.add_quadruped(body_state);

  QVector3D const horse_world_pos = RCP::model_world_origin(horse_ctx.model);
  const float horse_y_scale =
      horse_ctx.model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  const float horse_contact_y =
      RCP::horse_clip_contact_y(horse_clip_for_motion(motion), motion.phase)
          .value_or(0.0F);
  const float horse_surface_world_y = horse_world_pos.y() - k_ground_clearance_epsilon +
                                      horse_contact_y * horse_y_scale;
  float camera_distance = 0.0F;
  if (horse_ctx.camera != nullptr) {
    camera_distance = (horse_world_pos - horse_ctx.camera->get_position()).length();
  }
  RCP::QuadrupedShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = &horse_ctx;
  shadow_inputs.graph = &graph_output;
  shadow_inputs.world_pos = horse_world_pos;
  shadow_inputs.kind = RCP::CreatureKind::Horse;
  shadow_inputs.lod = lod;
  shadow_inputs.camera_distance = camera_distance;
  shadow_inputs.formation_id = ctx.entity != nullptr ? ctx.entity->get_id() : 0U;
  shadow_inputs.standing_idle = !motion.is_moving && !motion.is_fighting &&
                                !anim.is_attacking && !anim.is_hit_reacting &&
                                !anim.is_dying && !anim.is_dead;
  shadow_inputs.surface_world_y = horse_surface_world_y;
  shadow_inputs.surface_height_valid = true;
  const auto shadow_state = RCP::prepare_quadruped_shadow_state(shadow_inputs);
  if (shadow_state.enabled) {
    if (out.shadow_batch.empty()) {
      out.shadow_batch.init(
          shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
    }
    out.shadow_batch.add(shadow_state.model, shadow_state.alpha, shadow_state.pass);
  }
}

void prepare_horse_render(const Render::GL::HorseRendererBase& owner,
                          const Render::GL::DrawContext& ctx,
                          const Render::GL::AnimationInputs& anim,
                          const Render::GL::HumanoidAnimationContext& rider_ctx,
                          Render::GL::HorseProfile& profile,
                          const Render::GL::MountedAttachmentFrame* shared_mount,
                          const Render::GL::HorseMotionSample* shared_motion,
                          Render::Creature::CreatureLOD lod,
                          HorsePreparation& out,
                          std::optional<std::uint32_t> request_seed,
                          const QMatrix4x4* shared_grounded_world) {
  if (lod == Render::Creature::CreatureLOD::Billboard) {
    return;
  }
  prepare_horse_impl(owner,
                     ctx,
                     anim,
                     rider_ctx,
                     profile,
                     shared_mount,
                     shared_motion,
                     out,
                     lod,
                     request_seed.value_or(default_full_horse_request_seed(ctx)),
                     shared_grounded_world);
}

} // namespace Render::Horse
