#include "render/entity/marketplace_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/lot_building_renderer.h"
#include "render/entity/nations/carthage/marketplace_renderer.h"
#include "render/entity/nations/roman/marketplace_renderer.h"
#include "render/entity/registry.h"

namespace Render::GL {

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  Roman::register_marketplace_renderer(registry);
  Carthage::register_marketplace_renderer(registry);
  register_market_lot_renderers(registry);
  register_nation_variant_renderer(registry,
                                   "marketplace",
                                   building_renderer_key("roman", "marketplace"),
                                   building_renderer_key("carthage", "marketplace"));
}

} // namespace Render::GL
