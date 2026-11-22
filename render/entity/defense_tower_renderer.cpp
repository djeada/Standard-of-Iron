#include "render/entity/defense_tower_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/defense_tower_renderer.h"
#include "render/entity/nations/roman/defense_tower_renderer.h"
#include "render/entity/registry.h"

namespace Render::GL {

void register_defense_tower_renderer(EntityRendererRegistry& registry) {
  Roman::register_defense_tower_renderer(registry);
  Carthage::register_defense_tower_renderer(registry);
  register_nation_variant_renderer(registry,
                                   "defense_tower",
                                   building_renderer_key("roman", "defense_tower"),
                                   building_renderer_key("carthage", "defense_tower"));
}

} // namespace Render::GL
