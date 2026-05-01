#include "barracks_renderer.h"
#include "building_render_common.h"
#include "nations/carthage/barracks_renderer.h"
#include "nations/roman/barracks_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_barracks_renderer(EntityRendererRegistry &registry) {
  Roman::register_barracks_renderer(registry);
  Carthage::register_barracks_renderer(registry);
  register_nation_variant_renderer(
      registry, "barracks", building_renderer_key("roman", "barracks"),
      building_renderer_key("carthage", "barracks"));
}

} // namespace Render::GL
