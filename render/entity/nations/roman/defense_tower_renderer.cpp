#include "defense_tower_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "building_palette.h"
#include "game/core/component.h"
#include "math/math_utils.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/defense_tower_renderer_common.h"
#include "render/entity/registry.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D sandstone_light{0.82F, 0.75F, 0.62F};
  QVector3D sandstone_dark{0.70F, 0.62F, 0.50F};
  QVector3D sandstone_base{0.75F, 0.68F, 0.56F};
  QVector3D marble = BuildingPalette::k_marble;
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D blue_light = BuildingPalette::k_blue_light;
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D gold = BuildingPalette::k_gold;
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

  const float shaft_top = destroyed ? 1.56F : (damaged ? 2.50F : 2.90F);
  const float shaft_radius = destroyed ? 0.76F : (damaged ? 0.84F : 0.90F);
  const float belt_radius = (damaged ? 0.86F : 0.93F);
  const float platform_radius = damaged ? 0.96F : 1.06F;
  const float battlement_radius = damaged ? 0.88F : 0.98F;
  const float battlement_half_extent = damaged ? 0.13F : 0.15F;
  const float deck_y = destroyed ? 0.0F : shaft_top + 0.12F;
  const float battlement_y = destroyed ? 0.0F : shaft_top + (damaged ? 0.28F : 0.34F);
  const QVector3D merlon_cap = c.limestone_dark * 0.62F;

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
                     c.sandstone_base,
                     BuildingStateMask::All);
      }
    }
  }

  desc.add_box(
      QVector3D(0.0F, 0.44F, 0.0F), QVector3D(0.94F, 0.12F, 0.94F), c.sandstone_light);

  constexpr int k_courses = 6;
  const float course_height = (shaft_top - 0.52F) / static_cast<float>(k_courses);
  for (int i = 0; i < k_courses; ++i) {
    const float y0 = 0.52F + course_height * static_cast<float>(i);
    const bool even = (i % 2) == 0;
    desc.add_cylinder(QVector3D(0.0F, y0, 0.0F),
                      QVector3D(0.0F, y0 + course_height + 0.005F, 0.0F),
                      even ? shaft_radius : shaft_radius * 0.985F,
                      even ? c.limestone : c.limestone_shade);
  }

  if (!destroyed) {
    const float belt_low = 0.52F + (shaft_top - 0.52F) * 0.40F;
    const float belt_high = 0.52F + (shaft_top - 0.52F) * 0.72F;
    desc.add_cylinder(QVector3D(0.0F, belt_low, 0.0F),
                      QVector3D(0.0F, belt_low + 0.06F, 0.0F),
                      belt_radius,
                      c.marble);
    desc.add_cylinder(QVector3D(0.0F, belt_high, 0.0F),
                      QVector3D(0.0F, belt_high + 0.04F, 0.0F),
                      belt_radius * 0.98F,
                      c.limestone_shade);

    add_embrasures(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        belt_low - 0.28F,
        shaft_radius * 0.94F,
        QVector3D(0.045F, 0.17F, 0.05F),
        c.limestone_dark * 0.45F);
    add_embrasures(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        belt_high + 0.26F,
        shaft_radius * 0.90F,
        QVector3D(0.04F, 0.13F, 0.05F),
        c.limestone_dark * 0.45F);
  }

  const std::array<int, 4> corner_indices =
      damaged ? std::array<int, 4>{0, 2, -1, -1} : std::array<int, 4>{0, 1, 2, 3};
  for (int const index : corner_indices) {
    if (index < 0) {
      continue;
    }
    const float angle = static_cast<float>(index) * 1.57F + 0.785F;
    const float ox = std::sin(angle) * (shaft_radius - 0.04F);
    const float oz = std::cos(angle) * (shaft_radius - 0.04F);
    const float column_top = destroyed ? 0.0F : shaft_top - 0.15F;

    desc.add_cylinder(
        QVector3D(ox, 0.52F, oz), QVector3D(ox, column_top, oz), 0.09F, c.marble);

    desc.add_box(QVector3D(ox, 0.62F, oz),
                 QVector3D(0.14F, 0.10F, 0.14F),
                 c.marble,
                 BuildingStateMask::All);

    desc.add_box(QVector3D(ox, column_top + 0.06F, oz),
                 QVector3D(0.15F, 0.10F, 0.15F),
                 c.marble,
                 BuildingStateMask::All);
  }

  if (!destroyed) {
    desc.add_cylinder(QVector3D(0.0F, shaft_top + 0.02F, 0.0F),
                      QVector3D(0.0F, shaft_top + 0.08F, 0.0F),
                      damaged ? 0.90F : 0.96F,
                      c.limestone);

    const int corbel_count = damaged ? 8 : 12;
    for (int i = 0; i < corbel_count; ++i) {
      const float angle =
          static_cast<float>(i) * (6.28318F / static_cast<float>(corbel_count));
      desc.add_box(QVector3D(std::sin(angle) * (platform_radius - 0.10F),
                             deck_y - 0.09F,
                             std::cos(angle) * (platform_radius - 0.10F)),
                   QVector3D(0.07F, 0.05F, 0.07F),
                   c.limestone_shade,
                   BuildingStateMask::All);
    }

    desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                 QVector3D(platform_radius, 0.05F, platform_radius),
                 c.cedar);

    add_square_parapet(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        battlement_y,
        battlement_radius,
        damaged ? 3 : 4,
        QVector3D(
            battlement_half_extent, damaged ? 0.20F : 0.24F, battlement_half_extent),
        c.limestone,
        11);
    add_square_parapet(
        [&desc,
         merlon_cap](const QVector3D& centre, const QVector3D& half, const QVector3D&) {
          desc.add_box(centre, half, merlon_cap, BuildingStateMask::All);
        },
        battlement_y + (damaged ? 0.21F : 0.25F),
        battlement_radius,
        damaged ? 3 : 4,
        QVector3D(
            battlement_half_extent + 0.015F, 0.03F, battlement_half_extent + 0.015F),
        merlon_cap,
        11);

    if (!damaged) {

      const float roof_y = battlement_y + 0.62F;
      const float post_offset = battlement_radius - 0.30F;
      for (const float px : {-post_offset, post_offset}) {
        for (const float pz : {-post_offset, post_offset}) {
          desc.add_box(QVector3D(px, (deck_y + roof_y) * 0.5F, pz),
                       QVector3D(0.05F, (roof_y - deck_y) * 0.5F, 0.05F),
                       c.cedar_dark,
                       BuildingStateMask::Normal);
        }
      }
      desc.add_box(QVector3D(0.0F, roof_y - 0.03F, 0.0F),
                   QVector3D(1.10F, 0.025F, 1.10F),
                   c.cedar_dark,
                   BuildingStateMask::Normal);

      constexpr int k_roof_steps = 6;
      const QVector3D tile = c.terracotta * 0.94F;
      for (int i = 0; i < k_roof_steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_roof_steps);
        const float half = 1.14F - (1.14F - 0.26F) * t;
        desc.add_box(QVector3D(0.0F, roof_y + 0.056F * static_cast<float>(i), 0.0F),
                     QVector3D(half, 0.032F, half),
                     (i % 2 == 0) ? tile : tile * 0.93F,
                     BuildingStateMask::Normal);
      }
      desc.add_box(QVector3D(0.0F, roof_y + 0.34F, 0.0F),
                   QVector3D(0.14F, 0.04F, 0.14F),
                   c.terracotta_dark,
                   BuildingStateMask::Normal);
    }
  }

  if (damaged) {
    desc.add_box(QVector3D(0.48F, 2.22F, 0.24F),
                 QVector3D(0.18F, 0.12F, 0.14F),
                 c.sandstone_dark);
    desc.add_box(QVector3D(-0.40F, 2.08F, -0.44F),
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
                 BuildingStateMask::All);
    desc.add_box(
        QVector3D(0.28F, 1.36F, -0.20F), QVector3D(0.14F, 0.10F, 0.12F), c.terracotta);
  }

  if (!destroyed) {
    add_roman_aquila_relief(
        desc,
        QVector3D(0.0F, damaged ? 1.62F : 1.86F, shaft_radius + 0.02F),
        BuildingFacadePlane::XY,
        damaged ? 0.54F : 0.68F,
        c.gold,
        c.terracotta_dark);
  }

  add_broken_rim(desc,
                 BrokenRim{.center = QVector3D(0.0F, shaft_top - 0.17F, 0.0F),
                           .color = c.limestone_dark,
                           .radius = shaft_radius * 0.94F,
                           .chunk_half = 0.15F,
                           .rise = 0.13F,
                           .count = 8,
                           .seed = 353});

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(0.98F, 0.0F, 0.98F),
                                 .apron_extent = QVector3D(1.46F, 0.0F, 1.46F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.limestone_dark * 0.5F,
                                 .ground_y = 0.34F,
                                 .scale = 0.95F,
                                 .seed = 353});

  return build_building_archetype(desc, state);
}

auto tower_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_tower_archetype);
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
                                 .draw_banner = &draw_tower_banner_for_team,
                                 .selection = BuildingSelectionStyle{1.6F, 1.6F},
                                 .night_brazier_deck_y = 3.20F,
                                 .night_brazier_offset = 0.62F});
}

} // namespace Render::GL::Roman
