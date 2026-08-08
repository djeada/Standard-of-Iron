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
#include "../../building_decay.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../marketplace_renderer_common.h"
#include "../../registry.h"

namespace Render::GL::Roman {
namespace {

struct RomanMarketPalette {
  QVector3D limestone{0.78F, 0.74F, 0.65F};
  QVector3D limestone_shade{0.60F, 0.56F, 0.49F};
  QVector3D limestone_dark{0.43F, 0.40F, 0.35F};
  QVector3D marble{0.86F, 0.84F, 0.78F};
  QVector3D mortar{0.52F, 0.49F, 0.43F};
  QVector3D cedar{0.40F, 0.25F, 0.13F};
  QVector3D cedar_light{0.56F, 0.37F, 0.20F};
  QVector3D cedar_dark{0.27F, 0.16F, 0.09F};
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D cloth_red_faded{0.68F, 0.16F, 0.10F};
  QVector3D cloth_gold{0.72F, 0.52F, 0.16F};
  QVector3D terracotta{0.63F, 0.25F, 0.105F};
  QVector3D terracotta_dark{0.43F, 0.15F, 0.08F};
  QVector3D blue_accent{0.15F, 0.31F, 0.47F};
  QVector3D bronze{0.48F, 0.30F, 0.11F};
  QVector3D iron{0.12F, 0.13F, 0.13F};
  QVector3D olive{0.25F, 0.31F, 0.10F};
  QVector3D grape{0.24F, 0.08F, 0.18F};
  QVector3D ochre{0.66F, 0.38F, 0.09F};
  QVector3D gold{0.72F, 0.53F, 0.20F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

void add_amphora(BuildingArchetypeDesc& desc,
                 const QVector3D& base,
                 const QVector3D& clay,
                 const QVector3D& painted_band) {
  QVector3D const dark_clay = clay * 0.58F;
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.035F, 0.0F),
                    0.035F,
                    dark_clay,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.035F, 0.0F),
                    base + QVector3D(0.0F, 0.145F, 0.0F),
                    0.070F,
                    clay,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.135F, 0.0F),
                base + QVector3D(0.0F, 0.205F, 0.0F),
                0.072F,
                clay,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.19F, 0.0F),
                    base + QVector3D(0.0F, 0.265F, 0.0F),
                    0.025F,
                    dark_clay,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.245F, 0.0F),
                    base + QVector3D(0.0F, 0.265F, 0.0F),
                    0.037F,
                    painted_band,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.027F, 0.205F, 0.0F),
                      base + QVector3D(side * 0.073F, 0.145F, 0.0F),
                      0.010F,
                      dark_clay,
                      BuildingStateMask::Normal,
                      BuildingLODMask::Full);
  }
}

void add_produce_basket(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        const QVector3D& produce_a,
                        const QVector3D& produce_b,
                        const RomanMarketPalette& c) {
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.10F, 0.0F),
                    0.105F,
                    c.cedar_light,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(base + QVector3D(0.0F, 0.085F, 0.0F),
                    base + QVector3D(0.0F, 0.105F, 0.0F),
                    0.115F,
                    c.cedar_dark,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  int index = 0;
  for (float const x : {-0.055F, 0.0F, 0.055F}) {
    for (float const z : {-0.040F, 0.040F}) {
      QVector3D const fruit_base = base + QVector3D(x, 0.10F, z);
      desc.add_cone(fruit_base,
                    fruit_base + QVector3D(0.0F, 0.055F + 0.008F * index, 0.0F),
                    0.035F,
                    (index++ % 2 == 0) ? produce_a : produce_b,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
    }
  }
}

void add_stall_canopy(BuildingArchetypeDesc& desc,
                      const RomanMarketPalette& c,
                      const QVector3D& center,
                      float half_x,
                      float half_z,
                      float post_h,
                      const QVector3D& cloth,
                      const QVector3D& cloth_shade) {
  constexpr float post_r = 0.016F;
  float const top = center.y() + post_h;

  for (float const sx : {-1.0F, 1.0F}) {
    for (float const sz : {-1.0F, 1.0F}) {
      desc.add_cylinder(
          QVector3D(center.x() + sx * half_x, center.y(), center.z() + sz * half_z),
          QVector3D(center.x() + sx * half_x, top, center.z() + sz * half_z),
          post_r,
          c.cedar_dark,
          k_building_state_mask_intact,
          BuildingLODMask::Full);
    }
  }

  desc.add_cylinder(QVector3D(center.x(), top + 0.055F, center.z() - half_z),
                    QVector3D(center.x(), top + 0.055F, center.z() + half_z),
                    0.010F,
                    c.cedar,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(
        QVector3D(center.x() + side * half_x * 0.52F, top + 0.028F, center.z()),
        QVector3D(half_x * 0.60F, 0.011F, half_z * 1.04F),
        QVector3D(0.0F, 0.0F, side * 15.0F),
        side < 0.0F ? cloth : cloth_shade,
        BuildingStateMask::Normal | BuildingStateMask::Damaged);
  }

  desc.add_box(QVector3D(center.x() - half_x * 0.98F, top + 0.006F, center.z()),
               QVector3D(0.012F, 0.026F, half_z * 1.02F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged,
               BuildingLODMask::Full);
}

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  RomanMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("roman_marketplace");
  desc.set_full_lod_max_distance(72.0F);

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
  for (float const offset : {-1.08F, -0.72F, 0.72F, 1.08F}) {
    desc.add_box(QVector3D(offset, 0.171F, 0.0F),
                 QVector3D(0.010F, 0.004F, 1.18F),
                 c.limestone_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, 0.172F, offset),
                 QVector3D(1.18F, 0.004F, 0.010F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  float const wall_h = 0.46F * height_multiplier;
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.16F, 0.0F),
               QVector3D(0.12F, wall_h * 0.5F, 1.10F),
               c.limestone);

  desc.add_box(QVector3D(0.92F, wall_h + 0.18F, 0.0F),
               QVector3D(0.14F, 0.04F, 1.14F),
               c.limestone_shade,
               k_building_state_mask_intact);
  for (int course = 1; course < 4; ++course) {
    float const course_y = 0.16F + wall_h * static_cast<float>(course) / 4.0F;
    desc.add_box(QVector3D(0.795F, course_y, 0.0F),
                 QVector3D(0.006F, 0.010F, 1.07F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (float const joint_z : {-0.72F, -0.24F, 0.24F, 0.72F}) {
    desc.add_box(QVector3D(0.794F, 0.16F + wall_h * 0.50F, joint_z),
                 QVector3D(0.006F, wall_h * 0.48F, 0.008F),
                 c.limestone_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

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
  for (int plank = 0; plank < 5; ++plank) {
    float const z = -0.68F + static_cast<float>(plank) * 0.34F;
    desc.add_box(QVector3D(-0.20F, counter_h + 0.055F, z),
                 QVector3D(0.60F, 0.009F, 0.158F),
                 (plank % 2 == 0) ? c.cedar_light : c.cedar,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged,
                 BuildingLODMask::Full);
  }

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
  for (float const z : {-0.78F, 0.78F}) {
    desc.add_cylinder(QVector3D(-0.76F, 0.18F, z),
                      QVector3D(0.36F, counter_h - 0.04F, z),
                      0.018F,
                      c.cedar_dark,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }

  float const awning_y = col_height + 0.20F;
  for (float const z : {-0.56F, 0.0F, 0.56F}) {
    add_stall_canopy(desc,
                     c,
                     QVector3D(-0.20F, counter_h + 0.06F, z),
                     0.27F,
                     0.18F,
                     0.30F * height_multiplier,
                     c.cloth_gold,
                     c.ochre);
  }

  for (float const z : {-0.86F, 0.86F}) {
    desc.add_cylinder(QVector3D(-0.83F, awning_y - 0.02F, z),
                      QVector3D(-0.76F, 0.20F, z),
                      0.009F,
                      c.cloth_gold,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }

  for (float const z : {-0.74F, 0.0F, 0.74F}) {
    desc.add_box(
        QVector3D(0.46F, 0.16F + 0.19F, z), QVector3D(0.26F, 0.035F, 0.28F), c.cedar);
    for (float const sz : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.46F, 0.16F + 0.095F, z + sz * 0.24F),
                   QVector3D(0.030F, 0.095F, 0.030F),
                   c.cedar_dark,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
    add_stall_canopy(desc,
                     c,
                     QVector3D(0.46F, 0.16F, z),
                     0.24F,
                     0.21F,
                     0.58F * height_multiplier,
                     c.cloth_red,
                     c.cloth_red_faded);
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
    add_produce_basket(desc,
                       QVector3D(-0.50F, 0.395F, z),
                       z < 0.0F ? c.ochre : c.olive,
                       z < 0.0F ? c.terracotta : c.grape,
                       c);
    add_produce_basket(desc, QVector3D(-0.20F, 0.395F, z), c.olive, c.ochre, c);
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
  for (float const y : {0.22F, 0.28F, 0.34F}) {
    desc.add_box(QVector3D(0.50F, y, -0.845F),
                 QVector3D(0.13F, 0.012F, 0.008F),
                 c.cedar_light,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(0.50F, 0.28F, 0.68F),
               QVector3D(0.12F, 0.10F, 0.12F),
               c.cedar,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  add_amphora(desc,
              QVector3D(0.02F, counter_h + 0.06F, -0.46F),
              c.terracotta,
              c.terracotta_dark);
  add_amphora(desc, QVector3D(0.42F, 0.18F, 0.37F), c.terracotta_dark, c.cloth_gold);

  desc.add_cylinder(QVector3D(0.20F, counter_h + 0.06F, 0.28F),
                    QVector3D(0.20F, counter_h + 0.32F, 0.28F),
                    0.012F,
                    c.bronze,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(0.06F, counter_h + 0.29F, 0.28F),
                    QVector3D(0.34F, counter_h + 0.29F, 0.28F),
                    0.010F,
                    c.bronze,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
  for (float const x : {0.07F, 0.33F}) {
    desc.add_cylinder(QVector3D(x, counter_h + 0.29F, 0.28F),
                      QVector3D(x, counter_h + 0.19F, 0.28F),
                      0.004F,
                      c.iron,
                      BuildingStateMask::Normal,
                      BuildingLODMask::Full);
    desc.add_cone(QVector3D(x, counter_h + 0.20F, 0.28F),
                  QVector3D(x, counter_h + 0.17F, 0.28F),
                  0.060F,
                  c.bronze,
                  BuildingStateMask::Normal,
                  BuildingLODMask::Full);
  }

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

  add_roman_aquila_relief(desc,
                          QVector3D(0.99F, 0.62F * height_multiplier, 0.0F),
                          BuildingFacadePlane::ZY,
                          0.72F,
                          c.gold,
                          c.terracotta_dark);

  desc.add_cylinder(QVector3D(0.92F, wall_h + 0.20F, -1.02F),
                    QVector3D(0.92F, wall_h + 0.86F, -1.02F),
                    0.020F,
                    c.cedar_dark,
                    k_building_state_mask_intact);
  add_roman_roof_standard(
      desc, QVector3D(0.92F, wall_h + 0.86F, -1.02F), 0.58F, c.gold, c.cloth_red);

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.16F, 0.0F, 1.16F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.cedar_dark,
                                 .ground_y = 0.16F,
                                 .scale = 1.0F,
                                 .seed = 181});

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
