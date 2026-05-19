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
  register_nation_variant_renderer(
      registry,
      "wall_segment_isolated",
      building_renderer_key("roman", "wall_segment_isolated"),
      building_renderer_key("carthage", "wall_segment_isolated"));
  register_nation_variant_renderer(
      registry,
      "wall_segment_end",
      building_renderer_key("roman", "wall_segment_end"),
      building_renderer_key("carthage", "wall_segment_end"));
  register_nation_variant_renderer(
      registry,
      "wall_segment_straight",
      building_renderer_key("roman", "wall_segment_straight"),
      building_renderer_key("carthage", "wall_segment_straight"));
  register_nation_variant_renderer(
      registry,
      "wall_segment_corner",
      building_renderer_key("roman", "wall_segment_corner"),
      building_renderer_key("carthage", "wall_segment_corner"));
  register_nation_variant_renderer(
      registry,
      "wall_segment_tee",
      building_renderer_key("roman", "wall_segment_tee"),
      building_renderer_key("carthage", "wall_segment_tee"));
  register_nation_variant_renderer(
      registry,
      "wall_segment_cross",
      building_renderer_key("roman", "wall_segment_cross"),
      building_renderer_key("carthage", "wall_segment_cross"));
}

} // namespace Render::GL
