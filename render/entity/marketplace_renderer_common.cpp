#include "marketplace_renderer_common.h"

namespace Render::GL {

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "marketplace",
      [config](const DrawContext& ctx, ISubmitter& out) {
        const BuildingState state = resolve_building_state(ctx);
        submit_building_instance(out, ctx, config.archetype(state));
        draw_building_health_bar(out, ctx, config.health_bar);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
