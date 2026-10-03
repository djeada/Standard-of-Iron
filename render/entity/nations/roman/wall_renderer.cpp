#include "wall_renderer.h"

#include "building_palette.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/registry.h"
#include "render/entity/wall_gate_renderer_common.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Roman {
namespace {

const WallPalette k_wall_palette{.wood_light = BuildingPalette::k_cedar_light,
                                 .wood_mid = BuildingPalette::k_cedar,
                                 .wood_dark = BuildingPalette::k_cedar_dark,
                                 .rope = BuildingPalette::k_rope,
                                 .masonry_accent = BuildingPalette::k_limestone_shade,
                                 .earth_light = BuildingPalette::k_earth_light,
                                 .earth_dark = BuildingPalette::k_earth_dark,
                                 .rubble = BuildingPalette::k_limestone_dark,
                                 .alternate_starts_light = true};
const WallGeometry k_wall_geometry{.earthwork_base = true,
                                   .cross_braced = false,
                                   .metal_bands = false,
                                   .irregular_stakes = false,
                                   .open_span_length = 1.00F,
                                   .stake_height = 2.48F,
                                   .stake_radius = 0.120F,
                                   .tip_height = 0.40F,
                                   .post_radius = 0.220F,
                                   .post_extra_height = 0.14F,
                                   .lower_rail_y = 0.78F,
                                   .upper_rail_y = 1.58F,
                                   .rail_radius = 0.070F,
                                   .berm_half_width = 0.40F,
                                   .berm_height = 0.26F};
auto wall_archetypes() -> const WallArchetypeSet& {
  static const WallArchetypeSet archetypes = wall_archetype_set("roman_wall_variant");
  return archetypes;
}

auto ladder_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet& archetype =
      building_archetype_set("roman_wall_variant_ladder");
  return archetype;
}

auto gate_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet& archetype =
      wall_gate_archetype("roman_wall_variant");
  return archetype;
}

} // namespace

auto wall_palette() -> const WallPalette& {
  return k_wall_palette;
}

auto wall_geometry() -> const WallGeometry& {
  return k_wall_geometry;
}

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "roman", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }

  register_building_renderer(
      registry, "roman", "wall_ladder", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_ladder(out, p, ladder_archetype());
      });

  register_building_renderer(
      registry, "roman", "wall_gate", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_gate(out, p, gate_archetype(), k_wall_palette, k_wall_geometry);
      });
}

} // namespace Render::GL::Roman
