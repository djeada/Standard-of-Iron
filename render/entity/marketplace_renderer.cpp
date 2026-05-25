#include "marketplace_renderer.h"

#include "building_render_common.h"
#include "nations/carthage/marketplace_renderer.h"
#include "nations/roman/marketplace_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  Roman::register_marketplace_renderer(registry);
  Carthage::register_marketplace_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "marketplace",
                                   building_renderer_key("roman", "marketplace"),
                                   building_renderer_key("carthage", "marketplace"));
}

} // namespace Render::GL
