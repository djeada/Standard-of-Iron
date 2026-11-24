#include "render/entity/nations/carthage/defense_tower_authoring.h"

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
#include "render/entity/nation_motifs.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

using namespace defense_tower_detail;

namespace {

auto build_tower_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("carthage_defense_tower");

  const bool damaged = state == BuildingState::Damaged;
  const bool destroyed = state == BuildingState::Destroyed;
  const float core_half_y = destroyed ? 0.46F : (damaged ? 0.80F : 0.98F);
  const float core_top = 0.5F + core_half_y * 2.0F;
  const float upper_drum_y = destroyed ? 0.0F : (damaged ? 1.86F : 2.16F);
  const float deck_y = destroyed ? 0.0F : (damaged ? 2.20F : 2.52F);

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

  for (float z = -0.65F; z <= 0.65F; z += 0.325F) {
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
                   QVector3D(core_half + 0.004F, 0.005F, core_half + 0.004F),
                   c.stone_base * 0.95F);
    }
    if (!destroyed) {
      for (const float side : {-1.0F, 1.0F}) {
        desc.add_box(QVector3D(0.0F, 1.03F, side * (core_half + 0.025F)),
                     QVector3D(core_half + 0.035F, 0.022F, 0.025F),
                     c.brick,
                     k_building_state_mask_intact);
        desc.add_box(QVector3D(side * (core_half + 0.025F), 1.03F, 0.0F),
                     QVector3D(0.025F, 0.022F, core_half + 0.035F),
                     c.brick,
                     k_building_state_mask_intact);
      }
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
                 QVector3D(damaged ? 0.74F : 0.83F, 0.08F, damaged ? 0.74F : 0.83F),
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
    {
      BuildingPartMaterial const timber(desc, k_building_material_wood);
      desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                   QVector3D(damaged ? 0.90F : 1.00F, 0.05F, damaged ? 0.90F : 1.00F),
                   c.wood);
    }

    const float parapet_half = damaged ? 0.755F : 0.855F;
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
    add_square_parapet(add_part,
                       merlon_y + (damaged ? 0.26F : 0.30F),
                       parapet_half,
                       damaged ? 3 : 4,
                       QVector3D(0.066F, 0.06F, 0.066F),
                       c.stone_light,
                       23);

    if (!damaged) {

      for (const float ox : {-parapet_half, parapet_half}) {
        for (const float oz : {-parapet_half, parapet_half}) {
          add_part(
              QVector3D(ox, 2.94F, oz), QVector3D(0.08F, 0.06F, 0.08F), c.brick_dark);
        }
      }
    }

    for (const float side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.0F, deck_y - 0.02F, side * parapet_half),
                   QVector3D(parapet_half + 0.08F, 0.055F, 0.065F),
                   c.stone_light,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(side * parapet_half, deck_y - 0.02F, 0.0F),
                   QVector3D(0.065F, 0.055F, parapet_half + 0.08F),
                   c.stone_light,
                   k_building_state_mask_intact);
    }
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
    add_punic_tanit_relief(desc,

                           QVector3D(0.0F, damaged ? 1.36F : 1.52F, 0.82F),
                           BuildingFacadePlane::XY,
                           damaged ? 0.58F : 0.72F,
                           c.brick,
                           c.stone_dark);
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

  {
    // A stele and a burning stand at its foot.
    const Motif::Setter set(desc, state);
    Motif::add_punic_stele(desc, set.at(-0.52F, 1.04F, 0.8F), 0.95F, 1.0F, 6);
    Motif::add_punic_incense_stand(desc, set.at(0.52F, 1.04F, 0.8F), 0.80F);
  }
  return desc;
}

} // namespace

auto build_tower_desc(BuildingState state) -> BuildingArchetypeDesc {
  return build_tower_desc_impl(state);
}

} // namespace Render::GL::Carthage
