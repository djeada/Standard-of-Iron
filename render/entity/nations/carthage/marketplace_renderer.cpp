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

namespace Render::GL::Carthage {
namespace {

struct CarthageMarketPalette {
  QVector3D sandstone{0.88F, 0.82F, 0.68F};
  QVector3D sandstone_dark{0.76F, 0.70F, 0.56F};
  QVector3D wood_dark{0.36F, 0.24F, 0.14F};
  QVector3D wood_medium{0.48F, 0.34F, 0.22F};
  QVector3D cloth_purple{0.45F, 0.15F, 0.42F};
  QVector3D cloth_gold{0.82F, 0.68F, 0.22F};
  QVector3D brick{0.72F, 0.52F, 0.38F};
  QVector3D brick_dark{0.60F, 0.42F, 0.30F};
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D ceramic{0.78F, 0.58F, 0.36F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  CarthageMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.35F, 0.05F, 1.35F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.28F, 0.02F, 1.28F), c.sandstone);

  float const wall_h = 0.72F * height_multiplier;
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.14F, wall_h * 0.5F, 1.10F),
               c.sandstone);
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.16F, wall_h * 0.35F, 1.14F),
               c.sandstone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  if (state != BuildingState::Destroyed) {
    float const merlon_y = wall_h + 0.18F;
    auto add_merlon =
        [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
          desc.add_box(
              center, size, color, k_building_state_mask_intact, BuildingLODMask::Full);
        };
    add_merlon_strip_z(add_merlon,
                       0.92F,
                       merlon_y,
                       -0.90F,
                       0.36F,
                       6,
                       QVector3D(0.10F, 0.10F, 0.12F),
                       c.brick);
  }

  float const side_wall_h = 0.44F * height_multiplier;
  desc.add_box(QVector3D(0.20F, side_wall_h * 0.5F + 0.14F, -1.08F),
               QVector3D(0.60F, side_wall_h * 0.5F, 0.08F),
               c.brick,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.20F, side_wall_h * 0.5F + 0.14F, 1.08F),
               QVector3D(0.60F, side_wall_h * 0.5F, 0.08F),
               c.brick,
               k_building_state_mask_intact);

  float const counter_h = 0.42F * height_multiplier;
  desc.add_box(QVector3D(-0.30F, counter_h, 0.0F),
               QVector3D(0.60F, 0.05F, 0.80F),
               c.wood_medium);
  desc.add_box(QVector3D(-0.30F, counter_h * 0.5F, 0.0F),
               QVector3D(0.58F, counter_h * 0.5F - 0.05F, 0.04F),
               c.wood_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(-0.30F, counter_h - 0.08F, -0.78F),
               QVector3D(0.58F, 0.08F, 0.03F),
               c.wood_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  float const post_h = 0.88F * height_multiplier;
  desc.add_box(QVector3D(-0.82F, post_h * 0.5F, -0.72F),
               QVector3D(0.05F, post_h * 0.5F, 0.05F),
               c.wood_dark);
  desc.add_box(QVector3D(-0.82F, post_h * 0.5F, 0.72F),
               QVector3D(0.05F, post_h * 0.5F, 0.05F),
               c.wood_dark);
  desc.add_box(QVector3D(0.18F, post_h * 0.5F, -0.72F),
               QVector3D(0.05F, post_h * 0.5F, 0.05F),
               c.wood_dark);
  desc.add_box(QVector3D(0.18F, post_h * 0.5F, 0.72F),
               QVector3D(0.05F, post_h * 0.5F, 0.05F),
               c.wood_dark);

  float const awning_y = post_h + 0.04F;
  desc.add_box(QVector3D(-0.32F, awning_y, 0.0F),
               QVector3D(0.58F, 0.02F, 0.82F),
               c.cloth_purple,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(-0.32F, awning_y - 0.03F, 0.80F),
               QVector3D(0.58F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_box(QVector3D(-0.32F, awning_y - 0.03F, -0.80F),
               QVector3D(0.58F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(0.70F, wall_h * 0.52F + 0.14F, -0.82F),
               QVector3D(0.32F, wall_h * 0.52F, 0.28F),
               c.brick,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.70F, wall_h + 0.20F, -0.82F),
               QVector3D(0.38F, 0.06F, 0.34F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.70F, wall_h + 0.29F, -0.82F),
               QVector3D(0.34F, 0.07F, 0.30F),
               c.brick_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  for (float const z : {-0.94F, -0.70F}) {
    desc.add_box(QVector3D(0.34F, 0.52F, z),
                 QVector3D(0.025F, 0.30F, 0.15F),
                 c.wood_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (float const z : {-0.48F, 0.48F}) {
    desc.add_box(QVector3D(-0.38F, 0.70F, z),
                 QVector3D(0.72F, 0.025F, 0.25F),
                 c.cloth_purple,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-0.38F, 0.665F, z + (z < 0.0F ? -0.23F : 0.23F)),
                 QVector3D(0.72F, 0.025F, 0.035F),
                 c.cloth_gold,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
    for (float const x : {-0.98F, 0.22F}) {
      desc.add_box(QVector3D(x, 0.40F, z),
                   QVector3D(0.035F, 0.40F, 0.035F),
                   c.wood_dark,
                   k_building_state_mask_intact);
    }
  }

  for (float const x : {-0.78F, -0.56F, -0.34F}) {
    desc.add_cylinder(QVector3D(x, 0.14F, 0.84F),
                      QVector3D(x, 0.43F, 0.84F),
                      0.085F,
                      x == -0.56F ? c.tile_red : c.ceramic,
                      BuildingStateMask::Normal,
                      BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(0.44F, 0.30F, 0.72F),
               QVector3D(0.26F, 0.16F, 0.22F),
               c.wood_medium,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.44F, 0.47F, 0.72F),
               QVector3D(0.27F, 0.025F, 0.23F),
               c.wood_dark,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  float const rear_awning_y = (post_h * 0.85F) + 0.04F;
  desc.add_box(QVector3D(0.52F, rear_awning_y, 0.0F),
               QVector3D(0.28F, 0.02F, 0.70F),
               c.cloth_purple,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_cylinder(QVector3D(-0.50F, counter_h + 0.05F, -0.35F),
                    QVector3D(-0.50F, counter_h + 0.18F, -0.35F),
                    0.07F,
                    c.ceramic,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(-0.20F, counter_h + 0.05F, 0.40F),
                    QVector3D(-0.20F, counter_h + 0.16F, 0.40F),
                    0.06F,
                    c.ceramic,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(-0.60F, counter_h + 0.05F, 0.20F),
                    QVector3D(-0.60F, counter_h + 0.20F, 0.20F),
                    0.08F,
                    c.tile_red,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);

  desc.add_cylinder(QVector3D(-1.05F, 0.14F, 0.0F),
                    QVector3D(-1.05F, 0.14F + 0.62F * height_multiplier, 0.0F),
                    0.08F,
                    c.stone_light,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  desc.add_box(QVector3D(-1.05F, 0.14F + 0.62F * height_multiplier + 0.04F, 0.0F),
               QVector3D(0.12F, 0.04F, 0.12F),
               c.sandstone,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(0.62F, 0.24F, -0.60F),
               QVector3D(0.14F, 0.10F, 0.14F),
               c.wood_medium,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.62F, 0.24F, 0.55F),
               QVector3D(0.12F, 0.10F, 0.12F),
               c.wood_dark,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.62F, 0.40F, -0.60F),
               QVector3D(0.12F, 0.08F, 0.12F),
               c.wood_dark,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_palette_box(QVector3D(0.94F, 0.56F * height_multiplier, 0.0F),
                       QVector3D(0.02F, 0.20F, 0.14F),
                       k_marketplace_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  add_punic_tanit_relief(desc,
                         QVector3D(1.01F, 0.55F * height_multiplier, 0.28F),
                         BuildingFacadePlane::ZY,
                         0.68F,
                         c.cloth_gold,
                         c.brick_dark);

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
      MarketplaceRendererConfig{.nation_slug = "carthage",
                                .archetype = &marketplace_archetype,
                                .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.1F},
                                .selection = BuildingSelectionStyle{1.7F, 1.7F}});
}

} // namespace Render::GL::Carthage
