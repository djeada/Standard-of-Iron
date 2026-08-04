#include "sheep_renderer.h"

#include <QVector3D>

#include <cstdint>
#include <string>

#include "../../creature/animation_state_components.h"
#include "../../wildlife/sheep_spec.h"
#include "../../wildlife/wildlife_prepare.h"
#include "../registry.h"
#include "wildlife_draw_state.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_stride_rate = 1.30F;

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
  QVector3D const shade =
      mixed(tinted(wool, 0.82F), QVector3D(0.36F, 0.32F, 0.27F), 0.18F);

  Render::GL::WildlifeVariant variant;
  variant.roles[Render::Wildlife::k_sheep_role_wool - 1U] = wool;
  variant.roles[Render::Wildlife::k_sheep_role_wool_light - 1U] =
      mixed(tinted(wool, 1.07F), QVector3D(1.0F, 0.97F, 0.90F), 0.16F);
  variant.roles[Render::Wildlife::k_sheep_role_wool_shade - 1U] = shade;
  variant.roles[Render::Wildlife::k_sheep_role_wool_grubby - 1U] =
      mixed(shade, QVector3D(0.44F, 0.38F, 0.29F), 0.52F);
  variant.roles[Render::Wildlife::k_sheep_role_face - 1U] = face;
  variant.roles[Render::Wildlife::k_sheep_role_hoof - 1U] =
      mixed(face, QVector3D(0.10F, 0.09F, 0.09F), 0.72F);
  variant.roles[Render::Wildlife::k_sheep_role_nose - 1U] =
      mixed(face, QVector3D(0.08F, 0.07F, 0.07F), 0.65F);
  variant.roles[Render::Wildlife::k_sheep_role_eye - 1U] = {0.07F, 0.06F, 0.05F};
  variant.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_sheep_role_count);
  return variant;
}

auto resolve_state(const DrawState& state) -> Render::Creature::AnimationStateId {
  if (state.grazing) {
    return Render::Creature::AnimationStateId::Hold;
  }
  if (state.speed_ratio > 0.55F) {
    return Render::Creature::AnimationStateId::Run;
  }
  if (state.speed_ratio > 0.02F) {
    return Render::Creature::AnimationStateId::Walk;
  }
  return Render::Creature::AnimationStateId::Idle;
}

void draw_sheep(const DrawContext& ctx, ISubmitter& out) {
  const DrawState state = resolve_draw_state(ctx, k_stride_rate);

  Render::Wildlife::WildlifeRenderInputs inputs;
  inputs.kind = Render::Creature::Pipeline::CreatureKind::Sheep;
  inputs.variant = resolve_variant(state);
  inputs.phase = state.phase;
  inputs.state = resolve_state(state);

  Render::Wildlife::submit_wildlife(ctx, inputs, out);
}

} // namespace

void register_sheep_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(std::string("wildlife/sheep"), &draw_sheep);
}

} // namespace Render::GL::Wildlife
