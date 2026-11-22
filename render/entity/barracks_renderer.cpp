#include "render/entity/barracks_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/barracks_renderer.h"
#include "render/entity/nations/roman/barracks_renderer.h"
#include "render/entity/nations/sepulcher/barracks_renderer.h"
#include "render/entity/registry.h"

namespace Render::GL {

void register_barracks_renderer(EntityRendererRegistry& registry) {
  Roman::register_barracks_renderer(registry);
  Carthage::register_barracks_renderer(registry);
  Sepulcher::register_barracks_renderer(registry);

  register_nation_variant_renderer(registry,
                                   "barracks",
                                   building_renderer_key("roman", "barracks"),
                                   building_renderer_key("carthage", "barracks"),
                                   building_renderer_key("iron_sepulcher", "barracks"));
}

} // namespace Render::GL
