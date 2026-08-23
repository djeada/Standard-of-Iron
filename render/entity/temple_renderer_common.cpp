#include "temple_renderer_common.h"

namespace Render::GL {

void register_temple_renderer_variant(EntityRendererRegistry& registry,
                                      const TempleRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "temple",
      [config](const DrawContext& ctx, ISubmitter& out) {
        const BuildingState state = resolve_building_state(ctx);
        submit_building_instance(out, ctx, config.archetype(state));
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
