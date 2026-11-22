#include "render/entity/nations/roman/defense_tower_authoring.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Roman {

using namespace defense_tower_detail;

namespace {

auto build_tower_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("roman_defense_tower");

  const bool damaged = state == BuildingState::Damaged;
  const bool destroyed = state == BuildingState::Destroyed;

  const float shaft_top = destroyed ? 1.56F : (damaged ? 2.50F : 2.90F);
  const float shaft_radius = destroyed ? 0.76F : (damaged ? 0.84F : 0.90F);
  const float shaft_half = shaft_radius * 0.82F;
  const float belt_half = shaft_half + 0.035F;
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
    const float course_half = even ? shaft_half : shaft_half * 0.992F;
    desc.add_box(QVector3D(0.0F, y0 + (course_height + 0.005F) * 0.5F, 0.0F),
                 QVector3D(course_half, (course_height + 0.005F) * 0.5F, course_half),
                 weathered(c.limestone, i + 31, 0.045F));
  }

  for (const float angle : {0.0F, 1.5708F, 3.1416F, 4.7124F}) {
    desc.add_box(
        QVector3D(std::sin(angle) * shaft_half, 0.76F, std::cos(angle) * shaft_half),
        QVector3D(0.095F, 0.11F, 0.095F),
        c.limestone_shade,
        k_building_state_mask_intact);
  }

  if (!destroyed) {
    const float belt_low = 0.52F + (shaft_top - 0.52F) * 0.40F;
    const float belt_high = 0.52F + (shaft_top - 0.52F) * 0.72F;
    desc.add_box(QVector3D(0.0F, belt_low + 0.03F, 0.0F),
                 QVector3D(belt_half, 0.03F, belt_half),
                 c.marble);
    desc.add_box(QVector3D(0.0F, belt_high + 0.02F, 0.0F),
                 QVector3D(belt_half * 0.985F, 0.02F, belt_half * 0.985F),
                 c.limestone_shade);

    add_embrasures(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        belt_low - 0.28F,
        shaft_half - 0.004F,
        QVector3D(0.035F, 0.17F, 0.012F),
        c.limestone_dark * 0.45F);
    add_embrasures(
        [&desc](const QVector3D& centre, const QVector3D& half, const QVector3D& col) {
          desc.add_box(centre, half, col, BuildingStateMask::All);
        },
        belt_high + 0.26F,
        shaft_half - 0.004F,
        QVector3D(0.032F, 0.13F, 0.012F),
        c.limestone_dark * 0.45F);
  }

  const std::array<int, 4> corner_indices =
      damaged ? std::array<int, 4>{0, 2, -1, -1} : std::array<int, 4>{0, 1, 2, 3};
  for (int const index : corner_indices) {
    if (index < 0) {
      continue;
    }
    const float angle = static_cast<float>(index) * 1.57F + 0.785F;
    const float ox = (std::sin(angle) > 0.0F ? 1.0F : -1.0F) * (shaft_half - 0.02F);
    const float oz = (std::cos(angle) > 0.0F ? 1.0F : -1.0F) * (shaft_half - 0.02F);
    const float column_top = destroyed ? 0.82F : shaft_top - 0.15F;

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
    desc.add_box(QVector3D(0.0F, shaft_top + 0.05F, 0.0F),
                 QVector3D(shaft_half + 0.09F, 0.03F, shaft_half + 0.09F),
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

    {
      BuildingPartMaterial const timber(desc, k_building_material_wood);
      desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                   QVector3D(platform_radius, 0.05F, platform_radius),
                   c.cedar);
    }

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
      BuildingPartMaterial const timber(desc, k_building_material_wood);
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

      add_tiled_roof(desc,
                     QVector3D(0.0F, roof_y, 0.0F),
                     1.14F,
                     1.14F,
                     0.42F,
                     false,
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
    {
      BuildingPartMaterial const timber(desc, k_building_material_wood);
      desc.add_box(QVector3D(0.0F, 1.60F, 0.0F),
                   QVector3D(0.38F, 0.08F, 0.24F),
                   c.cedar_dark,
                   BuildingStateMask::All);
    }
    desc.add_box(
        QVector3D(0.28F, 1.36F, -0.20F), QVector3D(0.14F, 0.10F, 0.12F), c.terracotta);
  }

  if (!destroyed) {
    add_roman_aquila_relief(
        desc,
        QVector3D(0.0F, damaged ? 1.62F : 1.86F, shaft_half + 0.02F),
        BuildingFacadePlane::XY,
        damaged ? 0.54F : 0.68F,
        c.gold,
        c.terracotta_dark);
  }

  add_broken_rim(desc,
                 BrokenRim{.center = QVector3D(0.0F, shaft_top - 0.17F, 0.0F),
                           .color = c.limestone_dark,
                           .radius = shaft_half * 0.98F,
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

  return desc;
}

} // namespace

auto build_tower_desc(BuildingState state) -> BuildingArchetypeDesc {
  return build_tower_desc_impl(state);
}

} // namespace Render::GL::Roman
