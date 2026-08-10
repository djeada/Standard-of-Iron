#include "sheep_renderer.h"

#include <QVector3D>

#include <cstdint>
#include <string>

#include "render/creature/animation_state_components.h"
#include "render/entity/registry.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wildlife_prepare.h"
#include "wildlife_draw_state.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_top_speed = 3.4F;
constexpr float k_walk_threshold = 0.02F;
constexpr float k_idle_period_seconds = 24.0F / 24.0F;
constexpr float k_graze_period_seconds = 120.0F / 24.0F;
constexpr float k_run_threshold = 0.42F;

auto resolve_variant(const DrawState& state) -> Render::GL::WildlifeVariant {
  QVector3D wool = state.coat;
  float const breed = hash_unit_float(state.seed, 17U);
  if (breed > 0.88F) {
    wool = mixed(wool, QVector3D(0.24F, 0.21F, 0.20F), 0.72F);
  } else if (breed > 0.70F) {
    wool = mixed(wool, QVector3D(0.50F, 0.42F, 0.33F), 0.42F);
  } else {
    wool = mixed(wool, QVector3D(0.70F, 0.65F, 0.55F), 0.28F + (breed * 0.46F));
  }

  bool const dark_faced = breed > 0.70F || hash_unit_float(state.seed, 23U) < 0.50F;
  QVector3D const face = dark_faced
                             ? mixed(QVector3D(0.19F, 0.16F, 0.15F),
                                     QVector3D(0.30F, 0.25F, 0.21F),
                                     hash_unit_float(state.seed, 29U))
                             : mixed(wool, QVector3D(0.50F, 0.40F, 0.30F), 0.74F);

  wool = mixed(wool, QVector3D(0.80F, 0.77F, 0.70F), 0.34F);
  QVector3D const shade =
      mixed(tinted(wool, 0.70F), QVector3D(0.34F, 0.30F, 0.26F), 0.22F);

  Render::GL::WildlifeVariant variant;
  variant.roles[Render::Wildlife::k_sheep_role_wool - 1U] = wool;
  variant.roles[Render::Wildlife::k_sheep_role_wool_light - 1U] =
      mixed(tinted(wool, 1.10F), QVector3D(0.97F, 0.95F, 0.89F), 0.16F);
  variant.roles[Render::Wildlife::k_sheep_role_wool_shade - 1U] = shade;
  variant.roles[Render::Wildlife::k_sheep_role_wool_grubby - 1U] =
      mixed(shade, QVector3D(0.46F, 0.39F, 0.28F), 0.60F);
  variant.roles[Render::Wildlife::k_sheep_role_face - 1U] = face;
  variant.roles[Render::Wildlife::k_sheep_role_hoof - 1U] =
      mixed(face, QVector3D(0.15F, 0.13F, 0.12F), 0.62F);
  variant.roles[Render::Wildlife::k_sheep_role_nose - 1U] =
      mixed(face, QVector3D(0.08F, 0.07F, 0.07F), 0.65F);
  variant.roles[Render::Wildlife::k_sheep_role_eye - 1U] = {0.07F, 0.06F, 0.05F};
  variant.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_sheep_role_count);
  return variant;
}

auto resolve_gait(const DrawState& state,
                  float gait_ratio) -> Render::Wildlife::SheepGait {
  if (state.grazing) {
    return Render::Wildlife::SheepGait::Stand;
  }
  if (gait_ratio > k_run_threshold) {
    return Render::Wildlife::SheepGait::Run;
  }
  if (gait_ratio > k_walk_threshold) {
    return Render::Wildlife::SheepGait::Walk;
  }
  return Render::Wildlife::SheepGait::Stand;
}

auto state_for_gait(const DrawState& state, Render::Wildlife::SheepGait gait)
    -> Render::Creature::AnimationStateId {
  switch (gait) {
  case Render::Wildlife::SheepGait::Run:
    return Render::Creature::AnimationStateId::Run;
  case Render::Wildlife::SheepGait::Walk:
    return Render::Creature::AnimationStateId::Walk;
  case Render::Wildlife::SheepGait::Stand:
    break;
  }
  return state.grazing ? Render::Creature::AnimationStateId::Hold
                       : Render::Creature::AnimationStateId::Idle;
}

void draw_sheep(const DrawContext& ctx, ISubmitter& out) {
  const DrawState state = resolve_draw_state(ctx, k_top_speed);

  float const speed = gait_speed(state);
  float const gait_ratio = std::clamp(speed / k_top_speed, 0.0F, 1.0F);
  const Render::Wildlife::SheepGait gait = resolve_gait(state, gait_ratio);

  Render::Wildlife::WildlifeRenderInputs inputs;
  inputs.kind = Render::Creature::Pipeline::CreatureKind::Sheep;
  inputs.variant = resolve_variant(state);

  if (state.dead) {
    inputs.state = Render::Creature::AnimationStateId::Dead;
    inputs.phase = 1.0F;
  } else if (state.death_progress >= 0.0F) {
    inputs.state = Render::Creature::AnimationStateId::Die;
    inputs.phase = action_phase(state, state.death_progress);
  } else {
    inputs.state = state_for_gait(state, gait);
    if (gait == Render::Wildlife::SheepGait::Stand) {
      inputs.phase =
          ambient_phase(state,
                        inputs.state == Render::Creature::AnimationStateId::Hold
                            ? k_graze_period_seconds
                            : k_idle_period_seconds);
    } else {
      inputs.phase = gait_phase(state, Render::Wildlife::sheep_gait_advance(gait));
    }
  }

  const ClipTransition transition =
      resolve_clip_transition(state, inputs.state, inputs.phase);
  inputs.outgoing_state = transition.outgoing;
  inputs.outgoing_phase = transition.phase;
  inputs.outgoing_weight = transition.weight;

  Render::Wildlife::submit_wildlife(ctx, inputs, out);
}

} // namespace

void register_sheep_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(std::string("wildlife/sheep"), &draw_sheep);
}

} // namespace Render::GL::Wildlife
