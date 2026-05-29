#include "home_renderer_common.h"

#include "../../game/core/component.h"

namespace Render::GL {

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "home",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }

        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (r == nullptr) {
          return;
        }

        const QVector3D team(r->color[0], r->color[1], r->color[2]);
        const auto palette_slots = config.palette_slots(team);
        submit_building_instance(
            out, ctx, config.archetype(resolve_building_state(ctx)), palette_slots);
        draw_building_health_bar(out, ctx, config.health_bar);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
