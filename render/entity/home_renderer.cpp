#include "render/entity/home_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/home_renderer.h"
#include "render/entity/nations/carthage/tenement_renderer.h"
#include "render/entity/nations/roman/home_renderer.h"
#include "render/entity/nations/roman/tenement_renderer.h"
#include "render/entity/registry.h"

namespace Render::GL {

void register_home_renderer(EntityRendererRegistry& registry) {
  Roman::register_home_renderer(registry);
  Roman::register_tenement_renderer(registry);
  Carthage::register_home_renderer(registry);
  Carthage::register_tenement_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "home",
                                   building_renderer_key("roman", "home"),
                                   building_renderer_key("carthage", "home"));
}

} // namespace Render::GL
