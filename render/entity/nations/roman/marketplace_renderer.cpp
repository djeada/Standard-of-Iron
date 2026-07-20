#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../../template_cache.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../marketplace_renderer_common.h"
#include "../../registry.h"

namespace Render::GL::Roman {
namespace {

struct RomanMarketPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.40F, 0.28F, 0.18F};
  QVector3D cloth_red{0.72F, 0.18F, 0.14F};
  QVector3D cloth_gold{0.85F, 0.72F, 0.28F};
  QVector3D stone_base{0.80F, 0.76F, 0.70F};
  QVector3D terracotta{0.82F, 0.62F, 0.45F};
  QVector3D terracotta_dark{0.68F, 0.48F, 0.32F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  RomanMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("roman_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.40F, 0.04F, 1.40F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.32F, 0.02F, 1.32F), c.limestone_shade);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.24F, 0.02F, 1.24F), c.limestone);

  desc.add_box(QVector3D(0.0F, 0.165F, 0.0F),
               QVector3D(0.90F, 0.005F, 0.90F),
               c.marble,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.168F, 0.0F),
               QVector3D(0.60F, 0.005F, 0.60F),
               c.blue_accent,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.170F, 0.0F),
               QVector3D(0.30F, 0.005F, 0.30F),
               c.gold,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  float const wall_h = 0.78F * height_multiplier;
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.16F, 0.0F),
               QVector3D(0.12F, wall_h * 0.5F, 1.10F),
               c.limestone);

  desc.add_box(QVector3D(0.92F, wall_h + 0.18F, 0.0F),
               QVector3D(0.14F, 0.04F, 1.14F),
               c.limestone_shade,
               k_building_state_mask_intact);

  float const col_height = 0.82F * height_multiplier;
  float const col_radius = 0.05F;
  for (float const cz : {-0.80F, -0.27F, 0.27F, 0.80F}) {

    desc.add_box(QVector3D(-0.96F, 0.18F, cz),
                 QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F),
                 c.marble,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);

    desc.add_cylinder(QVector3D(-0.96F, 0.16F, cz),
                      QVector3D(-0.96F, 0.16F + col_height, cz),
                      col_radius,
                      c.limestone_shade);

    desc.add_box(QVector3D(-0.96F, 0.16F + col_height + 0.04F, cz),
                 QVector3D(col_radius * 1.5F, 0.05F, col_radius * 1.5F),
                 c.marble,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  float const entab_y = 0.16F + col_height + 0.10F;
  desc.add_box(QVector3D(-0.96F, entab_y, 0.0F),
               QVector3D(0.08F, 0.05F, 0.92F),
               c.limestone,
               k_building_state_mask_intact);

  float const counter_h = 0.44F * height_multiplier;
  desc.add_box(
      QVector3D(-0.20F, counter_h, 0.0F), QVector3D(0.62F, 0.05F, 0.86F), c.cedar);

  desc.add_box(QVector3D(-0.20F, counter_h * 0.5F, -0.78F),
               QVector3D(0.04F, counter_h * 0.5F - 0.04F, 0.04F),
               c.cedar_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-0.20F, counter_h * 0.5F, 0.78F),
               QVector3D(0.04F, counter_h * 0.5F - 0.04F, 0.04F),
               c.cedar_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  float const awning_y = col_height + 0.20F;
  desc.add_box(QVector3D(-0.20F, awning_y, 0.0F),
               QVector3D(0.62F, 0.02F, 0.88F),
               c.cloth_red,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(-0.20F, awning_y - 0.03F, 0.86F),
               QVector3D(0.62F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_box(QVector3D(-0.20F, awning_y - 0.03F, -0.86F),
               QVector3D(0.62F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(0.92F, wall_h + 0.26F, 0.0F),
               QVector3D(0.20F, 0.04F, 1.08F),
               c.terracotta,
               k_building_state_mask_intact);
  add_tile_rows_z(
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        desc.add_box(
            center, size, color, k_building_state_mask_intact, BuildingLODMask::Full);
      },
      wall_h + 0.32F,
      -0.88F,
      0.88F,
      0.44F,
      QVector3D(0.18F, 0.02F, 0.06F),
      c.terracotta_dark);

  float const hall_roof_y = wall_h + 0.38F;
  desc.add_rotated_box(QVector3D(0.72F, hall_roof_y, 0.0F),
                       QVector3D(0.38F, 0.055F, 1.18F),
                       QVector3D(0.0F, 0.0F, 12.0F),
                       c.terracotta,
                       k_building_state_mask_intact);
  desc.add_rotated_box(QVector3D(0.72F, hall_roof_y + 0.01F, 0.0F),
                       QVector3D(0.38F, 0.055F, 1.18F),
                       QVector3D(0.0F, 0.0F, -12.0F),
                       c.terracotta_dark,
                       k_building_state_mask_intact,
                       BuildingLODMask::Full);
  for (float const z : {-0.86F, -0.43F, 0.0F, 0.43F, 0.86F}) {
    desc.add_cylinder(QVector3D(0.34F, 0.16F, z),
                      QVector3D(0.34F, 0.16F + col_height * 0.82F, z),
                      0.045F,
                      c.marble,
                      k_building_state_mask_intact);
    desc.add_box(QVector3D(0.34F, 0.18F, z),
                 QVector3D(0.065F, 0.025F, 0.065F),
                 c.limestone_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (float const z : {-0.52F, 0.52F}) {
    desc.add_box(QVector3D(-0.35F, 0.34F, z),
                 QVector3D(0.42F, 0.055F, 0.22F),
                 c.cedar,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
    for (float const x : {-0.68F, -0.02F}) {
      desc.add_box(QVector3D(x, 0.22F, z),
                   QVector3D(0.035F, 0.17F, 0.035F),
                   c.cedar_dark,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
    for (float const x : {-0.58F, -0.36F, -0.14F}) {
      desc.add_cylinder(QVector3D(x, 0.40F, z),
                        QVector3D(x, 0.48F, z),
                        0.055F,
                        z < 0.0F ? c.terracotta : c.cloth_gold,
                        BuildingStateMask::Normal,
                        BuildingLODMask::Full);
    }
  }
  desc.add_box(QVector3D(-0.92F, 0.35F, 0.0F),
               QVector3D(0.13F, 0.20F, 0.13F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(-0.92F, 0.55F, 0.0F),
                    QVector3D(-0.92F, 0.82F, 0.0F),
                    0.055F,
                    c.gold,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);

  desc.add_box(QVector3D(0.50F, 0.28F, -0.70F),
               QVector3D(0.14F, 0.10F, 0.14F),
               c.cedar_dark,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.50F, 0.28F, 0.68F),
               QVector3D(0.12F, 0.10F, 0.12F),
               c.cedar,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(0.0F, counter_h + 0.05F, -0.45F),
                    QVector3D(0.0F, counter_h + 0.18F, -0.45F),
                    0.06F,
                    c.terracotta,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);

  desc.add_box(QVector3D(0.92F, wall_h * 0.6F + 0.16F, 0.0F),
               QVector3D(0.005F, 0.03F, 1.06F),
               c.blue_accent,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(-0.96F, entab_y + 0.06F, 0.0F),
               QVector3D(0.06F, 0.05F, 0.06F),
               c.gold,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_palette_box(QVector3D(0.96F, 0.56F * height_multiplier, 0.0F),
                       QVector3D(0.02F, 0.22F, 0.14F),
                       k_marketplace_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  return build_building_archetype(desc, state);
}

auto marketplace_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_marketplace_archetype);
  return k_set.for_state(state);
}

} // namespace

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_marketplace_renderer_variant(
      registry,
      MarketplaceRendererConfig{.nation_slug = "roman",
                                .archetype = &marketplace_archetype,
                                .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.2F},
                                .selection = BuildingSelectionStyle{1.8F, 1.8F}});
}

} // namespace Render::GL::Roman
