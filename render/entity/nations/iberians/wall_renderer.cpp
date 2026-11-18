#include "wall_renderer.h"

#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/registry.h"
#include "render/entity/wall_gate_renderer_common.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Iberians {
namespace {

namespace Stone = Render::GL::Carthage::BuildingPalette;

const WallPalette k_wall_palette{.wood_light = Stone::k_sandstone_light,
                                 .wood_mid = Stone::k_sandstone,
                                 .wood_dark = Stone::k_sandstone_dark,
                                 .rope = Stone::k_sandstone_shade,
                                 .masonry_accent = Stone::k_sandstone_shade,
                                 .earth_light = Stone::k_sandstone_shade,
                                 .earth_dark = Stone::k_sandstone_dark,
                                 .rubble = Stone::k_sandstone_dark,
                                 .alternate_starts_light = false,
                                 .horned_masonry = false};
const WallGeometry k_wall_geometry{.solid_masonry = true,
                                   .earthwork_base = false,
                                   .open_span_length = 1.00F,
                                   .stake_height = 2.80F,
                                   .post_radius = 0.30F,
                                   .post_extra_height = 0.30F,
                                   .masonry_half_width = 0.46F,
                                   .masonry_height = 2.60F};

auto wall_archetypes() -> const WallArchetypeSet& {
  static const WallArchetypeSet archetypes =
      wall_archetype_set("iberians_wall_variant");
  return archetypes;
}

auto ladder_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet& archetype =
      building_archetype_set("iberians_wall_variant_ladder");
  return archetype;
}

auto gate_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet& archetype =
      wall_gate_archetype("iberians_wall_variant");
  return archetype;
}

auto gate_leaf_archetype() -> const BuildingArchetypeSet& {
  static const BuildingArchetypeSet& archetype =
      wall_gate_leaf_archetype("iberians_wall_variant");
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
        registry, "iberians", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }

  register_building_renderer(
      registry, "iberians", "wall_ladder", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_ladder(out, p, ladder_archetype());
      });

  register_building_renderer(
      registry, "iberians", "wall_gate", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_gate(out, p, gate_archetype(), gate_leaf_archetype());
      });
}

} // namespace Render::GL::Iberians
