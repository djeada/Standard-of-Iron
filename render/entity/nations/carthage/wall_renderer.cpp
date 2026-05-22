#include "wall_renderer.h"

#include "../../building_render_common.h"
#include "../../registry.h"
#include "../../wall_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

const WallPalette k_wall_palette{.wood_light = QVector3D(0.62F, 0.45F, 0.27F),
                                 .wood_mid = QVector3D(0.49F, 0.34F, 0.19F),
                                 .wood_dark = QVector3D(0.31F, 0.21F, 0.11F),
                                 .rope = QVector3D(0.52F, 0.40F, 0.22F),
                                 .shadow = QVector3D(0.22F, 0.15F, 0.08F),
                                 .alternate_starts_light = false};
const WallGeometry k_wall_geometry{.connected_span_length = 1.04F,
                                   .open_span_length = 0.96F,
                                   .stake_center_y = 1.16F,
                                   .stake_half_height = 1.16F,
                                   .tip_center_y = 2.44F,
                                   .tip_half_height = 0.18F,
                                   .lower_lashing_y = 0.72F,
                                   .upper_lashing_y = 1.48F,
                                   .x_lashing_half_z = 0.22F,
                                   .x_shadow_y = 0.28F,
                                   .x_shadow_half_z = 0.26F,
                                   .x_stake_half_x = 0.16F,
                                   .x_stake_half_z = 0.20F,
                                   .x_tip_half_x = 0.09F,
                                   .x_tip_half_z = 0.12F,
                                   .x_cap_half_x = 0.21F,
                                   .x_cap_half_z = 0.24F,
                                   .z_lashing_half_x = 0.22F,
                                   .z_shadow_y = 0.28F,
                                   .z_shadow_half_x = 0.26F,
                                   .z_stake_half_x = 0.20F,
                                   .z_stake_half_z = 0.16F,
                                   .z_tip_half_x = 0.12F,
                                   .z_tip_half_z = 0.09F,
                                   .z_cap_half_x = 0.24F,
                                   .z_cap_half_z = 0.21F,
                                   .center_base_y = 0.17F,
                                   .center_base_half_x = 0.56F,
                                   .center_base_half_z = 0.56F,
                                   .center_post_half_x = 0.28F,
                                   .center_post_half_z = 0.28F,
                                   .center_tip_half_x = 0.12F,
                                   .center_tip_half_z = 0.12F,
                                   .center_tip_y_offset = 0.02F};
const std::array<RenderArchetype, 6> k_wall_archetypes =
    build_wall_archetype_set("carthage_wall_variant", k_wall_palette, k_wall_geometry);

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  for (const auto& [name, variant] : wall_renderer_variants()) {
    register_building_renderer(
        registry, "carthage", name, [variant](const DrawContext& p, ISubmitter& out) {
          submit_wall_segment_variant(out, p, k_wall_archetypes, variant);
        });
  }
}

} // namespace Render::GL::Carthage
