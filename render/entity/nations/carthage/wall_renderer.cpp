#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.34F, 0.22F, 0.11F),
                                 .wood_mid = QVector3D(0.23F, 0.14F, 0.07F),
                                 .wood_dark = QVector3D(0.11F, 0.065F, 0.035F),
                                 .rope = QVector3D(0.42F, 0.29F, 0.13F),
                                 .shadow = QVector3D(0.055F, 0.040F, 0.025F),
                                 .masonry_accent = QVector3D(0.58F, 0.34F, 0.09F),
                                 .alternate_starts_light = false,
                                 .horned_masonry = false};
const WallGeometry k_wall_geometry{.earthwork_base = true,
                                   .cross_braced = true,
                                   .metal_bands = true,
                                   .irregular_stakes = true,
                                   .connected_span_length = 1.08F,
                                   .open_span_length = 1.00F,
                                   .stake_center_y = 1.22F,
                                   .stake_half_height = 1.22F,
                                   .tip_center_y = 2.62F,
                                   .tip_half_height = 0.19F,
                                   .lower_lashing_y = 0.70F,
                                   .upper_lashing_y = 1.52F,
                                   .x_lashing_half_z = 0.15F,
                                   .x_shadow_y = 0.28F,
                                   .x_shadow_half_z = 0.25F,
                                   .x_stake_half_x = 0.15F,
                                   .x_stake_half_z = 0.17F,
                                   .x_tip_half_x = 0.08F,
                                   .x_tip_half_z = 0.10F,
                                   .x_cap_half_x = 0.19F,
                                   .x_cap_half_z = 0.19F,
                                   .z_lashing_half_x = 0.15F,
                                   .z_shadow_y = 0.28F,
                                   .z_shadow_half_x = 0.25F,
                                   .z_stake_half_x = 0.17F,
                                   .z_stake_half_z = 0.15F,
                                   .z_tip_half_x = 0.10F,
                                   .z_tip_half_z = 0.08F,
                                   .z_cap_half_x = 0.19F,
                                   .z_cap_half_z = 0.19F,
                                   .center_base_y = 0.17F,
                                   .center_base_half_x = 0.58F,
                                   .center_base_half_z = 0.58F,
                                   .center_post_half_x = 0.22F,
                                   .center_post_half_z = 0.22F,
                                   .center_tip_half_x = 0.12F,
                                   .center_tip_half_z = 0.12F,
                                   .center_tip_y_offset = 0.10F};
auto wall_archetypes() -> const std::array<RenderArchetype, 6>& {
  static const std::array<RenderArchetype, 6> archetypes = build_wall_archetype_set(
      "carthage_wall_variant", k_wall_palette, k_wall_geometry);
  return archetypes;
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "carthage", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }
}

} // namespace Render::GL::Carthage
