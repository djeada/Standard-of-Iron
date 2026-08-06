#include "wolf_renderer.h"

#include <QVector3D>

#include <cstdint>
#include <string>

#include "../../creature/animation_state_components.h"
#include "../../wildlife/wildlife_prepare.h"
#include "../../wildlife/wolf_spec.h"
#include "../registry.h"
#include "wildlife_draw_state.h"

namespace Render::GL::Wildlife {

namespace {

// The catalogue speed for wildlife/wolf. Ratios are taken against it so the gait
// thresholds below mean the same thing they read as.
constexpr float k_top_speed = 3.1F;
constexpr float k_run_threshold = 0.30F;

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
  // The dark roles need a floor. Taken all the way down, a black-morph wolf's saddle
  // and paws land under the scene's ambient and punch holes in its own silhouette.
  variant.roles[Render::Wildlife::k_wolf_role_saddle - 1U] =
      mixed(base, QVector3D(0.24F, 0.21F, 0.18F), 0.55F);
  variant.roles[Render::Wildlife::k_wolf_role_pale - 1U] =
      mixed(base, QVector3D(0.84F, 0.78F, 0.66F), 0.42F);
  variant.roles[Render::Wildlife::k_wolf_role_cream - 1U] =
      mixed(base, QVector3D(0.90F, 0.85F, 0.74F), 0.38F);
  variant.roles[Render::Wildlife::k_wolf_role_limb - 1U] =
      mixed(base, QVector3D(0.26F, 0.22F, 0.19F), 0.38F);
  variant.roles[Render::Wildlife::k_wolf_role_paw - 1U] =
      mixed(base, QVector3D(0.22F, 0.20F, 0.18F), 0.58F);
  variant.roles[Render::Wildlife::k_wolf_role_nose - 1U] = {0.07F, 0.065F, 0.07F};
  variant.roles[Render::Wildlife::k_wolf_role_eye - 1U] = {0.09F, 0.07F, 0.05F};
  variant.roles[Render::Wildlife::k_wolf_role_iris - 1U] = {0.86F, 0.64F, 0.16F};
  variant.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_wolf_role_count);
  return variant;
}

auto resolve_gait(const DrawState& state) -> Render::Wildlife::WolfGait {
  bool const stalking = state.behavior == Game::Wildlife::Behavior::Stalk;
  if (state.speed_ratio > k_run_threshold) {
    return Render::Wildlife::WolfGait::Run;
  }
  if (stalking && state.speed_ratio <= 0.02F) {
    return Render::Wildlife::WolfGait::Stand;
  }
  if (stalking) {
    return Render::Wildlife::WolfGait::Stalk;
  }
  if (state.speed_ratio > 0.02F) {
    return Render::Wildlife::WolfGait::Walk;
  }
  return Render::Wildlife::WolfGait::Stand;
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
  return state.behavior == Game::Wildlife::Behavior::Stalk
             ? Render::Creature::AnimationStateId::AttackMelee
             : Render::Creature::AnimationStateId::Idle;
}

void draw_wolf(const DrawContext& ctx, ISubmitter& out) {
  const DrawState state = resolve_draw_state(ctx, k_top_speed);
  const Render::Wildlife::WolfGait gait = resolve_gait(state);

  Render::Wildlife::WildlifeRenderInputs inputs;
  inputs.kind = Render::Creature::Pipeline::CreatureKind::Wolf;
  inputs.variant = resolve_variant(state);
  inputs.phase = gait_phase(state, Render::Wildlife::wolf_gait_advance(gait));
  inputs.state = state_for_gait(state, gait);

  Render::Wildlife::submit_wildlife(ctx, inputs, out);
}

} // namespace

void register_wolf_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(std::string("wildlife/wolf"), &draw_wolf);
}

} // namespace Render::GL::Wildlife
