#include "prepare.h"

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

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <optional>

namespace Render::Horse {

namespace {

constexpr Render::Creature::Quadruped::ClipSet k_horse_clips{0U, 1U, 2U,
                                                             3U, 4U, 5U};
constexpr float k_ground_clearance_epsilon = 1.0e-5F;

auto default_full_horse_request_seed(
    const Render::GL::DrawContext &ctx) noexcept -> std::uint32_t {
  if (ctx.entity == nullptr) {
    return 0U;
  }
  return static_cast<std::uint32_t>(
      reinterpret_cast<std::uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
}

auto horse_state_for_motion(
    const Render::GL::HorseMotionSample &motion) noexcept
    -> Render::Creature::AnimationStateId {
  if (motion.is_fighting) {
    return Render::Creature::AnimationStateId::AttackMelee;
  }
  switch (motion.gait_type) {
  case Render::GL::GaitType::IDLE:
    return Render::Creature::AnimationStateId::Idle;
  case Render::GL::GaitType::WALK:
    return Render::Creature::AnimationStateId::Walk;
  case Render::GL::GaitType::TROT:
  case Render::GL::GaitType::CANTER:
  case Render::GL::GaitType::GALLOP:
    return Render::Creature::AnimationStateId::Run;
  }
  return Render::Creature::AnimationStateId::Idle;
}

auto horse_clip_for_motion(const Render::GL::HorseMotionSample &motion) noexcept
    -> std::uint16_t {
  return Render::Creature::Quadruped::clip_for_motion(
      k_horse_clips, motion.gait_type, motion.is_fighting);
}

void ground_horse_model(QMatrix4x4 &model, std::uint16_t clip_id,
                        float phase) noexcept {
  float const y_scale = model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  float const contact_y =
      Render::Creature::Pipeline::horse_clip_contact_y(clip_id, phase)
          .value_or(0.0F);
  Render::Creature::Pipeline::ground_model_contact_to_surface(model, contact_y,
                                                              y_scale);
  auto const grounded_origin =
      Render::Creature::Pipeline::model_world_origin(model);
  Render::Creature::Pipeline::set_model_world_y(
      model, grounded_origin.y() + k_ground_clearance_epsilon);
}

} // namespace

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
  DrawContext render_ctx =
      ctx.template_prewarm
          ? Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx)
          : ctx;

  ++s_horseRenderStats.total;

  if (lod == HorseLOD::Billboard) {
    ++s_horseRenderStats.skipped_lod;
    return;
  }

  ++s_horseRenderStats.rendered;
  switch (lod) {
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
  Render::Horse::prepare_horse_render(*this, render_ctx, anim, rider_ctx,
                                      profile, shared_mount, shared_motion, lod,
                                      prep);
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

void prepare_horse_impl(const Render::GL::HorseRendererBase &owner,
                        const Render::GL::DrawContext &ctx,
                        const Render::GL::AnimationInputs &anim,
                        const Render::GL::HumanoidAnimationContext &rider_ctx,
                        Render::GL::HorseProfile &profile,
                        const Render::GL::MountedAttachmentFrame *shared_mount,
                        const Render::GL::HorseMotionSample *shared_motion,
                        HorsePreparation &out,
                        Render::Creature::CreatureLOD lod,
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
  std::uint16_t const horse_clip = horse_clip_for_motion(motion);

  Render::GL::DrawContext horse_ctx = ctx;
  horse_ctx.model = ctx.model;
  ground_horse_model(horse_ctx.model, horse_clip, motion.phase);

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
  body_state.animation_state = horse_state_for_motion(motion);
  body_state.phase = motion.phase;
  out.bodies.add_quadruped(body_state);

  // Shadow
  QVector3D const horse_world_pos = RCP::model_world_origin(horse_ctx.model);
  float camera_distance = 0.0F;
  if (horse_ctx.camera != nullptr) {
    camera_distance =
        (horse_world_pos - horse_ctx.camera->get_position()).length();
  }
  RCP::QuadrupedShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = &horse_ctx;
  shadow_inputs.graph = &graph_output;
  shadow_inputs.world_pos = horse_world_pos;
  shadow_inputs.kind = RCP::CreatureKind::Horse;
  shadow_inputs.lod = lod;
  shadow_inputs.camera_distance = camera_distance;
  const auto shadow_state = RCP::prepare_quadruped_shadow_state(shadow_inputs);
  if (shadow_state.enabled) {
    if (out.shadow_batch.empty()) {
      out.shadow_batch.init(shadow_state.shader, shadow_state.mesh,
                            shadow_state.light_dir);
    }
    out.shadow_batch.add(shadow_state.model, shadow_state.alpha,
                         shadow_state.pass);
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
    std::optional<std::uint32_t> request_seed) {
  if (lod == Render::Creature::CreatureLOD::Billboard) {
    return;
  }
  prepare_horse_impl(
      owner, ctx, anim, rider_ctx, profile, shared_mount, shared_motion, out,
      lod, request_seed.value_or(default_full_horse_request_seed(ctx)));
}

} // namespace Render::Horse
