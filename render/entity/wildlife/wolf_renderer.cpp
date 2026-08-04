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

constexpr float k_stride_rate = 1.70F;

auto resolve_variant(const DrawState& state) -> Render::GL::WildlifeVariant {
  float const morph = hash_unit_float(state.seed, 31U);
  QVector3D base = state.coat;
  if (morph < 0.30F) {
    base = mixed(base, QVector3D(0.36F, 0.28F, 0.19F), 0.55F);
  } else if (morph > 0.86F) {
    base = mixed(base, QVector3D(0.14F, 0.13F, 0.14F), 0.62F);
  }

  Render::GL::WildlifeVariant variant;
  variant.roles[Render::Wildlife::k_wolf_role_fur - 1U] = base;
  variant.roles[Render::Wildlife::k_wolf_role_saddle - 1U] =
      mixed(base, QVector3D(0.13F, 0.11F, 0.10F), 0.58F);
  variant.roles[Render::Wildlife::k_wolf_role_pale - 1U] =
      mixed(base, QVector3D(0.84F, 0.78F, 0.66F), 0.42F);
  variant.roles[Render::Wildlife::k_wolf_role_cream - 1U] =
      mixed(base, QVector3D(0.90F, 0.85F, 0.74F), 0.38F);
  variant.roles[Render::Wildlife::k_wolf_role_limb - 1U] =
      mixed(base, QVector3D(0.26F, 0.22F, 0.19F), 0.38F);
  variant.roles[Render::Wildlife::k_wolf_role_paw - 1U] =
      mixed(base, QVector3D(0.14F, 0.13F, 0.12F), 0.62F);
  variant.roles[Render::Wildlife::k_wolf_role_nose - 1U] = {0.07F, 0.065F, 0.07F};
  variant.roles[Render::Wildlife::k_wolf_role_eye - 1U] = {0.09F, 0.07F, 0.05F};
  variant.roles[Render::Wildlife::k_wolf_role_iris - 1U] = {0.86F, 0.64F, 0.16F};
  variant.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_wolf_role_count);
  return variant;
}

auto resolve_state(const DrawState& state) -> Render::Creature::AnimationStateId {
  bool const stalking = state.behavior == Game::Wildlife::Behavior::Stalk;
  if (state.speed_ratio > 0.60F) {
    return Render::Creature::AnimationStateId::Run;
  }
  if (stalking && state.speed_ratio <= 0.02F) {
    return Render::Creature::AnimationStateId::AttackMelee;
  }
  if (stalking) {
    return Render::Creature::AnimationStateId::Hold;
  }
  if (state.speed_ratio > 0.02F) {
    return Render::Creature::AnimationStateId::Walk;
  }
  return Render::Creature::AnimationStateId::Idle;
}

void draw_wolf(const DrawContext& ctx, ISubmitter& out) {
  const DrawState state = resolve_draw_state(ctx, k_stride_rate);

  Render::Wildlife::WildlifeRenderInputs inputs;
  inputs.kind = Render::Creature::Pipeline::CreatureKind::Wolf;
  inputs.variant = resolve_variant(state);
  inputs.phase = state.phase;
  inputs.state = resolve_state(state);

  Render::Wildlife::submit_wildlife(ctx, inputs, out);
}

} // namespace

void register_wolf_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(std::string("wildlife/wolf"), &draw_wolf);
}

} // namespace Render::GL::Wildlife
