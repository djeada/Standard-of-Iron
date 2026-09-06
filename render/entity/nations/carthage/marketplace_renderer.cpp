#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstdint>

#include "building_palette.h"
#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/marketplace_renderer_common.h"
#include "render/entity/registry.h"
#include "render/geom/transforms.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/render_archetype.h"
#include "render/submitter.h"
#include "render/template_cache.h"

namespace Render::GL::Carthage {
namespace {

struct CarthageMarketPalette {
  QVector3D sandstone = BuildingPalette::k_sandstone;
  QVector3D sandstone_light = BuildingPalette::k_sandstone_light;
  QVector3D sandstone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D mortar{0.63F, 0.55F, 0.42F};
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D wood_medium = BuildingPalette::k_wood;
  QVector3D wood_light = BuildingPalette::k_wood_light;
  QVector3D cloth_oxblood = BuildingPalette::k_oxblood;
  QVector3D cloth_oxblood_faded{0.64F, 0.17F, 0.13F};
  QVector3D cloth_gold{0.66F, 0.38F, 0.085F};
  QVector3D indigo = BuildingPalette::k_indigo;
  QVector3D brick = BuildingPalette::k_brick;
  QVector3D brick_dark = BuildingPalette::k_brick_dark;
  QVector3D stone_light = BuildingPalette::k_sandstone_light;
  QVector3D tile_red = BuildingPalette::k_tile_red;
  QVector3D ceramic{0.61F, 0.28F, 0.085F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D rope{0.51F, 0.38F, 0.20F};
  QVector3D saffron = BuildingPalette::k_saffron;
  QVector3D spice_red{0.54F, 0.095F, 0.032F};
  QVector3D herb{0.20F, 0.29F, 0.085F};
  QVector3D ember{0.76F, 0.19F, 0.025F};
};

constexpr std::uint8_t k_marketplace_team_slot = 0;

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

void add_punic_amphora(BuildingArchetypeDesc& desc,
                       const QVector3D& base,
                       const QVector3D& clay,
                       const QVector3D& painted_band) {
  QVector3D const shadow = clay * 0.52F;
  desc.add_cone(base + QVector3D(0.0F, 0.04F, 0.0F),
                base,
                0.035F,
                shadow,
                BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.035F, 0.0F),
                    base + QVector3D(0.0F, 0.155F, 0.0F),
                    0.075F,
                    clay,
                    BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, 0.145F, 0.0F),
                base + QVector3D(0.0F, 0.215F, 0.0F),
                0.078F,
                clay,
                BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.205F, 0.0F),
                    base + QVector3D(0.0F, 0.285F, 0.0F),
                    0.026F,
                    shadow,
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.255F, 0.0F),
                    base + QVector3D(0.0F, 0.278F, 0.0F),
                    0.040F,
                    painted_band,
                    BuildingStateMask::Normal);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.025F, 0.225F, 0.0F),
                      base + QVector3D(side * 0.078F, 0.155F, 0.0F),
                      0.011F,
                      shadow,
                      BuildingStateMask::Normal);
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
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.09F, 0.0F),
                    base + QVector3D(0.0F, 0.115F, 0.0F),
                    0.12F,
                    c.wood_dark,
                    BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, 0.11F, 0.0F),
                base + QVector3D(0.0F, 0.19F, 0.0F),
                0.095F,
                spice,
                BuildingStateMask::Normal);
}

void add_stall_canopy(BuildingArchetypeDesc& desc,
                      const CarthageMarketPalette& c,
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
          c.wood_dark,
          k_building_state_mask_intact);
    }
  }

  desc.add_cylinder(QVector3D(center.x(), top + 0.055F, center.z() - half_z),
                    QVector3D(center.x(), top + 0.055F, center.z() + half_z),
                    0.010F,
                    c.wood_medium,
                    k_building_state_mask_intact);
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
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
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

  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.35F, 0.05F, 1.35F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.28F, 0.02F, 1.28F), c.sandstone);
  for (float const offset : {-0.96F, -0.48F, 0.0F, 0.48F, 0.96F}) {
    desc.add_box(QVector3D(offset, 0.143F, 0.0F),
                 QVector3D(0.010F, 0.004F, 1.22F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
  }
  for (float const offset : {-0.84F, -0.28F, 0.28F, 0.84F}) {
    desc.add_box(QVector3D(0.0F, 0.144F, offset),
                 QVector3D(1.22F, 0.004F, 0.010F),
                 c.mortar,
                 k_building_state_mask_intact);
  }

  float const wall_h = 0.44F * height_multiplier;
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.14F, wall_h * 0.5F, 1.10F),
               c.sandstone);
  desc.add_box(QVector3D(0.92F, wall_h * 0.5F + 0.14F, 0.0F),
               QVector3D(0.16F, wall_h * 0.35F, 1.14F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.754F, 0.14F + wall_h * 0.52F, 0.0F),
               QVector3D(0.007F, wall_h * 0.48F, 1.08F),
               c.sandstone,
               k_building_state_mask_intact);
  for (int course = 1; course < 5; ++course) {
    float const y = 0.14F + wall_h * static_cast<float>(course) / 5.0F;
    desc.add_box(QVector3D(0.746F, y, 0.0F),
                 QVector3D(0.006F, 0.010F, 1.07F),
                 c.mortar,
                 k_building_state_mask_intact);
  }
  for (float const z : {-0.72F, -0.24F, 0.24F, 0.72F}) {
    desc.add_box(QVector3D(0.745F, 0.14F + wall_h * 0.50F, z),
                 QVector3D(0.006F, wall_h * 0.46F, 0.009F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(0.92F, wall_h + 0.17F, 0.0F),
               QVector3D(0.17F, 0.035F, 1.14F),
               c.sandstone_dark,
               k_building_state_mask_intact);

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
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
  }
  desc.add_box(QVector3D(-0.30F, counter_h * 0.5F, 0.0F),
               QVector3D(0.58F, counter_h * 0.5F - 0.05F, 0.04F),
               c.wood_dark,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(-0.30F, counter_h - 0.08F, -0.78F),
               QVector3D(0.58F, 0.08F, 0.03F),
               c.wood_dark,
               k_building_state_mask_intact);
  for (float const z : {-0.78F, 0.78F}) {
    desc.add_cylinder(QVector3D(-0.84F, 0.16F, z),
                      QVector3D(0.23F, counter_h - 0.04F, z),
                      0.018F,
                      c.wood_dark,
                      k_building_state_mask_intact);
  }

  float const post_h = 0.88F * height_multiplier;
  float const awning_y = post_h + 0.04F;
  for (float const z : {-0.56F, 0.0F, 0.56F}) {
    add_stall_canopy(desc,
                     c,
                     QVector3D(-0.32F, counter_h + 0.06F, z),
                     0.27F,
                     0.18F,
                     0.30F * height_multiplier,
                     c.cloth_oxblood,
                     c.cloth_oxblood_faded);
  }

  for (float const z : {-0.80F, 0.80F}) {
    desc.add_cylinder(QVector3D(-0.88F, awning_y - 0.02F, z),
                      QVector3D(-0.82F, 0.16F, z),
                      0.009F,
                      c.rope,
                      k_building_state_mask_intact);
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
               k_building_state_mask_intact);
  for (float const z : {-0.94F, -0.70F}) {
    desc.add_box(QVector3D(0.34F, 0.52F, z),
                 QVector3D(0.025F, 0.30F, 0.15F),
                 c.wood_dark,
                 k_building_state_mask_intact);
  }

  for (float const z : {-0.48F, 0.48F}) {
    for (int panel = 0; panel < 5; ++panel) {
      float const x = -0.94F + static_cast<float>(panel) * 0.28F;
      desc.add_rotated_box(QVector3D(x, 0.70F - (panel == 2 ? 0.018F : 0.0F), z),
                           QVector3D(0.132F, 0.012F, 0.26F),
                           QVector3D(z < 0.0F ? 4.0F : -4.0F, 0.0F, 0.0F),
                           (panel % 2 == 0) ? c.cloth_oxblood : c.cloth_oxblood_faded,
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
               BuildingStateMask::Normal);
  desc.add_box(QVector3D(0.44F, 0.47F, 0.72F),
               QVector3D(0.27F, 0.025F, 0.23F),
               c.wood_dark,
               BuildingStateMask::Normal);
  for (float const y : {0.20F, 0.30F, 0.40F}) {
    desc.add_box(QVector3D(0.44F, y, 0.945F),
                 QVector3D(0.24F, 0.012F, 0.008F),
                 c.wood_light,
                 BuildingStateMask::Normal);
  }

  for (float const z : {-0.62F, 0.0F, 0.62F}) {
    desc.add_box(QVector3D(0.46F, 0.14F + 0.19F, z),
                 QVector3D(0.24F, 0.035F, 0.26F),
                 c.wood_medium);
    for (float const sz : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.46F, 0.14F + 0.095F, z + sz * 0.22F),
                   QVector3D(0.030F, 0.095F, 0.030F),
                   c.wood_dark,
                   k_building_state_mask_intact);
    }
    add_stall_canopy(desc,
                     c,
                     QVector3D(0.46F, 0.14F, z),
                     0.24F,
                     0.20F,
                     0.56F * height_multiplier,
                     c.indigo,
                     c.cloth_oxblood);
  }

  add_spice_basket(desc, QVector3D(-0.53F, counter_h + 0.06F, -0.36F), c.saffron, c);
  add_spice_basket(desc, QVector3D(-0.19F, counter_h + 0.06F, 0.39F), c.spice_red, c);
  add_spice_basket(desc, QVector3D(-0.58F, counter_h + 0.06F, 0.20F), c.herb, c);
  add_punic_amphora(desc, QVector3D(0.34F, 0.14F, 0.30F), c.ceramic, c.indigo);

  desc.add_cylinder(QVector3D(-1.05F, 0.14F, 0.0F),
                    QVector3D(-1.05F, 0.14F + 0.62F * height_multiplier, 0.0F),
                    0.08F,
                    c.stone_light,
                    k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.05F, 0.14F + 0.62F * height_multiplier + 0.04F, 0.0F),
               QVector3D(0.12F, 0.04F, 0.12F),
               c.sandstone,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.62F, 0.24F, 0.55F),
               QVector3D(0.12F, 0.10F, 0.12F),
               c.wood_dark,
               BuildingStateMask::Normal);
  for (float const x_offset : {-0.08F, 0.08F}) {
    desc.add_cylinder(QVector3D(0.62F + x_offset, 0.14F, -0.60F),
                      QVector3D(0.62F + x_offset * 0.45F, 0.36F, -0.60F),
                      0.012F,
                      c.bronze,
                      BuildingStateMask::Normal);
  }
  desc.add_cylinder(QVector3D(0.62F, 0.14F, -0.68F),
                    QVector3D(0.62F, 0.36F, -0.625F),
                    0.012F,
                    c.bronze,
                    BuildingStateMask::Normal);
  desc.add_cone(QVector3D(0.62F, 0.43F, -0.60F),
                QVector3D(0.62F, 0.35F, -0.60F),
                0.14F,
                c.bronze,
                BuildingStateMask::Normal);
  desc.add_cylinder(QVector3D(0.62F, 0.42F, -0.60F),
                    QVector3D(0.62F, 0.45F, -0.60F),
                    0.105F,
                    c.brick_dark,
                    BuildingStateMask::Normal);
  desc.add_cone(QVector3D(0.62F, 0.44F, -0.60F),
                QVector3D(0.62F, 0.68F, -0.60F),
                0.085F,
                c.ember,
                BuildingStateMask::Normal);

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

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.12F, 0.0F, 1.12F),
                                 .stone = c.sandstone,
                                 .stone_dark = c.sandstone_dark,
                                 .timber = c.sandstone_dark * 0.5F,
                                 .ground_y = 0.14F,
                                 .scale = 1.0F,
                                 .seed = 223});

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
                                .palette_slots = &marketplace_palette_slots,
                                .selection = BuildingSelectionStyle{1.7F, 1.7F}});
}

} // namespace Render::GL::Carthage
