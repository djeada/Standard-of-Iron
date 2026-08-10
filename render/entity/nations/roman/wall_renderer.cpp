#include "wall_renderer.h"

#include "render/entity/building_render_common.h"
#include "render/entity/registry.h"
#include "render/entity/wall_gate_renderer_common.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Roman {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.62F, 0.43F, 0.23F),
                                 .wood_mid = QVector3D(0.46F, 0.30F, 0.15F),
                                 .wood_dark = QVector3D(0.27F, 0.17F, 0.085F),
                                 .rope = QVector3D(0.55F, 0.44F, 0.25F),
                                 .masonry_accent = QVector3D(0.46F, 0.30F, 0.15F),
                                 .earth_light = QVector3D(0.41F, 0.31F, 0.20F),
                                 .earth_dark = QVector3D(0.29F, 0.21F, 0.13F),
                                 .rubble = QVector3D(0.42F, 0.39F, 0.35F),
                                 .alternate_starts_light = true};
const WallGeometry k_wall_geometry{.earthwork_base = true,
                                   .cross_braced = false,
                                   .metal_bands = false,
                                   .irregular_stakes = false,
                                   .open_span_length = 1.00F,
                                   .stake_height = 2.48F,
                                   .stake_radius = 0.105F,
                                   .tip_height = 0.40F,
                                   .post_radius = 0.200F,
                                   .post_extra_height = 0.14F,
                                   .lower_rail_y = 0.78F,
                                   .upper_rail_y = 1.58F,
                                   .rail_radius = 0.060F,
                                   .berm_half_width = 0.30F,
                                   .berm_height = 0.22F};
auto wall_archetypes() -> const WallArchetypeSet& {
  static const WallArchetypeSet archetypes =
      build_wall_archetype_set("roman_wall_variant", k_wall_palette, k_wall_geometry);
  return archetypes;
}

auto gate_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet archetype =
      build_wall_gate_archetype("roman_wall_variant", k_wall_palette, k_wall_geometry);
  return archetype;
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "roman", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }

  register_building_renderer(
      registry, "roman", "wall_gate", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_gate(out, p, gate_archetype(), k_wall_palette, k_wall_geometry);
      });
}

} // namespace Render::GL::Roman
