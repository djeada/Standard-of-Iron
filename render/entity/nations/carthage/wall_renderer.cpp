#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.67F, 0.64F, 0.59F),
                                 .wood_mid = QVector3D(0.55F, 0.52F, 0.48F),
                                 .wood_dark = QVector3D(0.43F, 0.41F, 0.38F),
                                 .rope = QVector3D(0.60F, 0.58F, 0.53F),
                                 .shadow = QVector3D(0.31F, 0.29F, 0.27F),
                                 .masonry_accent = QVector3D(0.68F, 0.38F, 0.27F),
                                 .alternate_starts_light = false};
const WallGeometry k_wall_geometry{.solid_masonry = true,
                                   .connected_span_length = 1.04F,
                                   .open_span_length = 0.96F,
                                   .stake_center_y = 1.16F,
                                   .stake_half_height = 1.16F,
                                   .tip_center_y = 2.40F,
                                   .tip_half_height = 0.12F,
                                   .lower_lashing_y = 0.72F,
                                   .upper_lashing_y = 1.48F,
                                   .x_lashing_half_z = 0.24F,
                                   .x_shadow_y = 0.28F,
                                   .x_shadow_half_z = 0.28F,
                                   .x_stake_half_x = 0.18F,
                                   .x_stake_half_z = 0.22F,
                                   .x_tip_half_x = 0.10F,
                                   .x_tip_half_z = 0.22F,
                                   .x_cap_half_x = 0.20F,
                                   .x_cap_half_z = 0.24F,
                                   .z_lashing_half_x = 0.24F,
                                   .z_shadow_y = 0.28F,
                                   .z_shadow_half_x = 0.28F,
                                   .z_stake_half_x = 0.22F,
                                   .z_stake_half_z = 0.18F,
                                   .z_tip_half_x = 0.22F,
                                   .z_tip_half_z = 0.10F,
                                   .z_cap_half_x = 0.24F,
                                   .z_cap_half_z = 0.20F,
                                   .center_base_y = 0.17F,
                                   .center_base_half_x = 0.58F,
                                   .center_base_half_z = 0.58F,
                                   .center_post_half_x = 0.30F,
                                   .center_post_half_z = 0.30F,
                                   .center_tip_half_x = 0.30F,
                                   .center_tip_half_z = 0.30F,
                                   .center_tip_y_offset = 0.02F};
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
