#include "defense_tower_renderer_common.h"

#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cmath>

#include "../entity_appearance.h"
#include "game/core/component.h"
#include "render/scene_renderer.h"

namespace Render::GL {

namespace {

constexpr float k_brazier_radius = 0.16F;
constexpr float k_brazier_intensity = 0.95F;
constexpr float k_brazier_flicker = 0.14F;
const QVector3D k_brazier_color(1.0F, 0.50F, 0.16F);

void draw_night_braziers(const DrawContext& ctx,
                         ISubmitter& out,
                         const DefenseTowerRendererConfig& config,
                         BuildingState state) {
  if (state != BuildingState::Normal) {
    return;
  }
  auto* scene_renderer = dynamic_cast<Renderer*>(out.unwrap_submitter());
  if (scene_renderer == nullptr) {
    return;
  }
  const float night = environment_night_amount(scene_renderer->environment_lighting());
  if (night <= 0.01F) {
    return;
  }
  const float time = scene_renderer->get_animation_time();
  const std::array<QVector3D, 2> corners{QVector3D(config.night_brazier_offset,
                                                   config.night_brazier_deck_y,
                                                   config.night_brazier_offset),
                                         QVector3D(-config.night_brazier_offset,
                                                   config.night_brazier_deck_y,
                                                   -config.night_brazier_offset)};
  for (std::size_t i = 0; i < corners.size(); ++i) {
    const QVector4D world = ctx.model * QVector4D(corners[i], 1.0F);
    const float phase = static_cast<float>(i) * 2.1F + world.x() * 0.37F;
    const float flicker = 1.0F + k_brazier_flicker * std::sin(time * 5.3F + phase) *
                                     std::sin(time * 2.9F + phase * 1.7F);
    scene_renderer->fireball(world.toVector3D(),
                             k_brazier_color,
                             k_brazier_radius,
                             k_brazier_intensity * night * flicker,
                             time + phase);
  }
}

} // namespace

void register_defense_tower_renderer_variant(EntityRendererRegistry& registry,
                                             const DefenseTowerRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "defense_tower",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }

        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (r == nullptr) {
          return;
        }

        const QVector3D team = Render::entity_color(*ctx.entity);
        const BuildingState state = resolve_building_state(ctx);
        submit_building_instance(out, ctx, config.archetype(state));
        config.draw_banner(ctx, out, team, state);
        draw_night_braziers(ctx, out, config, state);
        draw_building_health_bar(out, ctx, config.health_bar_style(state));
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
