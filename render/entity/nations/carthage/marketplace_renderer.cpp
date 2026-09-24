#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
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
  QVector3D plaster{0.86F, 0.81F, 0.70F};
  QVector3D floor_earth{0.66F, 0.52F, 0.36F};
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D wood_medium = BuildingPalette::k_wood;
  QVector3D wood_light = BuildingPalette::k_wood_light;
  QVector3D cloth_oxblood = BuildingPalette::k_oxblood;
  QVector3D cloth_oxblood_faded{0.64F, 0.17F, 0.13F};
  QVector3D cloth_gold{0.66F, 0.38F, 0.085F};
  QVector3D murex{0.36F, 0.08F, 0.26F};
  QVector3D murex_deep{0.22F, 0.04F, 0.17F};
  QVector3D wool{0.86F, 0.82F, 0.72F};
  QVector3D indigo = BuildingPalette::k_indigo;
  QVector3D brick = BuildingPalette::k_brick;
  QVector3D brick_dark = BuildingPalette::k_brick_dark;
  QVector3D stone_light = BuildingPalette::k_sandstone_light;
  QVector3D tile_red = BuildingPalette::k_tile_red;
  QVector3D ceramic{0.61F, 0.28F, 0.085F};
  QVector3D ceramic_pale{0.76F, 0.56F, 0.36F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D rope{0.51F, 0.38F, 0.20F};
  QVector3D saffron = BuildingPalette::k_saffron;
  QVector3D spice_red{0.54F, 0.095F, 0.032F};
  QVector3D cumin{0.55F, 0.36F, 0.16F};
  QVector3D pepper{0.19F, 0.15F, 0.12F};
  QVector3D herb{0.20F, 0.29F, 0.085F};
  QVector3D date{0.40F, 0.18F, 0.08F};
  QVector3D pomegranate{0.62F, 0.11F, 0.10F};
  QVector3D fig{0.36F, 0.17F, 0.23F};
  QVector3D shell{0.84F, 0.78F, 0.68F};
  QVector3D water{0.14F, 0.24F, 0.28F};
  QVector3D ember{0.76F, 0.19F, 0.025F};
};

constexpr std::uint8_t k_marketplace_team_slot = 0;
constexpr float k_slab_top = 0.129F;
constexpr float k_walk_y = 0.138F;
constexpr float k_wall_base = 0.135F;

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

auto goods_palette(const CarthageMarketPalette& c) -> MarketGoodsPalette {
  return MarketGoodsPalette{.clay = c.ceramic,
                            .clay_dark = c.sandstone_dark,
                            .wicker = QVector3D(0.66F, 0.52F, 0.30F),
                            .wicker_dark = QVector3D(0.42F, 0.30F, 0.14F),
                            .burlap = QVector3D(0.66F, 0.56F, 0.38F),
                            .timber = c.wood_medium,
                            .timber_dark = c.wood_dark,
                            .metal = c.bronze,
                            .rope = c.rope};
}

void add_spice_bowl(BuildingArchetypeDesc& desc,
                    const QVector3D& base,
                    float radius,
                    const QVector3D& spice,
                    const MarketGoodsPalette& goods) {
  {
    BuildingPartMaterial wicker(desc, k_building_material_wood);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.030F, 0.0F),
                      radius * 0.90F,
                      goods.wicker,
                      BuildingStateMask::Normal);
    desc.add_cylinder(base + QVector3D(0.0F, 0.024F, 0.0F),
                      base + QVector3D(0.0F, 0.038F, 0.0F),
                      radius,
                      goods.wicker_dark,
                      BuildingStateMask::Normal);
  }
  BuildingPartMaterial powder(desc, k_building_material_leather);
  desc.add_cone(base + QVector3D(0.0F, 0.034F, 0.0F),
                base + QVector3D(0.0F, 0.034F + radius * 1.05F, 0.0F),
                radius * 0.94F,
                spice,
                BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, 0.034F + radius * 0.70F, 0.0F),
                base + QVector3D(0.0F, 0.034F + radius * 1.22F, 0.0F),
                radius * 0.34F,
                spice * 1.08F,
                BuildingStateMask::Normal);
}

void add_punic_jar(BuildingArchetypeDesc& desc,
                   const QVector3D& base,
                   float radius,
                   float height,
                   const QVector3D& clay,
                   const QVector3D& band) {
  BuildingPartMaterial ceramic(desc, k_building_material_ceramic);
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, height * 0.76F, 0.0F),
                    radius,
                    clay,
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, height * 0.40F, 0.0F),
                    base + QVector3D(0.0F, height * 0.48F, 0.0F),
                    radius + 0.002F,
                    band,
                    BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, height * 0.72F, 0.0F),
                base + QVector3D(0.0F, height, 0.0F),
                radius * 1.02F,
                clay,
                BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, height * 0.88F, 0.0F),
                    base + QVector3D(0.0F, height * 1.05F, 0.0F),
                    radius * 0.50F,
                    clay * 0.62F,
                    BuildingStateMask::Normal);
}

void add_shop_bays(BuildingArchetypeDesc& desc,
                   float wall_top,
                   const CarthageMarketPalette& c,
                   const MarketGoodsPalette& goods) {
  for (float const z : {-1.19F, -0.40F, 0.40F, 1.19F}) {
    const float part_top = wall_top - 0.008F;
    desc.add_box(QVector3D(0.85F, (k_wall_base + part_top) * 0.5F, z),
                 QVector3D(0.27F, (part_top - k_wall_base) * 0.5F, 0.045F),
                 c.sandstone);
    desc.add_box(QVector3D(0.585F, (k_wall_base + wall_top) * 0.5F, z),
                 QVector3D(0.012F, (wall_top - k_wall_base) * 0.5F, 0.056F),
                 c.plaster,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.585F, k_wall_base + 0.030F, z),
                 QVector3D(0.018F, 0.030F, 0.062F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
  }

  int bay = 0;
  for (float const zc : {-0.80F, 0.0F, 0.80F}) {
    {
      BuildingPartMaterial paint(desc, k_building_material_cloth);
      desc.add_box(QVector3D(1.0855F, (k_wall_base + wall_top - 0.01F) * 0.5F, zc),
                   QVector3D(0.0045F, (wall_top - 0.01F - k_wall_base) * 0.5F, 0.34F),
                   c.plaster * 0.96F,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(1.0845F, 0.30F, zc),
                   QVector3D(0.0045F, 0.006F, 0.34F),
                   (bay % 2 == 0) ? c.cloth_oxblood : c.indigo,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(1.0845F, 0.20F, zc),
                   QVector3D(0.0045F, 0.006F, 0.34F),
                   c.cloth_gold,
                   k_building_state_mask_intact);
    }
    {
      BuildingPartMaterial wood(desc, k_building_material_wood);
      desc.add_box(QVector3D(1.060F, 0.54F, zc),
                   QVector3D(0.019F, 0.006F, 0.30F),
                   c.wood_dark,
                   k_building_state_mask_intact);
    }
    int jar = 0;
    for (float const dz : {-0.19F, 0.0F, 0.19F}) {
      add_punic_jar(desc,
                    QVector3D(1.058F, 0.546F, zc + dz),
                    0.021F + 0.003F * static_cast<float>(jar % 2),
                    0.060F + 0.010F * static_cast<float>(jar),
                    (jar % 2 == 0) ? c.ceramic : c.ceramic_pale,
                    (jar == 1) ? c.cloth_oxblood : c.pepper);
      ++jar;
    }

    desc.add_box(QVector3D(0.67F, (k_wall_base + 0.338F) * 0.5F, zc),
                 QVector3D(0.07F, (0.338F - k_wall_base) * 0.5F, 0.30F),
                 c.brick,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.346F, zc),
                 QVector3D(0.09F, 0.008F, 0.32F),
                 c.plaster,
                 k_building_state_mask_intact);
    {
      BuildingPartMaterial paint(desc, k_building_material_cloth);
      desc.add_box(QVector3D(0.5965F, 0.240F, zc),
                   QVector3D(0.0035F, 0.055F, 0.25F),
                   (bay % 2 == 0) ? c.murex : c.indigo,
                   k_building_state_mask_intact);
    }
    ++bay;
  }

  constexpr float k_top = 0.354F;
  add_market_amphora(
      desc, QVector3D(0.67F, k_top, -0.94F), c.ceramic, c.saffron, 0.80F, true);
  add_spice_bowl(desc, QVector3D(0.67F, k_top, -0.76F), 0.060F, c.cumin, goods);
  add_spice_bowl(desc, QVector3D(0.67F, k_top, -0.62F), 0.055F, c.pepper, goods);

  add_market_scale(desc, QVector3D(0.67F, k_top, 0.0F), goods);
  add_spice_bowl(desc, QVector3D(0.67F, k_top, -0.20F), 0.070F, c.saffron, goods);
  add_spice_bowl(desc, QVector3D(0.67F, k_top, 0.20F), 0.070F, c.spice_red, goods);

  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    const std::array<QVector3D, 4> dyes{c.murex, c.indigo, c.cloth_gold, c.wool};
    int fold = 0;
    for (float const z : {0.60F, 1.00F}) {
      for (int layer = 0; layer < 4; ++layer) {
        const float shrink = 0.004F * static_cast<float>((layer + fold) % 2);
        desc.add_box(
            QVector3D(0.67F, k_top + 0.012F + 0.024F * static_cast<float>(layer), z),
            QVector3D(0.068F - shrink, 0.012F, 0.062F - shrink),
            dyes[static_cast<std::size_t>((layer + fold * 2) % 4)],
            BuildingStateMask::Normal);
      }
      ++fold;
    }
    int bolt = 0;
    for (const QVector3D& at : {QVector3D(0.67F, k_top + 0.028F, 0.745F),
                                QVector3D(0.67F, k_top + 0.028F, 0.855F),
                                QVector3D(0.67F, k_top + 0.076F, 0.800F)}) {
      desc.add_cylinder(at - QVector3D(0.075F, 0.0F, 0.0F),
                        at + QVector3D(0.075F, 0.0F, 0.0F),
                        0.027F,
                        (bolt == 2) ? c.murex_deep
                                    : dyes[static_cast<std::size_t>(bolt)],
                        BuildingStateMask::Normal);
      ++bolt;
    }
  }

  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float const z : {-1.10F, -0.54F}) {
      desc.add_cylinder(QVector3D(1.030F, k_walk_y, z),
                        QVector3D(1.030F, 0.38F, z),
                        0.012F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(1.030F, 0.34F, -1.11F),
                      QVector3D(1.030F, 0.34F, -0.53F),
                      0.011F,
                      c.wood_medium,
                      k_building_state_mask_intact);
  }
  for (float const z : {-1.00F, -0.84F, -0.68F}) {
    add_market_amphora(desc,
                       QVector3D(0.95F, k_walk_y, z),
                       (z > -0.9F && z < -0.8F) ? c.ceramic_pale : c.ceramic,
                       c.spice_red,
                       1.0F,
                       true);
  }
  add_market_sack(
      desc, QVector3D(0.94F, k_walk_y, -0.20F), 0.068F, 0.15F, goods, c.cumin, true);
  add_market_sack(
      desc, QVector3D(0.94F, k_walk_y, 0.00F), 0.066F, 0.14F, goods, c.saffron, true);
  add_market_sack(
      desc, QVector3D(0.94F, k_walk_y, 0.20F), 0.068F, 0.15F, goods, c.spice_red, true);
  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    int rug = 0;
    for (const QVector3D& at : {QVector3D(0.92F, k_walk_y + 0.040F, 0.80F),
                                QVector3D(1.00F, k_walk_y + 0.040F, 0.80F),
                                QVector3D(0.96F, k_walk_y + 0.112F, 0.80F)}) {
      const QVector3D colour = (rug == 0)   ? c.cloth_oxblood
                               : (rug == 1) ? c.indigo
                                            : c.murex;
      desc.add_cylinder(at - QVector3D(0.0F, 0.0F, 0.30F),
                        at + QVector3D(0.0F, 0.0F, 0.30F),
                        0.038F,
                        colour,
                        BuildingStateMask::Normal);
      for (float const dz : {-0.24F, 0.24F}) {
        desc.add_cylinder(at + QVector3D(0.0F, 0.0F, dz - 0.012F),
                          at + QVector3D(0.0F, 0.0F, dz + 0.012F),
                          0.040F,
                          c.cloth_gold,
                          BuildingStateMask::Normal);
      }
      ++rug;
    }
  }
}

void add_well(BuildingArchetypeDesc& desc, float hm, const CarthageMarketPalette& c) {
  const float cx = -0.30F;
  desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                    QVector3D(cx, k_slab_top + 0.012F, 0.0F),
                    0.26F,
                    c.sandstone_dark,
                    BuildingStateMask::All);
  desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                    QVector3D(cx, 0.14F + 0.14F * std::max(hm, 0.6F), 0.0F),
                    0.19F,
                    c.sandstone_light);
  desc.add_cylinder(QVector3D(cx, 0.272F, 0.0F),
                    QVector3D(cx, 0.288F, 0.0F),
                    0.198F,
                    c.plaster,
                    k_building_state_mask_intact);
  for (int band = 0; band < 2; ++band) {
    const float y = 0.17F + 0.045F * static_cast<float>(band);
    desc.add_cylinder(QVector3D(cx, y, 0.0F),
                      QVector3D(cx, y + 0.008F, 0.0F),
                      0.193F,
                      c.sandstone_dark * 1.05F,
                      k_building_state_mask_intact);
  }
  {
    BuildingPartMaterial water(desc, k_building_material_metal);
    desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                      QVector3D(cx, 0.279F, 0.0F),
                      0.155F,
                      c.water,
                      k_building_state_mask_intact);
  }
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float const z : {-0.17F, 0.17F}) {
      desc.add_cylinder(QVector3D(cx, 0.28F, z),
                        QVector3D(cx, 0.57F, z),
                        0.015F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(cx, 0.55F, -0.20F),
                      QVector3D(cx, 0.55F, 0.20F),
                      0.017F,
                      c.wood_medium,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(cx, 0.55F, 0.20F),
                      QVector3D(cx + 0.07F, 0.50F, 0.23F),
                      0.006F,
                      c.wood_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(cx, 0.55F, -0.03F),
                      QVector3D(cx, 0.55F, 0.03F),
                      0.028F,
                      c.rope,
                      BuildingStateMask::Normal);
    desc.add_cylinder(QVector3D(cx, 0.53F, 0.0F),
                      QVector3D(cx, 0.42F, 0.0F),
                      0.004F,
                      c.rope,
                      BuildingStateMask::Normal);
    desc.add_cylinder(QVector3D(cx, 0.37F, 0.0F),
                      QVector3D(cx, 0.42F, 0.0F),
                      0.035F,
                      c.wood_dark,
                      BuildingStateMask::Normal);
    desc.add_cylinder(QVector3D(cx, 0.412F, 0.0F),
                      QVector3D(cx, 0.420F, 0.0F),
                      0.038F,
                      c.bronze,
                      BuildingStateMask::Normal);
  }
  add_market_amphora(desc,
                     QVector3D(-0.50F, k_walk_y, -0.14F),
                     c.ceramic_pale,
                     c.cloth_oxblood,
                     0.85F,
                     true);
}

void add_dye_works(BuildingArchetypeDesc& desc, const CarthageMarketPalette& c) {
  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    for (float const x : {-1.00F, -0.72F}) {
      desc.add_cylinder(QVector3D(x, k_slab_top, -1.02F),
                        QVector3D(x, 0.29F, -1.02F),
                        0.12F,
                        c.sandstone_dark,
                        k_building_state_mask_intact);
      desc.add_cylinder(QVector3D(x, 0.282F, -1.02F),
                        QVector3D(x, 0.296F, -1.02F),
                        0.126F,
                        c.sandstone * 1.04F,
                        k_building_state_mask_intact);
      desc.add_cylinder(QVector3D(x, 0.15F, -1.02F),
                        QVector3D(x, 0.25F, -1.02F),
                        0.1215F,
                        c.murex_deep * 1.2F,
                        k_building_state_mask_intact);
    }
  }
  {
    BuildingPartMaterial dye(desc, k_building_material_metal);
    int vat = 0;
    for (float const x : {-1.00F, -0.72F}) {
      desc.add_cylinder(QVector3D(x, k_slab_top, -1.02F),
                        QVector3D(x, 0.287F, -1.02F),
                        0.095F,
                        (vat == 0) ? c.murex : c.indigo,
                        BuildingStateMask::Normal);
      ++vat;
    }
  }
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_cylinder(QVector3D(-1.03F, 0.24F, -1.00F),
                      QVector3D(-0.93F, 0.46F, -1.06F),
                      0.008F,
                      c.wood_medium,
                      BuildingStateMask::Normal);
    for (float const x : {-1.12F, -0.60F}) {
      desc.add_cylinder(QVector3D(x, k_walk_y, -0.78F),
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
  BuildingPartMaterial shells(desc, k_building_material_ceramic);
  const std::array<QVector3D, 6> heap{QVector3D(-0.88F, k_walk_y, -1.20F),
                                      QVector3D(-0.84F, k_walk_y, -1.17F),
                                      QVector3D(-0.80F, k_walk_y, -1.21F),
                                      QVector3D(-0.86F, k_walk_y + 0.022F, -1.19F),
                                      QVector3D(-0.82F, k_walk_y + 0.020F, -1.19F),
                                      QVector3D(-0.84F, k_walk_y + 0.040F, -1.19F)};
  int shell = 0;
  for (const QVector3D& at : heap) {
    const float lean = (shell % 2 == 0) ? 0.012F : -0.012F;
    desc.add_cone(at,
                  at + QVector3D(lean, 0.034F, 0.010F),
                  0.019F,
                  c.shell * (0.92F + 0.03F * static_cast<float>(shell % 3)),
                  BuildingStateMask::Normal);
    ++shell;
  }
}

void add_gateway(BuildingArchetypeDesc& desc,
                 float hm,
                 const CarthageMarketPalette& c) {
  float const pylon_h = 0.80F * hm;
  float const pylon_top = 0.14F + pylon_h;
  for (float const z : {-0.50F, 0.50F}) {
    desc.add_box(QVector3D(-1.14F, (k_slab_top + 0.18F) * 0.5F, z),
                 QVector3D(0.085F, (0.18F - k_slab_top) * 0.5F, 0.085F),
                 c.sandstone_dark,
                 BuildingStateMask::All);
    desc.add_box(QVector3D(-1.14F, (k_wall_base + pylon_top) * 0.5F, z),
                 QVector3D(0.07F, (pylon_top - k_wall_base) * 0.5F, 0.07F),
                 c.sandstone);
    desc.add_box(QVector3D(-1.14F, pylon_top - 0.030F, z),
                 QVector3D(0.078F, 0.008F, 0.078F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.14F, pylon_top + 0.02F, z),
                 QVector3D(0.085F, 0.02F, 0.085F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.14F, pylon_top + 0.046F, z),
                 QVector3D(0.092F, 0.006F, 0.092F),
                 c.plaster,
                 k_building_state_mask_intact);
  }
  float const beam_y = pylon_top + 0.07F;
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_box(QVector3D(-1.14F, beam_y, 0.0F),
                 QVector3D(0.05F, 0.035F, 0.62F),
                 c.wood_dark,
                 k_building_state_mask_intact);
    for (int end = 0; end < 7; ++end) {
      const float z = -0.42F + 0.14F * static_cast<float>(end);
      desc.add_cylinder(QVector3D(-1.196F, beam_y - 0.008F, z),
                        QVector3D(-1.186F, beam_y - 0.008F, z),
                        0.016F,
                        c.wood_medium,
                        k_building_state_mask_intact);
    }
  }
  add_punic_tanit_relief(desc,
                         QVector3D(-1.15F, beam_y + 0.17F, 0.0F),
                         BuildingFacadePlane::ZY,
                         0.28F,
                         c.cloth_gold,
                         c.brick_dark);
  BuildingPartMaterial cloth(desc, k_building_material_cloth);
  for (float const z : {-0.50F, 0.50F}) {
    desc.add_palette_box(QVector3D(-1.215F, 0.14F + pylon_h * 0.62F, z),
                         QVector3D(0.004F, 0.12F, 0.05F),
                         k_marketplace_team_slot,
                         BuildingStateMask::Normal);
  }
}

void add_stall_goods(BuildingArchetypeDesc& desc,
                     const CarthageMarketPalette& c,
                     const MarketGoodsPalette& goods) {
  constexpr float k_counter = 0.34F;
  add_spice_bowl(desc, QVector3D(-0.58F, k_counter, 0.83F), 0.068F, c.saffron, goods);
  add_spice_bowl(desc, QVector3D(-0.40F, k_counter, 0.83F), 0.068F, c.herb, goods);
  add_spice_bowl(desc, QVector3D(-0.22F, k_counter, 0.83F), 0.068F, c.cumin, goods);
  add_punic_jar(
      desc, QVector3D(-0.52F, 0.237F, 0.84F), 0.030F, 0.060F, c.ceramic, c.pepper);
  add_punic_jar(desc,
                QVector3D(-0.28F, 0.237F, 0.84F),
                0.028F,
                0.056F,
                c.ceramic_pale,
                c.spice_red);
  add_market_sack(desc,
                  QVector3D(-0.56F, k_walk_y, 1.02F),
                  0.058F,
                  0.14F,
                  goods,
                  c.spice_red,
                  true);
  add_market_sack(
      desc, QVector3D(-0.24F, k_walk_y, 1.02F), 0.058F, 0.14F, goods, c.saffron, true);

  add_market_basket(
      desc, QVector3D(-0.58F, k_counter, -0.83F), 0.068F, goods, c.date, c.fig);
  add_spice_bowl(
      desc, QVector3D(-0.40F, k_counter, -0.83F), 0.068F, c.spice_red, goods);
  add_market_basket(
      desc, QVector3D(-0.22F, k_counter, -0.83F), 0.068F, goods, c.pomegranate, c.date);
  add_punic_jar(desc,
                QVector3D(-0.52F, 0.237F, -0.84F),
                0.030F,
                0.062F,
                c.ceramic,
                c.cloth_oxblood);
  add_punic_jar(
      desc, QVector3D(-0.28F, 0.237F, -0.84F), 0.030F, 0.058F, c.ceramic, c.pepper);
  add_market_sack(
      desc, QVector3D(-0.56F, k_walk_y, -1.02F), 0.058F, 0.15F, goods, c.cumin, false);
  add_market_sack(
      desc, QVector3D(-0.24F, k_walk_y, -1.02F), 0.058F, 0.14F, goods, c.pepper, true);
}

void add_yard_goods(BuildingArchetypeDesc& desc,
                    const CarthageMarketPalette& c,
                    const MarketGoodsPalette& goods) {
  add_market_amphora(
      desc, QVector3D(-0.98F, k_walk_y, 1.04F), c.ceramic, c.cloth_gold, 1.0F, true);
  add_market_amphora(desc,
                     QVector3D(-0.83F, k_walk_y, 1.09F),
                     c.ceramic * 0.85F,
                     c.spice_red,
                     1.0F,
                     true);
  add_market_crate(desc,
                   QVector3D(-0.98F, k_walk_y + 0.070F, 0.84F),
                   QVector3D(0.070F, 0.070F, 0.070F),
                   goods,
                   true);
  add_market_basket(desc,
                    QVector3D(-0.98F, k_walk_y + 0.148F, 0.84F),
                    0.058F,
                    goods,
                    c.pomegranate,
                    c.date);

  add_market_sack(
      desc, QVector3D(0.12F, k_walk_y, 1.04F), 0.072F, 0.19F, goods, c.cumin, false);
  add_market_sack(
      desc, QVector3D(0.29F, k_walk_y, 1.07F), 0.068F, 0.17F, goods, c.saffron, true);
  add_market_crate(desc,
                   QVector3D(0.22F, k_walk_y + 0.070F, 0.88F),
                   QVector3D(0.080F, 0.070F, 0.070F),
                   goods,
                   true);
  add_punic_jar(desc,
                QVector3D(0.22F, k_walk_y + 0.148F, 0.88F),
                0.040F,
                0.090F,
                c.ceramic_pale,
                c.cloth_oxblood);
}

auto build_marketplace_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  CarthageMarketPalette const c;
  MarketGoodsPalette const goods = goods_palette(c);
  float hm = 1.0F;
  if (state == BuildingState::Damaged) {
    hm = 0.75F;
  } else if (state == BuildingState::Destroyed) {
    hm = 0.35F;
  }

  BuildingArchetypeDesc desc("carthage_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.35F, 0.05F, 1.35F), c.sandstone_dark);
  desc.add_box(QVector3D(0.0F, (0.10F + k_slab_top) * 0.5F, 0.0F),
               QVector3D(1.28F, (k_slab_top - 0.10F) * 0.5F, 1.28F),
               c.mortar);
  add_market_paving(
      desc,
      MarketPaving{.min_x = -1.25F,
                   .max_x = 0.50F,
                   .min_z = -1.25F,
                   .max_z = 1.25F,
                   .cell = 0.32F,
                   .gap = 0.010F,
                   .base_y = k_slab_top,
                   .min_rise = 0.008F,
                   .max_rise = 0.012F,
                   .tones = {c.sandstone, c.sandstone_light, c.sandstone * 0.90F},
                   .seed = 223});
  desc.add_box(QVector3D(0.87F, (k_slab_top + k_walk_y) * 0.5F, 0.0F),
               QVector3D(0.345F, (k_walk_y - k_slab_top) * 0.5F, 1.235F),
               c.floor_earth);

  float const wall_h = 0.60F * hm;
  float const wall_top = 0.14F + wall_h;
  desc.add_box(QVector3D(1.16F, (k_wall_base + wall_top) * 0.5F, 0.0F),
               QVector3D(0.07F, (wall_top - k_wall_base) * 0.5F, 1.24F),
               c.brick);
  desc.add_box(QVector3D(1.2335F, (0.30F + wall_top - 0.02F) * 0.5F, 0.0F),
               QVector3D(0.0035F, (wall_top - 0.32F) * 0.5F, 1.22F),
               c.plaster,
               k_building_state_mask_intact);
  {
    BuildingPartMaterial paint(desc, k_building_material_cloth);
    desc.add_box(QVector3D(1.238F, wall_top - 0.035F, 0.0F),
                 QVector3D(0.005F, 0.012F, 1.21F),
                 c.cloth_oxblood,
                 k_building_state_mask_intact);
  }
  add_shop_bays(desc, wall_top, c, goods);
  desc.add_box(QVector3D(0.58F, wall_top + 0.035F, 0.0F),
               QVector3D(0.05F, 0.04F, 1.24F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.89F, wall_top + 0.085F, 0.0F),
               QVector3D(0.35F, 0.012F, 1.25F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float z = -1.10F; z <= 1.11F; z += 0.22F) {
      desc.add_cylinder(QVector3D(0.52F, wall_top + 0.062F, z),
                        QVector3D(0.60F, wall_top + 0.062F, z),
                        0.012F,
                        c.wood_dark,
                        k_building_state_mask_intact);
    }
  }
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

  add_market_stall(desc,
                   MarketStall{.cx = -0.40F,
                               .cz = 0.82F,
                               .side = 1.0F,
                               .ground_y = 0.132F,
                               .counter_top = 0.34F,
                               .back_post_top = 0.855F,
                               .front_post_top = 0.735F,
                               .board = c.wood_light,
                               .apron = c.wood_medium,
                               .apron_panel = c.indigo},
                   goods);
  add_market_stall(desc,
                   MarketStall{.cx = -0.40F,
                               .cz = -0.82F,
                               .side = -1.0F,
                               .ground_y = 0.132F,
                               .counter_top = 0.34F,
                               .back_post_top = 0.855F,
                               .front_post_top = 0.735F,
                               .board = c.wood_light,
                               .apron = c.wood_medium,
                               .apron_panel = c.cloth_oxblood},
                   goods);
  add_stall_goods(desc, c, goods);

  add_well(desc, hm, c);
  add_dye_works(desc, c);
  add_gateway(desc, hm, c);
  add_punic_horned_crown(desc,
                         QVector3D(0.89F, wall_top + 0.10F, 0.0F),
                         0.50F,
                         c.wood_dark,
                         c.cloth_gold,
                         c.ember);
  add_yard_goods(desc, c, goods);

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

const std::array<MarketHanging, 8> k_hangings{{
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
    MarketHanging{.pivot = QVector3D(-0.56F, 0.708F, 0.605F),
                  .length = 0.14F,
                  .radius = 0.018F,
                  .color = QVector3D(0.54F, 0.10F, 0.04F),
                  .beads = 4},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.708F, 0.605F),
                  .length = 0.12F,
                  .radius = 0.020F,
                  .color = QVector3D(0.20F, 0.29F, 0.09F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.708F, -0.605F),
                  .length = 0.13F,
                  .radius = 0.022F,
                  .color = QVector3D(0.40F, 0.18F, 0.08F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.708F, -0.605F),
                  .length = 0.15F,
                  .radius = 0.019F,
                  .color = QVector3D(0.86F, 0.82F, 0.72F),
                  .beads = 4},
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
