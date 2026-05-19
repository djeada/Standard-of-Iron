#include "wall_renderer.h"

#include "building_render_common.h"
#include "nations/carthage/wall_renderer.h"
#include "nations/roman/wall_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_wall_renderer(EntityRendererRegistry& registry) {
  Roman::register_wall_renderer(registry);
  Carthage::register_wall_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "wall_segment",
                                   building_renderer_key("roman", "wall_segment"),
                                   building_renderer_key("carthage", "wall_segment"));
}

} // namespace Render::GL
