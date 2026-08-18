#include "farm_renderer.h"

#include "building_render_common.h"
#include "nations/carthage/farm_renderer.h"
#include "nations/roman/farm_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_farm_renderer(EntityRendererRegistry& registry) {
  Roman::register_farm_renderer(registry);
  Carthage::register_farm_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "farm",
                                   building_renderer_key("roman", "farm"),
                                   building_renderer_key("carthage", "farm"));
}

} // namespace Render::GL
