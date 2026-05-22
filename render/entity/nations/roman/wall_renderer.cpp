#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Roman {
namespace {

const WallPalette k_wall_palette{};
const WallGeometry k_wall_geometry{};
const std::array<RenderArchetype, 6> k_wall_archetypes =
    build_wall_archetype_set("roman_wall_variant", k_wall_palette, k_wall_geometry);

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "roman", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, k_wall_archetypes, variant);
        });
  }
}

} // namespace Render::GL::Roman
