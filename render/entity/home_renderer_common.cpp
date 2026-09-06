#include "home_renderer_common.h"

#include "../entity_appearance.h"
#include "game/core/component_core.h"

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

        const QVector3D team = Render::entity_color(*ctx.entity);
        const auto palette_slots = config.palette_slots(team);
        submit_building_instance(
            out, ctx, config.archetype(resolve_building_state(ctx)), palette_slots);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
