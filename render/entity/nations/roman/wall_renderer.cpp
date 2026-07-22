#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Roman {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.62F, 0.43F, 0.23F),
                                 .wood_mid = QVector3D(0.46F, 0.30F, 0.15F),
                                 .wood_dark = QVector3D(0.27F, 0.17F, 0.085F),
                                 .rope = QVector3D(0.55F, 0.44F, 0.25F),
                                 .shadow = QVector3D(0.19F, 0.12F, 0.06F),
                                 .masonry_accent = QVector3D(0.46F, 0.30F, 0.15F),
                                 .alternate_starts_light = true};
const WallGeometry k_wall_geometry{.earthwork_base = true,
                                   .cross_braced = false,
                                   .metal_bands = false,
                                   .irregular_stakes = false,
                                   .connected_span_length = 1.08F,
                                   .open_span_length = 1.00F,
                                   .stake_center_y = 1.24F,
                                   .stake_half_height = 1.24F,
                                   .tip_center_y = 2.60F,
                                   .tip_half_height = 0.20F,
                                   .lower_lashing_y = 0.78F,
                                   .upper_lashing_y = 1.58F,
                                   .x_lashing_half_z = 0.22F,
                                   .x_shadow_y = 0.32F,
                                   .x_shadow_half_z = 0.26F,
                                   .x_stake_half_x = 0.17F,
                                   .x_stake_half_z = 0.21F,
                                   .x_tip_half_x = 0.09F,
                                   .x_tip_half_z = 0.12F,
                                   .x_cap_half_x = 0.22F,
                                   .x_cap_half_z = 0.26F,
                                   .z_lashing_half_x = 0.22F,
                                   .z_shadow_y = 0.32F,
                                   .z_shadow_half_x = 0.26F,
                                   .z_stake_half_x = 0.21F,
                                   .z_stake_half_z = 0.17F,
                                   .z_tip_half_x = 0.12F,
                                   .z_tip_half_z = 0.09F,
                                   .z_cap_half_x = 0.26F,
                                   .z_cap_half_z = 0.22F,
                                   .center_base_y = 0.20F,
                                   .center_base_half_x = 0.58F,
                                   .center_base_half_z = 0.58F,
                                   .center_post_half_x = 0.30F,
                                   .center_post_half_z = 0.30F,
                                   .center_tip_half_x = 0.12F,
                                   .center_tip_half_z = 0.12F,
                                   .center_tip_y_offset = 0.02F};
auto wall_archetypes() -> const std::array<RenderArchetype, 6>& {
  static const std::array<RenderArchetype, 6> archetypes =
      build_wall_archetype_set("roman_wall_variant", k_wall_palette, k_wall_geometry);
  return archetypes;
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "roman", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, wall_archetypes(), variant);
        });
  }
}

} // namespace Render::GL::Roman
