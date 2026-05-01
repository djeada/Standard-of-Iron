#include "home_renderer.h"
#include "building_render_common.h"
#include "nations/carthage/home_renderer.h"
#include "nations/roman/home_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_home_renderer(EntityRendererRegistry &registry) {
  Roman::register_home_renderer(registry);
  Carthage::register_home_renderer(registry);
  register_nation_variant_renderer(registry, "home",
                                   building_renderer_key("roman", "home"),
                                   building_renderer_key("carthage", "home"));
}

} // namespace Render::GL
