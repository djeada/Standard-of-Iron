#include "defense_tower_renderer.h"
#include "building_render_common.h"
#include "nations/carthage/defense_tower_renderer.h"
#include "nations/roman/defense_tower_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_defense_tower_renderer(EntityRendererRegistry &registry) {
  Roman::register_defense_tower_renderer(registry);
  Carthage::register_defense_tower_renderer(registry);
  register_nation_variant_renderer(
      registry, "defense_tower",
      building_renderer_key("roman", "defense_tower"),
      building_renderer_key("carthage", "defense_tower"));
}

} // namespace Render::GL
