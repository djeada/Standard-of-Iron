#include "render/entity/masonry_wall_renderer.h"

#include <memory>
#include <string>

#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/wall_renderer.h"
#include "render/entity/nations/roman/wall_renderer.h"
#include "render/entity/registry.h"
#include "render/entity/wall_gate_renderer_common.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL {
namespace {

const WallPalette k_roman_palette{.wood_light = QVector3D(0.80F, 0.76F, 0.64F),
                                  .wood_mid = QVector3D(0.70F, 0.66F, 0.55F),
                                  .wood_dark = QVector3D(0.48F, 0.45F, 0.38F),
                                  .rope = QVector3D(0.60F, 0.57F, 0.48F),
                                  .masonry_accent = QVector3D(0.62F, 0.58F, 0.49F),
                                  .earth_light = QVector3D(0.60F, 0.57F, 0.48F),
                                  .earth_dark = QVector3D(0.44F, 0.41F, 0.35F),
                                  .rubble = QVector3D(0.48F, 0.45F, 0.38F),
                                  .alternate_starts_light = true,
                                  .horned_masonry = false};

const WallPalette k_carthage_palette{.wood_light = QVector3D(0.90F, 0.85F, 0.73F),
                                     .wood_mid = QVector3D(0.82F, 0.75F, 0.61F),
                                     .wood_dark = QVector3D(0.55F, 0.49F, 0.39F),
                                     .rope = QVector3D(0.70F, 0.62F, 0.49F),
                                     .masonry_accent = QVector3D(0.70F, 0.62F, 0.49F),
                                     .earth_light = QVector3D(0.70F, 0.62F, 0.49F),
                                     .earth_dark = QVector3D(0.50F, 0.44F, 0.35F),
                                     .rubble = QVector3D(0.55F, 0.49F, 0.39F),
                                     .alternate_starts_light = false,
                                     .horned_masonry = false};

const WallGeometry k_geometry{.solid_masonry = true,
                              .earthwork_base = false,
                              .open_span_length = 1.00F,
                              .stake_height = 2.90F,
                              .post_radius = 0.30F,
                              .post_extra_height = 0.30F,
                              .masonry_half_width = 0.48F,
                              .masonry_height = 2.70F,
                              .stone_gatehouse = true};

void register_family(EntityRendererRegistry& registry, std::string_view slug) {
  const std::string prefix = std::string(slug) + "_wall_variant";
  const auto archetypes =
      std::make_shared<const WallArchetypeSet>(wall_archetype_set(prefix));
  const BuildingArchetypeSet* ladder = &building_archetype_set(prefix + "_ladder");
  const BuildingArchetypeSet* gate = &wall_gate_archetype(prefix);
  const BuildingArchetypeSet* leaf = &wall_gate_leaf_archetype(prefix);

  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry,
        slug,
        name,
        [archetypes, variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, *archetypes, variant);
        });
  }
  register_building_renderer(
      registry, slug, "wall_ladder", [ladder](const DrawContext& p, ISubmitter& out) {
        submit_wall_ladder(out, p, *ladder);
      });
  register_building_renderer(
      registry, slug, "wall_gate", [gate, leaf](const DrawContext& p, ISubmitter& out) {
        submit_wall_gate(out, p, *gate, *leaf);
      });
}

} // namespace

auto roman_masonry_wall() -> const MasonryWallLook& {
  static const MasonryWallLook k_look{
      "roman_masonry", &k_roman_palette, &k_geometry, &Roman::wall_palette()};
  return k_look;
}

auto carthage_masonry_wall() -> const MasonryWallLook& {
  static const MasonryWallLook k_look{
      "carthage_masonry", &k_carthage_palette, &k_geometry, &Carthage::wall_palette()};
  return k_look;
}

void register_masonry_wall_renderers(EntityRendererRegistry& registry) {
  register_family(registry, roman_masonry_wall().slug);
  register_family(registry, carthage_masonry_wall().slug);
}

} // namespace Render::GL
