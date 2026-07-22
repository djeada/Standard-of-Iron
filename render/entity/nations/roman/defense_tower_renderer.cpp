#include "defense_tower_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../../../../game/core/component.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../defense_tower_renderer_common.h"
#include "../../registry.h"
#include "math/math_utils.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D sandstone_light{0.82F, 0.75F, 0.62F};
  QVector3D sandstone_dark{0.70F, 0.62F, 0.50F};
  QVector3D sandstone_base{0.75F, 0.68F, 0.56F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D terracotta{0.78F, 0.32F, 0.18F};
  QVector3D terracotta_dark{0.46F, 0.12F, 0.07F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D blue_light{0.40F, 0.60F, 0.80F};
  QVector3D bronze{0.60F, 0.45F, 0.25F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

auto make_palette(const QVector3D& team) -> TowerPalette {
  TowerPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

auto build_tower_archetype(BuildingState state) -> RenderArchetype {
  TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("roman_defense_tower");

  const bool damaged = state == BuildingState::Damaged;
  const bool destroyed = state == BuildingState::Destroyed;
  const float shaft_top = destroyed ? 1.56F : (damaged ? 2.04F : 2.30F);
  const float shaft_radius = destroyed ? 0.52F : (damaged ? 0.58F : 0.62F);
  const float belt_radius = damaged ? 0.58F : 0.62F;
  const float platform_radius = damaged ? 0.72F : 0.82F;
  const float battlement_radius = damaged ? 0.60F : 0.72F;
  const float battlement_half_extent = damaged ? 0.13F : 0.15F;
  const float deck_y = destroyed ? 0.0F : shaft_top + 0.12F;
  const float battlement_y = destroyed ? 0.0F : shaft_top + (damaged ? 0.28F : 0.34F);

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.30F, 0.04F, 1.30F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.18F, 0.06F, 1.18F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.18F, 0.0F), QVector3D(1.08F, 0.08F, 1.08F), c.limestone);
  desc.add_box(QVector3D(0.0F, 0.28F, 0.0F), QVector3D(1.02F, 0.02F, 1.02F), c.marble);

  for (float x = -0.88F; x <= 0.88F; x += 0.44F) {
    for (float z = -0.88F; z <= 0.88F; z += 0.44F) {
      if (std::fabs(x) > 0.32F || std::fabs(z) > 0.32F) {
        desc.add_box(QVector3D(x, 0.31F, z),
                     QVector3D(0.18F, 0.01F, 0.18F),
                     c.terracotta,
                     BuildingStateMask::All,
                     BuildingLODMask::Full);
      }
    }
  }

  desc.add_box(
      QVector3D(0.0F, 0.44F, 0.0F), QVector3D(0.94F, 0.12F, 0.94F), c.sandstone_light);

  desc.add_cylinder(QVector3D(0.0F, 0.52F, 0.0F),
                    QVector3D(0.0F, shaft_top, 0.0F),
                    shaft_radius,
                    c.limestone);

  if (!destroyed) {
    desc.add_cylinder(QVector3D(0.0F, damaged ? 1.18F : 1.38F, 0.0F),
                      QVector3D(0.0F, damaged ? 1.24F : 1.44F, 0.0F),
                      belt_radius,
                      c.marble);

    desc.add_cylinder(QVector3D(0.0F, damaged ? 1.58F : 1.82F, 0.0F),
                      QVector3D(0.0F, damaged ? 1.62F : 1.86F, 0.0F),
                      belt_radius * 0.98F,
                      c.limestone_shade);
  }

  const std::array<int, 4> corner_indices =
      damaged ? std::array<int, 4>{0, 2, -1, -1} : std::array<int, 4>{0, 1, 2, 3};
  for (int const index : corner_indices) {
    if (index < 0) {
      continue;
    }
    const float angle = static_cast<float>(index) * 1.57F + 0.785F;
    const float ox = std::sin(angle) * 0.50F;
    const float oz = std::cos(angle) * 0.50F;
    const float column_top = destroyed ? 0.0F : shaft_top - 0.15F;

    desc.add_cylinder(
        QVector3D(ox, 0.52F, oz), QVector3D(ox, column_top, oz), 0.09F, c.marble);

    desc.add_box(QVector3D(ox, 0.62F, oz),
                 QVector3D(0.14F, 0.10F, 0.14F),
                 c.marble,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);

    desc.add_box(QVector3D(ox, column_top + 0.06F, oz),
                 QVector3D(0.15F, 0.10F, 0.15F),
                 c.marble,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);

    if (!damaged) {
      desc.add_box(QVector3D(ox, column_top + 0.14F, oz),
                   QVector3D(0.08F, 0.05F, 0.08F),
                   c.gold,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }
  }

  if (!destroyed) {
    const int spoke_count = damaged ? 2 : 4;
    for (int i = 0; i < spoke_count; ++i) {
      const float angle =
          static_cast<float>(i) * (damaged ? std::numbers::pi_v<float> : 1.57F);
      const float ox = std::sin(angle) * 0.60F;
      const float oz = std::cos(angle) * 0.60F;
      desc.add_box(QVector3D(ox, damaged ? 1.00F : 1.24F, oz),
                   QVector3D(0.04F, damaged ? 0.18F : 0.24F, 0.04F),
                   c.sandstone_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    desc.add_cylinder(QVector3D(0.0F, shaft_top + 0.02F, 0.0F),
                      QVector3D(0.0F, shaft_top + 0.08F, 0.0F),
                      damaged ? 0.62F : 0.66F,
                      c.limestone);
    desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                 QVector3D(platform_radius, 0.05F, platform_radius),
                 c.cedar);

    const int merlon_count = damaged ? 5 : 10;
    for (int i = 0; i < merlon_count; ++i) {
      const float angle =
          static_cast<float>(i) * (6.28318F / static_cast<float>(merlon_count));
      const float ox = std::sin(angle) * battlement_radius;
      const float oz = std::cos(angle) * battlement_radius;
      desc.add_box(QVector3D(ox, battlement_y, oz),
                   QVector3D(battlement_half_extent,
                             damaged ? 0.20F : 0.24F,
                             battlement_half_extent),
                   c.terracotta,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    desc.add_box(QVector3D(0.0F, battlement_y + 0.14F, 0.0F),
                 QVector3D(damaged ? 0.74F : 0.88F, 0.04F, damaged ? 0.74F : 0.88F),
                 c.limestone);

    if (!damaged) {
      for (float const x : {-0.78F, 0.78F}) {
        for (float const z : {-0.78F, 0.78F}) {
          desc.add_box(QVector3D(x, battlement_y + 0.22F, z),
                       QVector3D(0.07F, 0.08F, 0.07F),
                       c.blue_accent,
                       BuildingStateMask::All,
                       BuildingLODMask::Full);
        }
      }

      desc.add_box(QVector3D(0.0F, battlement_y + 0.24F, 0.0F),
                   QVector3D(0.10F, 0.10F, 0.10F),
                   c.gold,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
    }
  }

  if (damaged) {
    desc.add_box(QVector3D(0.48F, 1.82F, 0.24F),
                 QVector3D(0.18F, 0.12F, 0.14F),
                 c.sandstone_dark);
    desc.add_box(QVector3D(-0.40F, 1.68F, -0.44F),
                 QVector3D(0.16F, 0.14F, 0.18F),
                 c.limestone_dark);
  }

  if (destroyed) {
    desc.add_box(QVector3D(0.42F, 1.26F, 0.30F),
                 QVector3D(0.22F, 0.14F, 0.18F),
                 c.sandstone_dark);
    desc.add_box(QVector3D(-0.36F, 1.06F, -0.38F),
                 QVector3D(0.20F, 0.16F, 0.20F),
                 c.limestone_dark);
    desc.add_box(QVector3D(0.0F, 1.60F, 0.0F),
                 QVector3D(0.38F, 0.08F, 0.24F),
                 c.cedar_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.28F, 1.36F, -0.20F), QVector3D(0.14F, 0.10F, 0.12F), c.terracotta);
  }

  if (!destroyed) {
    add_roman_aquila_relief(
        desc,
        QVector3D(0.0F, damaged ? 1.43F : 1.62F, shaft_radius + 0.02F),
        BuildingFacadePlane::XY,
        damaged ? 0.54F : 0.68F,
        c.gold,
        c.blue_accent);
    add_roman_roof_standard(desc,
                            QVector3D(0.0F, battlement_y + 0.20F, 0.0F),
                            damaged ? 0.48F : 0.66F,
                            c.gold,
                            c.terracotta_dark);
  }

  return build_building_archetype(desc, state);
}

auto tower_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_tower_archetype);
  return k_set.for_state(state);
}

auto tower_health_bar_style(BuildingState state) -> BuildingHealthBarStyle {
  switch (state) {
  case BuildingState::Damaged:
    return BuildingHealthBarStyle{0.96F, 0.085F, 2.92F, true};
  case BuildingState::Destroyed:
    return BuildingHealthBarStyle{0.88F, 0.08F, 2.00F, true};
  case BuildingState::Normal:
  default:
    return BuildingHealthBarStyle{1.02F, 0.09F, 3.42F, true};
  }
}

auto tower_banner_style(BuildingState state)
    -> BarracksFlagRenderer::HangingBannerStyle {
  if (state == BuildingState::Damaged) {
    return {.pole_base = QVector3D(0.0F, 2.42F, 0.0F),
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

  return {.pole_base = QVector3D(0.0F, 2.74F, 0.0F),
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
  if (p.backend != nullptr) {
    cloth.cloth_mesh = p.backend->banner_mesh();
    cloth.banner_shader = p.backend->banner_shader();
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

} // namespace

void register_defense_tower_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_defense_tower_renderer_variant(
      registry,
      DefenseTowerRendererConfig{.nation_slug = "roman",
                                 .archetype = &tower_archetype,
                                 .health_bar_style = &tower_health_bar_style,
                                 .draw_banner = &draw_tower_banner_for_team,
                                 .selection = BuildingSelectionStyle{1.6F, 1.6F}});
}

} // namespace Render::GL::Roman
