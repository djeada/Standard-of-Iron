#include "render/creature/elephant/prepare.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/core/component.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/creature/elephant/elephant_body_state.h"
#include "render/creature/elephant/elephant_components.h"
#include "render/creature/elephant/elephant_motion.h"
#include "render/creature/elephant/elephant_render_batch.h"
#include "render/creature/elephant/elephant_renderer_base.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/movement_animation.h"
#include "render/creature/pipeline/corpse_sink.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/quadruped/mount_scale.h"
#include "render/creature/quadruped/quadruped_prepare.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/quadruped/runtime_context.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"
#include "render/frame/terrain_contact.h"
#include "render/geom/primitives.h"
#include "render/math/creature_math_utils.h"
#include "scene/camera.h"

namespace Render::Elephant {

namespace {

constexpr float k_elephant_max_ground_pitch_degrees = 16.0F;

enum HowdahSlot : std::uint8_t {
  k_howdah_cloth = 0,
  k_howdah_trim = 1,
  k_howdah_wood = 2,
  k_howdah_wood_dark = 3,
  k_howdah_bronze = 4,
  k_howdah_slot_count = 5,
};

constexpr float k_howdah_half_width = 0.30F;
constexpr float k_howdah_half_length = 0.40F;
constexpr float k_howdah_wall_height = 0.30F;
constexpr float k_howdah_floor_y = 0.05F;
constexpr float k_howdah_cloth_half_width = 0.37F;
constexpr float k_howdah_cloth_half_length = 0.50F;
constexpr float k_howdah_flap_drop = 0.46F;
constexpr float k_howdah_flap_splay_degrees = 16.0F;
constexpr float k_howdah_topple_roll_degrees = 82.0F;

auto howdah_archetype() -> const Render::GL::RenderArchetype& {
  static const Render::GL::RenderArchetype k_archetype = [] {
    using Render::GL::box_local_model;
    Render::GL::RenderArchetypeBuilder builder{"elephant_howdah"};
    auto* cube = Render::GL::get_unit_cube();

    builder.add_palette_box(
        QVector3D(0.0F, -0.02F, 0.0F),
        QVector3D(k_howdah_cloth_half_width, 0.030F, k_howdah_cloth_half_length),
        k_howdah_cloth);
    for (float const side : {-1.0F, 1.0F}) {
      QMatrix4x4 hinge;
      hinge.translate(side * k_howdah_cloth_half_width, -0.02F, 0.0F);
      hinge.rotate(side * k_howdah_flap_splay_degrees, 0.0F, 0.0F, 1.0F);
      builder.add_palette_mesh(
          cube,
          hinge * box_local_model(QVector3D(0.0F, -k_howdah_flap_drop * 0.5F, 0.0F),
                                  QVector3D(0.022F,
                                            k_howdah_flap_drop * 0.5F,
                                            k_howdah_cloth_half_length * 0.90F)),
          k_howdah_cloth);
      builder.add_palette_mesh(
          cube,
          hinge * box_local_model(
                      QVector3D(0.0F, -k_howdah_flap_drop, 0.0F),
                      QVector3D(0.030F, 0.035F, k_howdah_cloth_half_length * 0.92F)),
          k_howdah_trim);
      for (float const z : {-0.24F, 0.0F, 0.24F}) {
        builder.add_palette_mesh(
            cube,
            hinge * box_local_model(QVector3D(0.004F, -k_howdah_flap_drop * 0.55F, z),
                                    QVector3D(0.026F, 0.060F, 0.060F)),
            k_howdah_trim);
      }
    }

    float const wall_y = k_howdah_floor_y + k_howdah_wall_height * 0.5F;
    builder.add_palette_box(
        QVector3D(0.0F, k_howdah_floor_y, 0.0F),
        QVector3D(k_howdah_half_width + 0.03F, 0.030F, k_howdah_half_length + 0.03F),
        k_howdah_wood_dark);
    for (float const side : {-1.0F, 1.0F}) {
      builder.add_palette_box(
          QVector3D(side * k_howdah_half_width, wall_y, 0.0F),
          QVector3D(0.026F, k_howdah_wall_height * 0.5F, k_howdah_half_length),
          k_howdah_wood);
      builder.add_palette_box(
          QVector3D(0.0F, wall_y, side * k_howdah_half_length),
          QVector3D(k_howdah_half_width, k_howdah_wall_height * 0.5F, 0.026F),
          k_howdah_wood);
      builder.add_palette_box(QVector3D(side * k_howdah_half_width,
                                        k_howdah_floor_y + k_howdah_wall_height,
                                        0.0F),
                              QVector3D(0.034F, 0.016F, k_howdah_half_length + 0.02F),
                              k_howdah_bronze);
      builder.add_palette_box(QVector3D(0.0F,
                                        k_howdah_floor_y + k_howdah_wall_height,
                                        side * k_howdah_half_length),
                              QVector3D(k_howdah_half_width + 0.02F, 0.016F, 0.034F),
                              k_howdah_bronze);
      for (float const z : {-0.28F, 0.0F, 0.28F}) {
        builder.add_palette_box(
            QVector3D(side * k_howdah_half_width,
                      k_howdah_floor_y + k_howdah_wall_height + 0.050F,
                      z),
            QVector3D(0.028F, 0.045F, 0.070F),
            k_howdah_wood);
      }
      for (float const x : {-0.17F, 0.17F}) {
        builder.add_palette_box(
            QVector3D(x,
                      k_howdah_floor_y + k_howdah_wall_height + 0.050F,
                      side * k_howdah_half_length),
            QVector3D(0.070F, 0.045F, 0.028F),
            k_howdah_wood);
      }
      for (float const z : {-0.19F, 0.19F}) {
        QVector3D const boss(side * (k_howdah_half_width + 0.030F), wall_y, z);
        builder.add_palette_cylinder(
            boss, boss + QVector3D(side * 0.022F, 0.0F, 0.0F), 0.105F, k_howdah_bronze);
        builder.add_palette_cylinder(boss + QVector3D(side * 0.020F, 0.0F, 0.0F),
                                     boss + QVector3D(side * 0.036F, 0.0F, 0.0F),
                                     0.045F,
                                     k_howdah_cloth);
      }
      for (float const corner : {-1.0F, 1.0F}) {
        QVector3D const foot(side * k_howdah_half_width,
                             k_howdah_floor_y,
                             corner * k_howdah_half_length);
        builder.add_palette_cylinder(
            foot,
            foot + QVector3D(0.0F, k_howdah_wall_height + 0.11F, 0.0F),
            0.034F,
            k_howdah_wood_dark);
      }
    }

    QVector3D const pole_foot(
        k_howdah_half_width, k_howdah_floor_y, -k_howdah_half_length);
    QVector3D const pole_top = pole_foot + QVector3D(0.0F, 0.98F, 0.0F);
    builder.add_palette_cylinder(pole_foot, pole_top, 0.016F, k_howdah_wood_dark);
    builder.add_palette_box(pole_top + QVector3D(0.0F, -0.11F, -0.15F),
                            QVector3D(0.006F, 0.095F, 0.150F),
                            k_howdah_cloth);
    builder.add_palette_box(pole_top + QVector3D(0.0F, -0.11F, -0.30F),
                            QVector3D(0.007F, 0.100F, 0.016F),
                            k_howdah_trim);
    return std::move(builder).build();
  }();
  return k_archetype;
}

auto howdah_world(const QMatrix4x4& elephant_model,
                  const Render::GL::HowdahAttachmentFrame& frame,
                  const Render::GL::AnimationInputs& anim) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.seat_right, 0.0F));
  local.setColumn(1, QVector4D(frame.seat_up, 0.0F));
  local.setColumn(2, QVector4D(frame.seat_forward, 0.0F));
  local.setColumn(3, QVector4D(frame.howdah_center, 1.0F));
  if (!anim.is_dying && !anim.is_dead) {
    return elephant_model * local;
  }
  float const fall = anim.is_dead ? 1.0F : std::clamp(anim.death_progress, 0.0F, 1.0F);
  float const eased = fall * fall * (3.0F - 2.0F * fall);
  QMatrix4x4 topple;
  topple.translate(
      eased * 0.95F, -eased * std::max(frame.howdah_center.y() - 0.34F, 0.0F), 0.0F);
  topple.translate(frame.howdah_center);
  topple.rotate(-eased * k_howdah_topple_roll_degrees, 0.0F, 0.0F, 1.0F);
  topple.translate(-frame.howdah_center);
  return elephant_model * topple * local;
}

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
  if (motion.is_pivoting) {
    return Render::Creature::AnimationStateId::Walk;
  }
  return Render::Creature::animation_state_for_movement(motion.movement_state);
}

auto elephant_blend_seconds(Render::Creature::AnimationStateId from,
                            Render::Creature::AnimationStateId to) noexcept -> float {
  using Render::Creature::AnimationStateId;
  if (to == AnimationStateId::Die || to == AnimationStateId::Dead) {
    return from == AnimationStateId::Die ? 0.0F : 0.18F;
  }
  if (to == AnimationStateId::AttackMelee || from == AnimationStateId::AttackMelee) {
    return 0.20F;
  }
  if (to == AnimationStateId::Idle || from == AnimationStateId::Idle) {
    return 0.35F;
  }
  return 0.28F;
}

struct ElephantClipBlend {
  Render::Creature::AnimationStateId outgoing_state{
      Render::Creature::AnimationStateId::Idle};
  float outgoing_phase{0.0F};
  float outgoing_weight{0.0F};
};

auto resolve_elephant_clip_blend(
    Render::Creature::ElephantAnimationStateComponent* state,
    Render::Creature::AnimationStateId incoming,
    float incoming_phase,
    float time) noexcept -> ElephantClipBlend {
  if (state == nullptr) {
    return {};
  }
  bool const continuous =
      state->clip_valid && time >= state->clip_time && time - state->clip_time <= 0.5F;
  if (!continuous) {
    state->clip_valid = true;
    state->clip_state = incoming;
    state->clip_phase = incoming_phase;
    state->clip_time = time;
    state->blend_started_at = -1.0F;
    return {};
  }
  if (incoming != state->clip_state) {
    float const duration = elephant_blend_seconds(state->clip_state, incoming);
    if (duration > 0.0F) {
      state->outgoing_state = state->clip_state;
      state->outgoing_phase = state->clip_phase;
      state->blend_started_at = time;
      state->blend_duration = duration;
    }
    state->clip_state = incoming;
  }
  state->clip_phase = incoming_phase;
  state->clip_time = time;
  if (state->blend_started_at < 0.0F || state->blend_duration <= 0.0F) {
    return {};
  }
  float const remaining =
      1.0F -
      std::clamp((time - state->blend_started_at) / state->blend_duration, 0.0F, 1.0F);
  if (remaining <= 0.0F) {
    state->blend_started_at = -1.0F;
    return {};
  }
  return {.outgoing_state = state->outgoing_state,
          .outgoing_phase = state->outgoing_phase,
          .outgoing_weight = remaining * remaining * (3.0F - 2.0F * remaining)};
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
  QMatrix4x4 howdah_model;
  Render::Elephant::prepare_elephant_render(*this,
                                            render_ctx,
                                            anim,
                                            profile,
                                            shared_howdah,
                                            shared_motion,
                                            effective_lod,
                                            prep,
                                            &howdah_model);
  Render::Creature::Pipeline::submit_preparation(prep, out);
  if (!ctx.template_prewarm) {
    Render::Elephant::submit_howdah(render_ctx, profile, howdah_model, out);
  }
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

void submit_howdah(const Render::GL::DrawContext& ctx,
                   const Render::GL::ElephantProfile& profile,
                   const QMatrix4x4& world,
                   Render::GL::ISubmitter& out) {
  QVector3D team = profile.variant.howdah_fabric_color;
  if (ctx.entity != nullptr && ctx.world != nullptr &&
      ctx.world->has<Engine::Core::UnitComponent>(ctx.entity->get_id())) {
    team = Render::entity_color(*ctx.entity);
  }
  std::array<QVector3D, k_howdah_slot_count> const palette{
      team * 0.86F + QVector3D(0.04F, 0.03F, 0.03F),
      QVector3D(0.84F, 0.68F, 0.30F),
      profile.variant.howdah_wood_color,
      profile.variant.howdah_wood_color * 0.62F,
      profile.variant.howdah_metal_color,
  };
  Render::GL::RenderInstance instance;
  instance.archetype = &howdah_archetype();
  instance.world = world;
  instance.palette = palette;
  Render::GL::submit_render_instance(out, instance);
}

void prepare_elephant_render(const Render::GL::ElephantRendererBase& owner,
                             const Render::GL::DrawContext& ctx,
                             const Render::GL::AnimationInputs& anim,
                             Render::GL::ElephantProfile& profile,
                             const Render::GL::HowdahAttachmentFrame* shared_howdah,
                             const Render::GL::ElephantMotionSample* shared_motion,
                             Render::Creature::CreatureLOD lod,
                             ElephantPreparation& out,
                             QMatrix4x4* out_howdah_world) {
  if (lod == Render::Creature::CreatureLOD::Culled) {
    return;
  }

  using Render::GL::ElephantMotionSample;
  using Render::GL::ElephantVariant;
  using Render::GL::HowdahAttachmentFrame;

  const ElephantVariant& v = profile.variant;
  auto* animation_state = Engine::Core::get_or_add_component<
      Render::Creature::ElephantAnimationStateComponent>(ctx.entity);
  ElephantMotionSample const motion =
      (shared_motion != nullptr)
          ? *shared_motion
          : evaluate_elephant_motion(
                profile,
                anim,
                animation_state,
                Render::Creature::Quadruped::mount_model_scale(ctx.world, ctx.entity),
                Animation::resolve_soldier_individuality({
                    .soldier_seed =
                        ctx.entity != nullptr
                            ? Render::Creature::stable_entity_seed(ctx.entity->get_id())
                            : 0U,
                }),
                std::atan2(ctx.model.column(2).x(), ctx.model.column(2).z()) * 180.0F /
                    3.14159265F);

  HowdahAttachmentFrame const howdah =
      (shared_howdah != nullptr) ? *shared_howdah : motion.howdah;
  Render::GL::DrawContext elephant_ctx = ctx;
  elephant_ctx.model = ctx.model;
  elephant_ctx.model.translate(howdah.ground_offset);
  const float elephant_surface_world_y =
      Render::Creature::Pipeline::ground_model_to_terrain(
          ctx.world_view.terrain_or_empty(), elephant_ctx.model);
  Render::pitch_model_to_ground(elephant_ctx.model,
                                ctx.world_view.terrain_or_empty(),
                                k_elephant_max_ground_pitch_degrees);

  if (anim.death_sink_progress > 0.0F) {
    QMatrix4x4 sink;
    sink.translate(0.0F,
                   Render::Creature::Pipeline::corpse_sink_offset(
                       Render::Creature::Pipeline::CreatureKind::Elephant,
                       anim.death_sink_progress),
                   0.0F);
    elephant_ctx.model = sink * elephant_ctx.model;
  }

  if (out_howdah_world != nullptr) {
    *out_howdah_world = howdah_world(elephant_ctx.model, howdah, anim);
  }

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
  input.shadow_intensity_scale = ((anim.is_dying || anim.is_dead) ? 0.45F : 1.0F) *
                                 RCP::corpse_shadow_scale(anim.death_sink_progress);

  auto const body = RCQ::build_quadruped_body(input);

  RCP::PreparedElephantBodyState body_state;
  body_state.graph = body.graph;
  body_state.variant = v;
  body_state.animation_state = input.animation;
  body_state.phase = input.phase;
  auto const blend = resolve_elephant_clip_blend(
      animation_state, input.animation, input.phase, anim.time);
  body_state.outgoing_state = blend.outgoing_state;
  body_state.outgoing_phase = blend.outgoing_phase;
  body_state.outgoing_weight = blend.outgoing_weight;
  add_quadruped(out.bodies, body_state);

  RCQ::add_quadruped_shadow(input, body, out);
}

} // namespace Render::Elephant
