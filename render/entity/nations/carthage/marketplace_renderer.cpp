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
  QVector3D sandstone{0.65F, 0.49F, 0.30F};
  QVector3D sandstone_light{0.78F, 0.63F, 0.42F};
  QVector3D sandstone_dark{0.36F, 0.27F, 0.18F};
  QVector3D mortar{0.43F, 0.35F, 0.25F};
  QVector3D wood_dark{0.13F, 0.072F, 0.032F};
  QVector3D wood_medium{0.31F, 0.17F, 0.068F};
  QVector3D wood_light{0.47F, 0.29F, 0.12F};
  QVector3D cloth_oxblood{0.34F, 0.040F, 0.024F};
  QVector3D cloth_oxblood_faded{0.49F, 0.085F, 0.045F};
  QVector3D cloth_gold{0.66F, 0.38F, 0.085F};
  QVector3D indigo{0.10F, 0.12F, 0.30F};
  QVector3D brick{0.48F, 0.22F, 0.095F};
  QVector3D brick_dark{0.24F, 0.075F, 0.028F};
  QVector3D stone_light{0.66F, 0.52F, 0.34F};
  QVector3D tile_red{0.36F, 0.085F, 0.032F};
  QVector3D ceramic{0.61F, 0.28F, 0.085F};
  QVector3D bronze{0.48F, 0.27F, 0.075F};
  QVector3D rope{0.51F, 0.38F, 0.20F};
  QVector3D saffron{0.76F, 0.36F, 0.035F};
  QVector3D spice_red{0.54F, 0.095F, 0.032F};
  QVector3D herb{0.20F, 0.29F, 0.085F};
  QVector3D ember{0.76F, 0.19F, 0.025F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

void add_punic_amphora(BuildingArchetypeDesc& desc,
                       const QVector3D& base,
                       const QVector3D& clay,
                       const QVector3D& painted_band) {
  QVector3D const shadow = clay * 0.52F;
  desc.add_cone(base + QVector3D(0.0F, 0.04F, 0.0F),
                base,
                0.035F,
                shadow,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.035F, 0.0F),
                    base + QVector3D(0.0F, 0.155F, 0.0F),
                    0.075F,
                    clay,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.145F, 0.0F),
                base + QVector3D(0.0F, 0.215F, 0.0F),
                0.078F,
                clay,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.205F, 0.0F),
                    base + QVector3D(0.0F, 0.285F, 0.0F),
                    0.026F,
                    shadow,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.255F, 0.0F),
                    base + QVector3D(0.0F, 0.278F, 0.0F),
                    0.040F,
                    painted_band,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.025F, 0.225F, 0.0F),
                      base + QVector3D(side * 0.078F, 0.155F, 0.0F),
                      0.011F,
                      shadow,
                      BuildingStateMask::Normal,
                      BuildingLODMask::Full);
  }
}

void add_spice_basket(BuildingArchetypeDesc& desc,
                      const QVector3D& base,
                      const QVector3D& spice,
                      const CarthageMarketPalette& c) {
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.11F, 0.0F),
                    0.11F,
                    c.rope,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.09F, 0.0F),
                    base + QVector3D(0.0F, 0.115F, 0.0F),
                    0.12F,
                    c.wood_dark,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.11F, 0.0F),
                base + QVector3D(0.0F, 0.19F, 0.0F),
                0.095F,
                spice,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
}

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  CarthageMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_marketplace");
  desc.set_full_lod_max_distance(72.0F);

  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.35F, 0.05F, 1.35F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.28F, 0.02F, 1.28F), c.sandstone);
  for (float const offset : {-0.96F, -0.48F, 0.0F, 0.48F, 0.96F}) {
    desc.add_box(QVector3D(offset, 0.143F, 0.0F),
                 QVector3D(0.010F, 0.004F, 1.22F),
                 c.sandstone_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (float const offset : {-0.84F, -0.28F, 0.28F, 0.84F}) {
    desc.add_box(QVector3D(0.0F, 0.144F, offset),
                 QVector3D(1.22F, 0.004F, 0.010F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  float const wall_h = 0.72F * height_multiplier;
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.14F, wall_h * 0.5F, 1.10F),
               c.sandstone);
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.16F, wall_h * 0.35F, 1.14F),
               c.sandstone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.754F, 0.14F + wall_h * 0.52F, 0.0F),
               QVector3D(0.007F, wall_h * 0.48F, 1.08F),
               c.sandstone,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  for (int course = 1; course < 5; ++course) {
    float const y = 0.14F + wall_h * static_cast<float>(course) / 5.0F;
    desc.add_box(QVector3D(0.746F, y, 0.0F),
                 QVector3D(0.006F, 0.010F, 1.07F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (float const z : {-0.72F, -0.24F, 0.24F, 0.72F}) {
    desc.add_box(QVector3D(0.745F, 0.14F + wall_h * 0.50F, z),
                 QVector3D(0.006F, wall_h * 0.46F, 0.009F),
                 c.sandstone_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

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
  for (int plank = 0; plank < 5; ++plank) {
    float const z = -0.64F + static_cast<float>(plank) * 0.32F;
    desc.add_box(QVector3D(-0.30F, counter_h + 0.055F, z),
                 QVector3D(0.58F, 0.009F, 0.148F),
                 (plank % 2 == 0) ? c.wood_light : c.wood_medium,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged,
                 BuildingLODMask::Full);
  }
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
  for (float const z : {-0.78F, 0.78F}) {
    desc.add_cylinder(QVector3D(-0.84F, 0.16F, z),
                      QVector3D(0.23F, counter_h - 0.04F, z),
                      0.018F,
                      c.wood_dark,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }

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
  for (int panel = 0; panel < 8; ++panel) {
    float const z = -0.715F + static_cast<float>(panel) * 0.205F;
    float const sag = (panel == 3 || panel == 4) ? 0.025F : 0.0F;
    desc.add_rotated_box(QVector3D(-0.32F, awning_y - sag, z),
                         QVector3D(0.60F, 0.012F, 0.098F),
                         QVector3D(0.0F, 0.0F, -5.5F),
                         (panel % 2 == 0) ? c.cloth_oxblood
                                          : c.cloth_oxblood_faded,
                         BuildingStateMask::Normal | BuildingStateMask::Damaged);
  }

  desc.add_box(QVector3D(-0.32F, awning_y - 0.03F, 0.80F),
               QVector3D(0.58F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_box(QVector3D(-0.32F, awning_y - 0.03F, -0.80F),
               QVector3D(0.58F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  for (float const z : {-0.80F, 0.80F}) {
    desc.add_cylinder(QVector3D(-0.88F, awning_y - 0.02F, z),
                      QVector3D(-0.82F, 0.16F, z),
                      0.009F,
                      c.rope,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }

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
    for (int panel = 0; panel < 5; ++panel) {
      float const x = -0.94F + static_cast<float>(panel) * 0.28F;
      desc.add_rotated_box(QVector3D(x, 0.70F - (panel == 2 ? 0.018F : 0.0F), z),
                           QVector3D(0.132F, 0.012F, 0.26F),
                           QVector3D(z < 0.0F ? 4.0F : -4.0F, 0.0F, 0.0F),
                           (panel % 2 == 0) ? c.cloth_oxblood
                                            : c.cloth_oxblood_faded,
                           BuildingStateMask::Normal | BuildingStateMask::Damaged);
    }
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

  add_punic_amphora(desc, QVector3D(-0.80F, 0.14F, 0.84F), c.ceramic, c.indigo);
  add_punic_amphora(desc, QVector3D(-0.57F, 0.14F, 0.84F), c.tile_red, c.cloth_gold);
  add_punic_amphora(desc, QVector3D(-0.34F, 0.14F, 0.84F), c.ceramic, c.brick_dark);
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
  for (float const y : {0.20F, 0.30F, 0.40F}) {
    desc.add_box(QVector3D(0.44F, y, 0.945F),
                 QVector3D(0.24F, 0.012F, 0.008F),
                 c.wood_light,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  float const rear_awning_y = (post_h * 0.85F) + 0.04F;
  for (int panel = 0; panel < 6; ++panel) {
    float const z = -0.575F + static_cast<float>(panel) * 0.23F;
    desc.add_rotated_box(QVector3D(0.52F, rear_awning_y, z),
                         QVector3D(0.30F, 0.010F, 0.108F),
                         QVector3D(0.0F, 0.0F, 4.0F),
                         (panel % 2 == 0) ? c.indigo : c.cloth_oxblood,
                         BuildingStateMask::Normal,
                         BuildingLODMask::Full);
  }

  add_spice_basket(
      desc, QVector3D(-0.53F, counter_h + 0.06F, -0.36F), c.saffron, c);
  add_spice_basket(
      desc, QVector3D(-0.19F, counter_h + 0.06F, 0.39F), c.spice_red, c);
  add_spice_basket(desc, QVector3D(-0.58F, counter_h + 0.06F, 0.20F), c.herb, c);
  add_punic_amphora(desc,
                    QVector3D(0.34F, 0.14F, 0.30F),
                    c.ceramic,
                    c.indigo);

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

  desc.add_box(QVector3D(0.62F, 0.24F, 0.55F),
               QVector3D(0.12F, 0.10F, 0.12F),
               c.wood_dark,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  for (float const x_offset : {-0.08F, 0.08F}) {
    desc.add_cylinder(QVector3D(0.62F + x_offset, 0.14F, -0.60F),
                      QVector3D(0.62F + x_offset * 0.45F, 0.36F, -0.60F),
                      0.012F,
                      c.bronze,
                      BuildingStateMask::Normal,
                      BuildingLODMask::Full);
  }
  desc.add_cylinder(QVector3D(0.62F, 0.14F, -0.68F),
                    QVector3D(0.62F, 0.36F, -0.625F),
                    0.012F,
                    c.bronze,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cone(QVector3D(0.62F, 0.43F, -0.60F),
                QVector3D(0.62F, 0.35F, -0.60F),
                0.14F,
                c.bronze,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(0.62F, 0.42F, -0.60F),
                    QVector3D(0.62F, 0.45F, -0.60F),
                    0.105F,
                    c.brick_dark,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cone(QVector3D(0.62F, 0.44F, -0.60F),
                QVector3D(0.62F, 0.68F, -0.60F),
                0.085F,
                c.ember,
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
  add_punic_horned_crown(desc,
                         QVector3D(0.70F, wall_h + 0.38F, -0.82F),
                         0.58F,
                         c.wood_dark,
                         c.cloth_gold,
                         c.ember);

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
