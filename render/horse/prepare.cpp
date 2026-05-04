#include "prepare.h"

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

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <optional>

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

constexpr float k_ground_clearance_epsilon = 1.0e-5F;
constexpr float k_shadow_size_horse = 0.35F;
constexpr float k_shadow_width_scale_horse = 1.15F;
constexpr float k_shadow_depth_scale_horse = 1.65F;

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

  namespace RCP = Render::Creature::Pipeline;
  RCP::HorseMotionStateInputs motion_inputs{};
  motion_inputs.ctx = &ctx;
  motion_inputs.anim = &anim;
  motion_inputs.rider_ctx = &rider_ctx;
  motion_inputs.profile = &profile;
  motion_inputs.shared_motion = shared_motion;
  const auto motion_state = RCP::resolve_horse_motion_state(motion_inputs);
  (void)shared_mount;

  RCP::QuadrupedFrameStateInputs frame_inputs{};
  frame_inputs.ctx = &ctx;
  frame_inputs.anim = &anim;
  frame_inputs.spec = owner.visual_spec();
  frame_inputs.lod = lod;
  frame_inputs.seed = request_seed;
  frame_inputs.contact_y =
      RCP::horse_clip_contact_y(motion_state.clip_id, motion_state.motion.phase)
          .value_or(0.0F);
  frame_inputs.ground_clearance = k_ground_clearance_epsilon;
  frame_inputs.use_contact_grounding = true;
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
  shadow_inputs.shadow_size = k_shadow_size_horse;
  shadow_inputs.width_scale = k_shadow_width_scale_horse;
  shadow_inputs.depth_scale = k_shadow_depth_scale_horse;
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

  RCP::PreparedHorseBodyState body_state;
  body_state.graph = frame_state.graph;
  body_state.variant = v;
  body_state.animation_state = motion_state.animation_state;
  body_state.phase = motion_state.motion.phase;
  out.bodies.add_quadruped(body_state);
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
