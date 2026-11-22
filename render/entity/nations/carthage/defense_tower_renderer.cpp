#include "render/entity/nations/carthage/defense_tower_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/defense_tower_renderer_common.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/nations/carthage/defense_tower_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

using namespace defense_tower_detail;

namespace {

auto tower_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set =
      building_archetype_set("carthage_defense_tower");
  return k_set.for_state(state);
}

auto tower_banner_style(BuildingState state)
    -> BarracksFlagRenderer::HangingBannerStyle {
  if (state == BuildingState::Damaged) {
    return {.pole_base = QVector3D(0.0F, 2.50F, 0.0F),
            .pole_height = 0.88F,
            .pole_radius = 0.042F,
            .banner_width = 0.56F,
            .banner_height = 0.34F,
            .beam_inset = 0.02F,
            .banner_depth = 0.02F,
            .banner_z_offset = 0.0F,
            .connector_drop_ratio = 0.30F,
            .capture_lowering_ratio = 0.45F,
            .pole_color = QVector3D(),
            .beam_color = QVector3D(),
            .connector_color = QVector3D(),
            .ornament_offset = QVector3D(0.0F, 0.90F, 0.0F),
            .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
            .ornament_color = QVector3D(),
            .ring_count = 1,
            .ring_y_start = 0.26F,
            .ring_spacing = 0.22F,
            .ring_height = 0.024F,
            .ring_radius_scale = 2.1F,
            .ring_color = QVector3D()};
  }

  return {.pole_base = QVector3D(0.0F, 2.94F, 0.0F),
          .pole_height = 1.16F,
          .pole_radius = 0.045F,
          .banner_width = 0.72F,
          .banner_height = 0.40F,
          .beam_inset = 0.02F,
          .banner_depth = 0.02F,
          .banner_z_offset = 0.0F,
          .connector_drop_ratio = 0.32F,
          .capture_lowering_ratio = 0.50F,
          .pole_color = QVector3D(),
          .beam_color = QVector3D(),
          .connector_color = QVector3D(),
          .ornament_offset = QVector3D(0.0F, 1.12F, 0.0F),
          .ornament_size = QVector3D(0.14F, 0.11F, 0.14F),
          .ornament_color = QVector3D(),
          .ring_count = 2,
          .ring_y_start = 0.44F,
          .ring_spacing = 0.24F,
          .ring_height = 0.026F,
          .ring_radius_scale = 2.2F,
          .ring_color = QVector3D()};
}

void draw_tower_banner(const DrawContext& p,
                       ISubmitter& out,
                       const TowerPalette& palette,
                       BuildingState state) {
  if (state == BuildingState::Destroyed || p.resources == nullptr) {
    return;
  }

  Mesh* unit = p.resources->unit();
  if (unit == nullptr) {
    unit = get_unit_cube();
  }
  Texture* white = p.resources->white();

  auto style = tower_banner_style(state);
  style.pole_color = palette.wood_dark;
  style.beam_color = palette.wood;
  style.connector_color = palette.stone_light;
  style.ornament_color = palette.iron;
  style.ring_color = palette.iron;

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.resources != nullptr) {
    cloth.cloth_mesh = p.resources->banner_mesh();
    cloth.banner_shader = p.resources->banner_shader();
  }

  BarracksFlagRenderer::draw_hanging_banner(
      p, out, unit, white, palette.team, palette.team_trim, style, &cloth);
}

void draw_tower_banner_for_team(const DrawContext& p,
                                ISubmitter& out,
                                const QVector3D& team,
                                BuildingState state) {
  draw_tower_banner(p, out, make_palette(team), state);
}

const std::array<TorchMount, 2> k_torches{{
    TorchMount{.at = QVector3D(-0.40F, 1.10F, 0.78F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(0.40F, 1.10F, 0.78F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

} // namespace

void register_defense_tower_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_defense_tower_renderer_variant(
      registry,
      DefenseTowerRendererConfig{.nation_slug = "carthage",
                                 .archetype = &tower_archetype,
                                 .draw_banner = &draw_tower_banner_for_team,
                                 .selection = BuildingSelectionStyle{1.6F, 1.6F},
                                 .night_brazier_deck_y = 2.68F,
                                 .night_brazier_offset = 0.60F,
                                 .torches = k_torches,
                                 .lookout_deck_top = 2.57F,
                                 .lookout_deck_top_damaged = 2.25F,
                                 .lookout_walk_half = 0.40F});
}

} // namespace Render::GL::Carthage
