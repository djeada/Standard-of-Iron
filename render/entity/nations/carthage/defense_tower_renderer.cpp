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

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D stone_light = BuildingPalette::k_sandstone_light;
  QVector3D stone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D stone_base = BuildingPalette::k_sandstone;
  QVector3D brick = BuildingPalette::k_brick;
  QVector3D brick_dark = BuildingPalette::k_brick_dark;
  QVector3D tile_red = BuildingPalette::k_tile_red;
  QVector3D wood = BuildingPalette::k_wood;
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D iron{0.24F, 0.23F, 0.21F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D ember{0.68F, 0.22F, 0.045F};
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
  BuildingArchetypeDesc desc("carthage_defense_tower");

  const bool damaged = state == BuildingState::Damaged;
  const bool destroyed = state == BuildingState::Destroyed;
  const float core_half_y = destroyed ? 0.46F : (damaged ? 0.80F : 0.98F);
  const float core_top = 0.5F + core_half_y * 2.0F;
  const float upper_drum_y = destroyed ? 0.0F : (damaged ? 1.86F : 2.16F);
  const float deck_y = destroyed ? 0.0F : (damaged ? 2.20F : 2.52F);
  const float roof_y = destroyed ? 0.0F : (damaged ? 2.50F : 2.94F);

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.24F, 0.04F, 1.24F), c.stone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.14F, 0.08F, 1.14F), c.stone_base);
  desc.add_box(
      QVector3D(0.0F, 0.22F, 0.0F), QVector3D(1.06F, 0.10F, 1.06F), c.stone_base);

  for (float x = -0.92F; x <= 0.92F; x += 0.46F) {
    desc.add_box(QVector3D(x, 0.38F, -0.90F),
                 QVector3D(0.14F, 0.08F, 0.10F),
                 c.brick_dark,
                 BuildingStateMask::All);
    desc.add_box(QVector3D(x, 0.38F, 0.90F),
                 QVector3D(0.14F, 0.08F, 0.10F),
                 c.brick_dark,
                 BuildingStateMask::All);
  }
  for (float z = -0.84F; z <= 0.84F; z += 0.42F) {
    desc.add_box(QVector3D(-0.90F, 0.38F, z),
                 QVector3D(0.10F, 0.08F, 0.14F),
                 c.brick_dark,
                 BuildingStateMask::All);
    desc.add_box(QVector3D(0.90F, 0.38F, z),
                 QVector3D(0.10F, 0.08F, 0.14F),
                 c.brick_dark,
                 BuildingStateMask::All);
  }

  desc.add_box(
      QVector3D(0.0F, 0.52F, 0.0F), QVector3D(0.92F, 0.10F, 0.92F), c.stone_light);
  desc.add_box(
      QVector3D(0.0F, 0.52F + core_half_y, 0.0F),
      QVector3D(destroyed ? 0.68F : 0.78F, core_half_y, destroyed ? 0.68F : 0.78F),
      c.stone_light);

  {
    const float core_half = destroyed ? 0.68F : 0.78F;
    const float core_top_y = 0.52F + core_half_y * 2.0F;
    for (float y = 0.86F; y < core_top_y - 0.10F; y += 0.32F) {
      desc.add_box(QVector3D(0.0F, y, 0.0F),
                   QVector3D(core_half + 0.006F, 0.011F, core_half + 0.006F),
                   c.stone_base * 0.86F);
    }
  }

  if (!destroyed) {
    add_embrasures(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        damaged ? 1.05F : 1.20F,
        (destroyed ? 0.68F : 0.78F) * 0.98F,
        QVector3D(0.05F, 0.18F, 0.05F),
        c.brick_dark * 0.5F);

    desc.add_box(QVector3D(0.0F, damaged ? 1.22F : 1.40F, 0.0F),
                 QVector3D(damaged ? 0.76F : 0.82F, 0.04F, damaged ? 0.76F : 0.82F),
                 c.stone_dark,
                 BuildingStateMask::All);

    desc.add_box(QVector3D(0.0F, damaged ? 1.62F : 1.86F, 0.0F),
                 QVector3D(damaged ? 0.72F : 0.80F, 0.03F, damaged ? 0.72F : 0.80F),
                 c.brick_dark,
                 BuildingStateMask::All);
  }

  const std::array<int, 4> corner_indices =
      damaged ? std::array<int, 4>{0, 1, 3, -1} : std::array<int, 4>{0, 1, 2, 3};
  for (int const index : corner_indices) {
    if (index < 0) {
      continue;
    }
    const float angle = static_cast<float>(index) * 1.57F;
    const float ox = std::sin(angle) * 0.68F;
    const float oz = std::cos(angle) * 0.68F;
    desc.add_cylinder(QVector3D(ox, 0.4F, oz),
                      QVector3D(ox, damaged ? 2.26F : 2.58F, oz),
                      damaged ? 0.14F : 0.16F,
                      c.stone_dark);

    desc.add_box(QVector3D(ox, damaged ? 2.32F : 2.64F, oz),
                 QVector3D(damaged ? 0.18F : 0.22F, 0.08F, damaged ? 0.18F : 0.22F),
                 c.stone_light,
                 BuildingStateMask::All);

    if (!destroyed && !damaged) {
      desc.add_box(QVector3D(ox, 2.76F, oz),
                   QVector3D(0.10F, 0.08F, 0.10F),
                   c.brick,
                   BuildingStateMask::All);
    }
  }

  if (!destroyed) {
    const int buttress_count = damaged ? 2 : 4;
    for (int i = 0; i < buttress_count; ++i) {
      const float angle =
          static_cast<float>(i) * (damaged ? std::numbers::pi_v<float> : 1.57F) +
          0.785F;
      const float ox = std::sin(angle) * 0.64F;
      const float oz = std::cos(angle) * 0.64F;

      desc.add_box(QVector3D(ox, damaged ? 0.64F : 0.72F, oz),
                   QVector3D(0.14F, damaged ? 0.22F : 0.30F, 0.14F),
                   c.brick,
                   BuildingStateMask::All);

      desc.add_box(QVector3D(ox, damaged ? 0.92F : 1.08F, oz),
                   QVector3D(0.10F, damaged ? 0.14F : 0.20F, 0.10F),
                   c.brick_dark,
                   BuildingStateMask::All);
    }

    desc.add_box(QVector3D(0.0F, upper_drum_y, 0.0F),
                 QVector3D(damaged ? 0.78F : 0.86F, 0.08F, damaged ? 0.78F : 0.86F),
                 c.brick_dark);

    for (int i = 0; i < (damaged ? 2 : 4); ++i) {
      const float angle =
          static_cast<float>(i) * (damaged ? std::numbers::pi_v<float> : 1.57F);
      const float sx = std::sin(angle) * (damaged ? 0.72F : 0.80F);
      const float sz = std::cos(angle) * (damaged ? 0.72F : 0.80F);
      desc.add_box(QVector3D(sx, upper_drum_y - 0.12F, sz),
                   QVector3D(0.02F, 0.10F, 0.04F),
                   c.wood_dark,
                   BuildingStateMask::All);
    }

    desc.add_box(QVector3D(0.0F, damaged ? 2.30F : 2.46F, 0.0F),
                 QVector3D(damaged ? 0.94F : 1.04F, 0.04F, damaged ? 0.94F : 1.04F),
                 c.stone_dark,
                 BuildingStateMask::All);
    desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                 QVector3D(damaged ? 0.90F : 1.00F, 0.05F, damaged ? 0.90F : 1.00F),
                 c.wood);

    const float parapet_half = damaged ? 0.78F : 0.88F;
    const float merlon_y = damaged ? 2.44F : 2.72F;
    auto add_part =
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        };

    add_square_parapet(add_part,
                       merlon_y,
                       parapet_half,
                       damaged ? 3 : 4,
                       QVector3D(0.12F, damaged ? 0.16F : 0.20F, 0.12F),
                       c.stone_light,
                       23);
    add_square_parapet(add_part,
                       merlon_y + (damaged ? 0.17F : 0.21F),
                       parapet_half,
                       damaged ? 3 : 4,
                       QVector3D(0.135F, 0.03F, 0.135F),
                       c.brick_dark,
                       23);

    if (!damaged) {

      for (const float ox : {-parapet_half, parapet_half}) {
        for (const float oz : {-parapet_half, parapet_half}) {
          add_part(
              QVector3D(ox, 2.94F, oz), QVector3D(0.08F, 0.06F, 0.08F), c.brick_dark);
        }
      }
    }

    const float canopy_y = roof_y + (damaged ? 0.22F : 0.30F);
    const float post_half = parapet_half - 0.12F;
    for (const float px : {-post_half, post_half}) {
      for (const float pz : {-post_half, post_half}) {
        desc.add_box(QVector3D(px, (merlon_y + canopy_y) * 0.5F, pz),
                     QVector3D(0.055F, (canopy_y - merlon_y) * 0.5F, 0.055F),
                     c.wood_dark,
                     BuildingStateMask::All);
      }
    }
    desc.add_box(QVector3D(0.0F, canopy_y, 0.0F),
                 QVector3D(damaged ? 0.70F : 0.80F, 0.03F, damaged ? 0.70F : 0.80F),
                 c.tile_red);
    desc.add_box(QVector3D(0.0F, canopy_y + 0.05F, 0.0F),
                 QVector3D(damaged ? 0.48F : 0.56F, 0.03F, damaged ? 0.48F : 0.56F),
                 c.tile_red,
                 BuildingStateMask::All);
  }

  if (damaged) {
    desc.add_box(
        QVector3D(0.44F, 1.92F, 0.32F), QVector3D(0.20F, 0.12F, 0.18F), c.brick_dark);
    desc.add_box(
        QVector3D(-0.34F, 1.66F, -0.36F), QVector3D(0.18F, 0.14F, 0.16F), c.stone_dark);
  }

  if (destroyed) {
    desc.add_box(
        QVector3D(0.0F, 1.36F, 0.0F), QVector3D(0.74F, 0.10F, 0.54F), c.brick_dark);
    desc.add_box(
        QVector3D(0.40F, 1.02F, 0.36F), QVector3D(0.22F, 0.14F, 0.20F), c.brick);
    desc.add_box(
        QVector3D(-0.38F, 0.96F, -0.32F), QVector3D(0.22F, 0.12F, 0.18F), c.stone_dark);
    desc.add_box(
        QVector3D(0.22F, 0.82F, -0.42F), QVector3D(0.16F, 0.10F, 0.14F), c.brick_dark);
  }

  if (!destroyed) {
    add_punic_tanit_relief(
        desc,
        QVector3D(0.0F, damaged ? 1.36F : 1.52F, damaged ? 0.70F : 0.80F),
        BuildingFacadePlane::XY,
        damaged ? 0.58F : 0.72F,
        c.brick,
        c.stone_dark);
    add_punic_horned_crown(desc,
                           QVector3D(0.0F, roof_y + 0.05F, 0.0F),
                           damaged ? 0.54F : 0.78F,
                           c.iron,
                           c.bronze,
                           c.ember);
  }

  add_broken_rim(desc,
                 BrokenRim{.center = QVector3D(0.0F, core_top - 0.16F, 0.0F),
                           .color = c.stone_dark,
                           .radius = 0.66F,
                           .chunk_half = 0.14F,
                           .rise = 0.13F,
                           .count = 7,
                           .seed = 397});

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(0.98F, 0.0F, 0.98F),
                                 .apron_extent = QVector3D(1.46F, 0.0F, 1.46F),
                                 .stone = c.stone_base,
                                 .stone_dark = c.stone_dark,
                                 .timber = c.wood_dark,
                                 .ground_y = 0.34F,
                                 .scale = 0.95F,
                                 .seed = 397});

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
      DefenseTowerRendererConfig{.nation_slug = "carthage",
                                 .archetype = &tower_archetype,
                                 .draw_banner = &draw_tower_banner_for_team,
                                 .selection = BuildingSelectionStyle{1.6F, 1.6F},
                                 .night_brazier_deck_y = 2.68F,
                                 .night_brazier_offset = 0.60F});
}

} // namespace Render::GL::Carthage
