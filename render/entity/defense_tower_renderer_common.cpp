#include "defense_tower_renderer_common.h"

#include "../../game/core/component.h"

namespace Render::GL {

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

        const QVector3D team(r->color[0], r->color[1], r->color[2]);
        const BuildingState state = resolve_building_state(ctx);
        submit_building_instance(out, ctx, config.archetype(state));
        config.draw_banner(ctx, out, team, state);
        draw_building_health_bar(out, ctx, config.health_bar_style(state));
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
