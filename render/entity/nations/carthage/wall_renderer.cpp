#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_gate_renderer_common.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.34F, 0.22F, 0.11F),
                                 .wood_mid = QVector3D(0.23F, 0.14F, 0.07F),
                                 .wood_dark = QVector3D(0.11F, 0.065F, 0.035F),
                                 .rope = QVector3D(0.42F, 0.29F, 0.13F),
                                 .masonry_accent = QVector3D(0.58F, 0.34F, 0.09F),
                                 .earth_light = QVector3D(0.34F, 0.24F, 0.15F),
                                 .earth_dark = QVector3D(0.23F, 0.16F, 0.10F),
                                 .rubble = QVector3D(0.33F, 0.30F, 0.27F),
                                 .alternate_starts_light = false,
                                 .horned_masonry = false};
const WallGeometry k_wall_geometry{.earthwork_base = true,
                                   .cross_braced = true,
                                   .metal_bands = true,
                                   .irregular_stakes = true,
                                   .open_span_length = 1.00F,
                                   .stake_height = 2.44F,
                                   .stake_radius = 0.102F,
                                   .tip_height = 0.38F,
                                   .post_radius = 0.190F,
                                   .post_extra_height = 0.22F,
                                   .lower_rail_y = 0.70F,
                                   .upper_rail_y = 1.52F,
                                   .rail_radius = 0.055F,
                                   .berm_half_width = 0.28F,
                                   .berm_height = 0.20F};
auto wall_archetypes() -> const std::array<RenderArchetype, 6>& {
  static const std::array<RenderArchetype, 6> archetypes = build_wall_archetype_set(
      "carthage_wall_variant", k_wall_palette, k_wall_geometry);
  return archetypes;
}

auto gate_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = build_wall_gate_archetype(
      "carthage_wall_variant", k_wall_palette, k_wall_geometry);
  return archetype;
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "carthage", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }

  register_building_renderer(
      registry, "carthage", "wall_gate", [](const DrawContext& p, ISubmitter& out) {
        submit_wall_gate(out, p, gate_archetype(), k_wall_palette, k_wall_geometry);
      });
}

} // namespace Render::GL::Carthage
