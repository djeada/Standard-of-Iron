#include "render/entity/nations/roman/defense_tower_renderer.h"

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
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/defense_tower_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Roman {

using namespace defense_tower_detail;

namespace {

auto tower_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set =
      building_archetype_set("roman_defense_tower");
  return k_set.for_state(state);
}

auto tower_banner_style(BuildingState state)
    -> BarracksFlagRenderer::HangingBannerStyle {
  if (state == BuildingState::Damaged) {
    return {.pole_base = QVector3D(0.0F, 2.90F, 0.0F),
            .pole_height = 0.92F,
            .pole_radius = 0.042F,
            .banner_width = 0.52F,
            .banner_height = 0.32F,
            .beam_inset = 0.04F,
            .banner_depth = 0.02F,
            .banner_z_offset = 0.0F,
            .connector_drop_ratio = 0.28F,
            .capture_lowering_ratio = 0.45F,
            .pole_color = QVector3D(),
            .beam_color = QVector3D(),
            .connector_color = QVector3D(),
            .ornament_offset = QVector3D(0.18F, 0.98F, 0.0F),
            .ornament_size = QVector3D(0.16F, 0.025F, 0.015F),
            .ornament_color = QVector3D(),
            .ring_count = 1,
            .ring_y_start = 0.34F,
            .ring_spacing = 0.18F,
            .ring_height = 0.022F,
            .ring_radius_scale = 1.8F,
            .ring_color = QVector3D()};
  }

  return {.pole_base = QVector3D(0.0F, 4.24F, 0.0F),
          .pole_height = 1.18F,
          .pole_radius = 0.044F,
          .banner_width = 0.68F,
          .banner_height = 0.42F,
          .beam_inset = 0.05F,
          .banner_depth = 0.02F,
          .banner_z_offset = 0.0F,
          .connector_drop_ratio = 0.30F,
          .capture_lowering_ratio = 0.50F,
          .pole_color = QVector3D(),
          .beam_color = QVector3D(),
          .connector_color = QVector3D(),
          .ornament_offset = QVector3D(0.20F, 1.22F, 0.0F),
          .ornament_size = QVector3D(0.22F, 0.03F, 0.015F),
          .ornament_color = QVector3D(),
          .ring_count = 2,
          .ring_y_start = 0.48F,
          .ring_spacing = 0.30F,
          .ring_height = 0.024F,
          .ring_radius_scale = 1.9F,
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
  style.pole_color = palette.cedar_dark;
  style.beam_color = palette.cedar;
  style.connector_color = palette.marble;
  style.ornament_color = palette.bronze;
  style.ring_color = palette.gold;

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
    TorchMount{.at = QVector3D(-0.375F, 1.10F, 0.803F),
               .outward = QVector3D(-0.423F, 0.0F, 0.906F)},
    TorchMount{.at = QVector3D(0.375F, 1.10F, 0.803F),
               .outward = QVector3D(0.423F, 0.0F, 0.906F)},
}};

} // namespace

void register_defense_tower_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_defense_tower_renderer_variant(
      registry,
      DefenseTowerRendererConfig{.nation_slug = "roman",
                                 .archetype = &tower_archetype,
                                 .draw_banner = &draw_tower_banner_for_team,
                                 .selection = BuildingSelectionStyle{1.6F, 1.6F},
                                 .night_brazier_deck_y = 3.20F,
                                 .night_brazier_offset = 0.62F,
                                 .torches = k_torches,
                                 .lookout_deck_top = 3.07F,
                                 .lookout_deck_top_damaged = 2.67F,
                                 .lookout_walk_half = 0.40F});
}

} // namespace Render::GL::Roman
