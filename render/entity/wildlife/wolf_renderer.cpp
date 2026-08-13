#include "wolf_renderer.h"

#include <QVector3D>

#include <cstdint>
#include <string>

#include "render/creature/animation_state_components.h"
#include "render/entity/registry.h"
#include "render/wildlife/wildlife_prepare.h"
#include "render/wildlife/wolf_spec.h"
#include "wildlife_draw_state.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_top_speed = 4.6F;
constexpr float k_run_threshold = 0.55F;
constexpr float k_walk_threshold = 0.02F;
constexpr float k_idle_period_seconds = 96.0F / 24.0F;
constexpr float k_crouch_period_seconds = 72.0F / 24.0F;

auto resolve_variant(const DrawState& state) -> Render::GL::WildlifeVariant {
  float const morph = hash_unit_float(state.seed, 31U);
  QVector3D base = state.coat;
  if (morph < 0.30F) {
    base = mixed(base, QVector3D(0.36F, 0.28F, 0.19F), 0.55F);
  } else if (morph > 0.86F) {
    base = mixed(base, QVector3D(0.22F, 0.20F, 0.20F), 0.55F);
  }

  Render::GL::WildlifeVariant variant;
  variant.roles[Render::Wildlife::k_wolf_role_fur - 1U] = base;

  variant.roles[Render::Wildlife::k_wolf_role_saddle - 1U] =
      mixed(base, QVector3D(0.18F, 0.17F, 0.17F), 0.62F);
  variant.roles[Render::Wildlife::k_wolf_role_pale - 1U] =
      mixed(base, QVector3D(0.78F, 0.74F, 0.66F), 0.44F);
  variant.roles[Render::Wildlife::k_wolf_role_cream - 1U] =
      mixed(base, QVector3D(0.85F, 0.81F, 0.72F), 0.40F);
  variant.roles[Render::Wildlife::k_wolf_role_limb - 1U] =
      mixed(base, QVector3D(0.24F, 0.21F, 0.19F), 0.42F);
  variant.roles[Render::Wildlife::k_wolf_role_paw - 1U] =
      mixed(base, QVector3D(0.22F, 0.20F, 0.18F), 0.58F);
  variant.roles[Render::Wildlife::k_wolf_role_nose - 1U] = {0.07F, 0.065F, 0.07F};
  variant.roles[Render::Wildlife::k_wolf_role_eye - 1U] = {0.09F, 0.07F, 0.05F};
  variant.roles[Render::Wildlife::k_wolf_role_iris - 1U] = {0.86F, 0.64F, 0.16F};
  variant.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_wolf_role_count);
  return variant;
}

auto resolve_gait(const DrawState& state,
                  float gait_ratio) -> Render::Wildlife::WolfGait {
  bool const stalking = state.behavior == Game::Wildlife::Behavior::Stalk;
  if (gait_ratio > k_run_threshold) {
    return Render::Wildlife::WolfGait::Run;
  }
  if (gait_ratio <= k_walk_threshold) {

    return Render::Wildlife::WolfGait::Stand;
  }
  if (stalking) {
    return Render::Wildlife::WolfGait::Stalk;
  }
  return Render::Wildlife::WolfGait::Walk;
}

auto state_for_gait(const DrawState& state, Render::Wildlife::WolfGait gait)
    -> Render::Creature::AnimationStateId {
  switch (gait) {
  case Render::Wildlife::WolfGait::Run:
    return Render::Creature::AnimationStateId::Run;
  case Render::Wildlife::WolfGait::Walk:
    return Render::Creature::AnimationStateId::Walk;
  case Render::Wildlife::WolfGait::Stalk:
    return Render::Creature::AnimationStateId::Hold;
  case Render::Wildlife::WolfGait::Stand:
    break;
  }

  return state.behavior == Game::Wildlife::Behavior::Stalk || state.alert
             ? Render::Creature::AnimationStateId::WildlifeTense
             : Render::Creature::AnimationStateId::Idle;
}

void draw_wolf(const DrawContext& ctx, ISubmitter& out) {
  const DrawState state = resolve_draw_state(ctx, k_top_speed);

  float const speed = gait_speed(state);
  float const gait_ratio = std::clamp(speed / k_top_speed, 0.0F, 1.0F);
  const Render::Wildlife::WolfGait gait = resolve_gait(state, gait_ratio);

  Render::Wildlife::WildlifeRenderInputs inputs;
  inputs.kind = Render::Creature::Pipeline::CreatureKind::Wolf;
  inputs.variant = resolve_variant(state);

  if (state.dead) {
    inputs.state = Render::Creature::AnimationStateId::Dead;
    inputs.phase = 1.0F;
  } else if (state.death_progress >= 0.0F) {
    inputs.state = Render::Creature::AnimationStateId::Die;
    inputs.phase = action_phase(state, state.death_progress);
  } else if (state.bite_progress >= 0.0F) {
    inputs.state = Render::Creature::AnimationStateId::AttackMelee;
    inputs.phase = action_phase(state, state.bite_progress);
  } else {
    inputs.state = state_for_gait(state, gait);
    switch (inputs.state) {
    case Render::Creature::AnimationStateId::Idle:
      inputs.phase = ambient_phase(state, k_idle_period_seconds);
      break;
    case Render::Creature::AnimationStateId::WildlifeTense:
      inputs.phase = ambient_phase(state, k_crouch_period_seconds);
      break;
    default:
      inputs.phase = gait_phase(state, Render::Wildlife::wolf_gait_advance(gait));
      break;
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

void register_wolf_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(std::string("wildlife/wolf"), &draw_wolf);
}

} // namespace Render::GL::Wildlife
