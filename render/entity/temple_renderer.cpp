#include "render/entity/temple_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/temple_renderer.h"
#include "render/entity/nations/roman/temple_renderer.h"
#include "render/entity/registry.h"

namespace Render::GL {

void register_temple_renderer(EntityRendererRegistry& registry) {
  Roman::register_temple_renderer(registry);
  Carthage::register_temple_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "temple",
                                   building_renderer_key("roman", "temple"),
                                   building_renderer_key("carthage", "temple"));
}

} // namespace Render::GL
