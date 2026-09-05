#include "marketplace_renderer_common.h"

#include "../entity_appearance.h"

namespace Render::GL {

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "marketplace",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const BuildingState state = resolve_building_state(ctx);
        if (config.palette_slots != nullptr) {
          const QVector3D team = Render::entity_color(*ctx.entity);
          const auto palette_slots = config.palette_slots(team);
          submit_building_instance(out, ctx, config.archetype(state), palette_slots);
        } else {
          submit_building_instance(out, ctx, config.archetype(state));
        }
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
