#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
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

void add_spice_stall(BuildingArchetypeDesc& desc,
                     float cx,
                     float cz,
                     float side,
                     const QVector3D& spice_a,
                     const QVector3D& spice_b,
                     const CarthageMarketPalette& c) {
  BuildingPartMaterial wood(desc, k_building_material_wood);
  desc.add_box(QVector3D(cx, 0.332F, cz),
               QVector3D(0.25F, 0.012F, 0.13F),
               c.wood_medium,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  for (float const lx : {-0.21F, 0.21F}) {
    for (float const lz : {-0.10F, 0.10F}) {
      desc.add_cylinder(QVector3D(cx + lx, 0.14F, cz + lz),
                        QVector3D(cx + lx, 0.322F, cz + lz),
                        0.014F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
  }
  for (float const lx : {-0.26F, 0.26F}) {
    desc.add_cylinder(QVector3D(cx + lx, 0.14F, cz + side * 0.22F),
                      QVector3D(cx + lx, 0.85F, cz + side * 0.22F),
                      0.016F,
                      c.wood_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(cx + lx, 0.14F, cz - side * 0.22F),
                      QVector3D(cx + lx, 0.73F, cz - side * 0.22F),
                      0.016F,
                      c.wood_dark,
                      k_building_state_mask_intact);
  }
  add_spice_basket(desc, QVector3D(cx - 0.13F, 0.344F, cz), spice_a, c);
  add_spice_basket(desc, QVector3D(cx + 0.12F, 0.344F, cz), spice_b, c);
  BuildingPartMaterial clay(desc, k_building_material_stone);
  desc.add_cylinder(QVector3D(cx, 0.14F, cz - side * 0.02F),
                    QVector3D(cx, 0.20F, cz - side * 0.02F),
                    0.05F,
                    c.ceramic,
                    BuildingStateMask::Normal);
}

auto build_marketplace_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  CarthageMarketPalette const c;
  float hm = 1.0F;
  if (state == BuildingState::Damaged) {
    hm = 0.75F;
  } else if (state == BuildingState::Destroyed) {
    hm = 0.35F;
  }
  QVector3D const plaster{0.86F, 0.81F, 0.70F};
  QVector3D const murex{0.36F, 0.08F, 0.26F};

  BuildingArchetypeDesc desc("carthage_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.35F, 0.05F, 1.35F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.28F, 0.02F, 1.28F), c.sandstone);
  for (float g = -0.96F; g <= 0.97F; g += 0.32F) {
    desc.add_box(QVector3D(g, 0.1415F, -0.02F),
                 QVector3D(0.005F, 0.0015F, 1.22F),
                 c.mortar,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.31F, 0.1415F, g),
                 QVector3D(0.91F, 0.0015F, 0.005F),
                 c.mortar,
                 k_building_state_mask_intact);
  }

  float const wall_h = 0.60F * hm;
  float const wall_top = 0.14F + wall_h;
  desc.add_box(QVector3D(1.16F, 0.14F + wall_h * 0.5F, 0.0F),
               QVector3D(0.07F, wall_h * 0.5F, 1.24F),
               c.brick);
  for (float const z : {-1.19F, -0.40F, 0.40F, 1.19F}) {
    desc.add_box(QVector3D(0.85F, 0.14F + wall_h * 0.5F - 0.004F, z),
                 QVector3D(0.27F, wall_h * 0.5F - 0.004F, 0.045F),
                 c.sandstone);
    desc.add_box(QVector3D(0.585F, 0.14F + wall_h * 0.5F, z),
                 QVector3D(0.012F, wall_h * 0.5F, 0.056F),
                 plaster,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(0.58F, wall_top + 0.035F, 0.0F),
               QVector3D(0.05F, 0.04F, 1.24F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.89F, wall_top + 0.085F, 0.0F),
               QVector3D(0.35F, 0.012F, 1.25F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  for (float const z : {-1.22F, 1.22F}) {
    desc.add_box(QVector3D(0.89F, wall_top + 0.13F, z),
                 QVector3D(0.35F, 0.035F, 0.025F),
                 c.sandstone,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(1.22F, wall_top + 0.132F, 0.0F),
               QVector3D(0.025F, 0.037F, 1.24F),
               c.sandstone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.545F, wall_top + 0.01F, 0.0F),
               QVector3D(0.006F, 0.012F, 1.22F),
               c.cloth_oxblood,
               BuildingStateMask::Normal);
  for (float const zc : {-0.80F, 0.0F, 0.80F}) {
    desc.add_box(QVector3D(1.087F, 0.14F + wall_h * 0.5F, zc),
                 QVector3D(0.004F, wall_h * 0.5F - 0.01F, 0.34F),
                 c.brick_dark * 0.70F,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.24F, zc),
                 QVector3D(0.07F, 0.10F, 0.30F),
                 c.brick,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.346F, zc),
                 QVector3D(0.09F, 0.008F, 0.32F),
                 plaster,
                 k_building_state_mask_intact);
  }
  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    for (float const z : {-1.02F, -0.84F, -0.66F}) {
      add_punic_amphora(desc, QVector3D(0.96F, 0.14F, z), c.ceramic, c.spice_red);
    }
    add_punic_amphora(desc, QVector3D(0.67F, 0.354F, -0.94F), c.ceramic, c.saffron);
  }
  add_spice_basket(desc, QVector3D(0.67F, 0.354F, -0.16F), c.saffron, c);
  add_spice_basket(desc, QVector3D(0.67F, 0.354F, 0.16F), c.spice_red, c);
  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    int bolt = 0;
    for (float const z : {0.62F, 0.78F, 0.94F}) {
      for (float const y : {0.38F, 0.435F}) {
        QVector3D const colour = (bolt % 3 == 0)   ? murex
                                 : (bolt % 3 == 1) ? c.indigo
                                                   : c.cloth_gold;
        desc.add_cylinder(QVector3D(0.61F, y, z),
                          QVector3D(0.73F, y, z),
                          0.028F,
                          colour,
                          BuildingStateMask::Normal);
        ++bolt;
      }
    }
  }

  add_spice_stall(desc, -0.40F, 0.82F, 1.0F, c.saffron, c.herb, c);
  add_spice_stall(desc, -0.40F, -0.82F, -1.0F, c.spice_red, c.saffron, c);

  desc.add_cylinder(QVector3D(-0.30F, 0.14F, 0.0F),
                    QVector3D(-0.30F, 0.14F + 0.14F * std::max(hm, 0.6F), 0.0F),
                    0.19F,
                    c.sandstone_light);
  desc.add_cylinder(QVector3D(-0.30F, 0.14F, 0.0F),
                    QVector3D(-0.30F, 0.275F, 0.0F),
                    0.155F,
                    QVector3D(0.16F, 0.26F, 0.30F),
                    k_building_state_mask_intact);
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float const z : {-0.17F, 0.17F}) {
      desc.add_cylinder(QVector3D(-0.30F, 0.28F, z),
                        QVector3D(-0.30F, 0.56F, z),
                        0.014F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(-0.30F, 0.55F, -0.19F),
                      QVector3D(-0.30F, 0.55F, 0.19F),
                      0.016F,
                      c.wood_medium,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(-0.30F, 0.55F, 0.0F),
                      QVector3D(-0.30F, 0.42F, 0.0F),
                      0.004F,
                      c.rope,
                      BuildingStateMask::Normal);
    desc.add_cylinder(QVector3D(-0.30F, 0.37F, 0.0F),
                      QVector3D(-0.30F, 0.42F, 0.0F),
                      0.035F,
                      c.wood_dark,
                      BuildingStateMask::Normal);
  }

  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    for (float const x : {-1.00F, -0.72F}) {
      desc.add_cylinder(QVector3D(x, 0.14F, -1.02F),
                        QVector3D(x, 0.29F, -1.02F),
                        0.12F,
                        c.sandstone_dark,
                        k_building_state_mask_intact);
      desc.add_cylinder(QVector3D(x, 0.14F, -1.02F),
                        QVector3D(x, 0.285F, -1.02F),
                        0.095F,
                        murex,
                        BuildingStateMask::Normal);
    }
  }
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float const x : {-1.12F, -0.60F}) {
      desc.add_cylinder(QVector3D(x, 0.14F, -0.78F),
                        QVector3D(x, 0.80F, -0.78F),
                        0.014F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(-1.13F, 0.79F, -0.78F),
                      QVector3D(-0.59F, 0.79F, -0.78F),
                      0.005F,
                      c.rope,
                      k_building_state_mask_intact);
  }

  float const pylon_h = 0.80F * hm;
  for (float const z : {-0.50F, 0.50F}) {
    desc.add_box(QVector3D(-1.14F, 0.14F + pylon_h * 0.5F, z),
                 QVector3D(0.07F, pylon_h * 0.5F, 0.07F),
                 c.sandstone);
    desc.add_box(QVector3D(-1.14F, 0.14F + pylon_h + 0.02F, z),
                 QVector3D(0.085F, 0.02F, 0.085F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }
  float const beam_y = 0.14F + pylon_h + 0.07F;
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_box(QVector3D(-1.14F, beam_y, 0.0F),
                 QVector3D(0.05F, 0.035F, 0.62F),
                 c.wood_dark,
                 k_building_state_mask_intact);
  }
  add_punic_tanit_relief(desc,
                         QVector3D(-1.15F, beam_y + 0.17F, 0.0F),
                         BuildingFacadePlane::ZY,
                         0.28F,
                         c.cloth_gold,
                         c.brick_dark);
  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    for (float const z : {-0.50F, 0.50F}) {
      desc.add_palette_box(QVector3D(-1.215F, 0.14F + pylon_h * 0.62F, z),
                           QVector3D(0.004F, 0.12F, 0.05F),
                           k_marketplace_team_slot,
                           BuildingStateMask::Normal);
    }
  }
  add_punic_horned_crown(desc,
                         QVector3D(0.89F, wall_top + 0.10F, 0.0F),
                         0.50F,
                         c.wood_dark,
                         c.cloth_gold,
                         c.ember);

  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    add_punic_amphora(desc, QVector3D(-0.98F, 0.14F, 1.04F), c.ceramic, c.cloth_gold);
    add_punic_amphora(
        desc, QVector3D(-0.83F, 0.14F, 1.09F), c.ceramic * 0.85F, c.spice_red);
  }
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_box(QVector3D(-0.98F, 0.21F, 0.84F),
                 QVector3D(0.07F, 0.07F, 0.07F),
                 c.wood_medium,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.12F, 0.0F, 1.12F),
                                 .stone = c.sandstone,
                                 .stone_dark = c.sandstone_dark,
                                 .timber = c.sandstone_dark * 0.5F,
                                 .ground_y = 0.14F,
                                 .scale = 1.0F,
                                 .seed = 223});

  return desc;
}

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  return build_building_archetype(build_marketplace_desc_impl(state), state);
}

auto marketplace_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_marketplace_archetype);
  return k_set.for_state(state);
}

const std::array<TorchMount, 4> k_torches{{
    TorchMount{.at = QVector3D(-1.22F, 0.64F, 0.50F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-1.22F, 0.64F, -0.50F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.58F, 0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.58F, -0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
}};

const std::array<MarketAwning, 7> k_awnings{{
    MarketAwning{.back = QVector3D(-0.40F, 0.855F, 1.05F),
                 .front = QVector3D(-0.40F, 0.735F, 0.59F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.80F, 0.56F, 0.16F),
                 .stripes = 5},
    MarketAwning{.back = QVector3D(-0.40F, 0.855F, -1.05F),
                 .front = QVector3D(-0.40F, 0.735F, -0.59F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.52F, 0.10F, 0.08F),
                 .stripe_b = QVector3D(0.86F, 0.78F, 0.62F),
                 .stripes = 5},
    MarketAwning{.back = QVector3D(-1.02F, 0.785F, -0.78F),
                 .front = QVector3D(-1.01F, 0.52F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.09F,
                 .stripe_a = QVector3D(0.38F, 0.08F, 0.28F),
                 .stripe_b = QVector3D(0.44F, 0.10F, 0.32F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(-0.84F, 0.785F, -0.78F),
                 .front = QVector3D(-0.83F, 0.56F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.08F,
                 .stripe_a = QVector3D(0.30F, 0.06F, 0.24F),
                 .stripe_b = QVector3D(0.36F, 0.08F, 0.28F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(-0.69F, 0.785F, -0.78F),
                 .front = QVector3D(-0.68F, 0.54F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.06F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.20F, 0.24F, 0.52F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(0.535F, 0.735F, -0.80F),
                 .front = QVector3D(0.52F, 0.63F, -0.80F),
                 .width_axis = QVector3D(0.0F, 0.0F, 1.0F),
                 .half_width = 0.32F,
                 .stripe_a = QVector3D(0.52F, 0.10F, 0.08F),
                 .stripe_b = QVector3D(0.60F, 0.14F, 0.10F),
                 .stripes = 4},
    MarketAwning{.back = QVector3D(0.535F, 0.735F, 0.80F),
                 .front = QVector3D(0.52F, 0.63F, 0.80F),
                 .width_axis = QVector3D(0.0F, 0.0F, 1.0F),
                 .half_width = 0.32F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.80F, 0.56F, 0.16F),
                 .stripes = 4},
}};

const std::array<MarketHanging, 4> k_hangings{{
    MarketHanging{.pivot = QVector3D(-0.40F, 0.78F, 1.00F),
                  .length = 0.11F,
                  .radius = 0.022F,
                  .color = QVector3D(0.80F, 0.56F, 0.16F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.78F, -1.00F),
                  .length = 0.11F,
                  .radius = 0.024F,
                  .color = QVector3D(0.54F, 0.10F, 0.04F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.73F, -0.20F),
                  .length = 0.13F,
                  .radius = 0.026F,
                  .color = QVector3D(0.20F, 0.29F, 0.09F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(0.55F, 0.73F, 0.20F),
                  .length = 0.13F,
                  .radius = 0.022F,
                  .color = QVector3D(0.66F, 0.38F, 0.09F),
                  .beads = 3},
}};

const std::array<QVector3D, 5> k_buyer_a{QVector3D(-1.80F, 0.0F, -0.20F),
                                         QVector3D(-1.40F, 0.0F, -0.18F),
                                         QVector3D(-1.30F, 0.14F, -0.18F),
                                         QVector3D(-0.78F, 0.14F, -0.40F),
                                         QVector3D(-0.40F, 0.14F, -0.46F)};
const std::array<QVector3D, 5> k_buyer_b{QVector3D(-1.80F, 0.0F, 0.22F),
                                         QVector3D(-1.40F, 0.0F, 0.20F),
                                         QVector3D(-1.30F, 0.14F, 0.22F),
                                         QVector3D(0.06F, 0.14F, 0.34F),
                                         QVector3D(0.42F, 0.14F, 0.10F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(-0.90F, 0.14F, 0.36F),
                                              QVector3D(0.05F, 0.14F, 0.32F),
                                              QVector3D(0.08F, 0.14F, -0.34F),
                                              QVector3D(-0.44F, 0.14F, -0.36F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(-1.80F, 0.0F, 0.38F),
                                              QVector3D(-1.40F, 0.0F, 0.38F),
                                              QVector3D(-1.30F, 0.14F, 0.38F),
                                              QVector3D(0.05F, 0.14F, 0.40F),
                                              QVector3D(0.46F, 0.14F, 0.64F)};
const std::array<QVector3D, 1> k_seller_a{QVector3D(-0.40F, 0.14F, 1.05F)};
const std::array<QVector3D, 1> k_seller_b{QVector3D(-0.40F, 0.14F, -1.05F)};
const std::array<QVector3D, 1> k_shopkeeper{QVector3D(0.86F, 0.14F, 0.02F)};
const std::array<QVector3D, 1> k_dyer{QVector3D(-0.86F, 0.14F, -0.90F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{
        .min_x = -1.28F, .max_x = 1.28F, .min_z = -1.28F, .max_z = 1.28F, .top = 0.14F},
}};
const std::array<AmbientPerson, 8> k_people{{
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_a,
                  .facing = QVector3D(-0.40F, 0.4F, -0.82F),
                  .linger = AmbientRole::Haggle},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_b,
                  .facing = QVector3D(0.67F, 0.4F, 0.0F),
                  .linger = AmbientRole::Haggle},
    AmbientPerson{.role = AmbientRole::Haggle,
                  .route = k_seller_a,
                  .facing = QVector3D(-0.40F, 0.3F, 0.40F)},
    AmbientPerson{.role = AmbientRole::Weave,
                  .route = k_seller_b,
                  .facing = QVector3D(-0.40F, 0.3F, -0.40F)},
    AmbientPerson{.role = AmbientRole::Haggle,
                  .route = k_shopkeeper,
                  .facing = QVector3D(0.30F, 0.4F, 0.0F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_stroll_route,
                  .facing = QVector3D(-0.30F, 0.3F, 0.0F),
                  .linger = AmbientRole::Weave},
    AmbientPerson{.role = AmbientRole::Porter,
                  .route = k_porter_route,
                  .facing = QVector3D(0.67F, 0.35F, 0.80F)},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_dyer,
                  .facing = QVector3D(-0.86F, 0.2F, -1.02F)},
}};

} // namespace

auto build_marketplace_desc(BuildingState state) -> BuildingArchetypeDesc {
  return build_marketplace_desc_impl(state);
}

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_marketplace_renderer_variant(
      registry,
      MarketplaceRendererConfig{.nation_slug = "carthage",
                                .archetype = &marketplace_archetype,
                                .palette_slots = &marketplace_palette_slots,
                                .selection = BuildingSelectionStyle{1.7F, 1.7F},
                                .torches = k_torches,
                                .people = k_people,
                                .walk_surfaces = k_walk_surfaces,
                                .awnings = k_awnings,
                                .hangings = k_hangings});
}

} // namespace Render::GL::Carthage
