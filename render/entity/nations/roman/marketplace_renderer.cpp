#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "building_architecture.h"
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

namespace Render::GL::Roman {
namespace {

struct RomanMarketPalette {
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D marble = BuildingPalette::k_marble;
  QVector3D mortar{0.52F, 0.49F, 0.43F};
  QVector3D signinum{0.56F, 0.35F, 0.26F};
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_light = BuildingPalette::k_cedar_light;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D cloth_red_faded{0.68F, 0.16F, 0.10F};
  QVector3D cloth_gold{0.72F, 0.52F, 0.16F};
  QVector3D cloth_cream{0.84F, 0.79F, 0.66F};
  QVector3D pompeian_red{0.56F, 0.13F, 0.09F};
  QVector3D dado_black{0.14F, 0.12F, 0.10F};
  QVector3D wall_ochre{0.72F, 0.52F, 0.22F};
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D iron{0.12F, 0.13F, 0.13F};
  QVector3D water{0.17F, 0.33F, 0.39F};
  QVector3D water_light{0.34F, 0.52F, 0.58F};
  QVector3D wine{0.11F, 0.03F, 0.045F};
  QVector3D apple{0.60F, 0.13F, 0.08F};
  QVector3D pear{0.64F, 0.58F, 0.22F};
  QVector3D olive{0.25F, 0.31F, 0.10F};
  QVector3D grape{0.26F, 0.08F, 0.20F};
  QVector3D fig{0.36F, 0.17F, 0.23F};
  QVector3D cabbage{0.34F, 0.48F, 0.20F};
  QVector3D wheat{0.78F, 0.64F, 0.34F};
  QVector3D lentil{0.50F, 0.30F, 0.14F};
  QVector3D bread{0.70F, 0.46F, 0.20F};
  QVector3D bread_crust{0.46F, 0.25F, 0.10F};
  QVector3D cheese{0.86F, 0.76F, 0.50F};
  QVector3D cheese_rind{0.62F, 0.48F, 0.24F};
  QVector3D ochre{0.66F, 0.38F, 0.09F};
  QVector3D gold = BuildingPalette::k_gold;
};

constexpr std::uint8_t k_marketplace_team_slot = 0;
constexpr float k_slab_top = 0.149F;
constexpr float k_walk_y = 0.158F;
constexpr float k_wall_base = 0.155F;

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

auto goods_palette(const RomanMarketPalette& c) -> MarketGoodsPalette {
  return MarketGoodsPalette{.clay = c.terracotta,
                            .clay_dark = c.terracotta_dark,
                            .wicker = QVector3D(0.62F, 0.48F, 0.28F),
                            .wicker_dark = QVector3D(0.40F, 0.28F, 0.14F),
                            .burlap = QVector3D(0.62F, 0.53F, 0.37F),
                            .timber = c.cedar,
                            .timber_dark = c.cedar_dark,
                            .metal = c.bronze,
                            .rope = QVector3D(0.52F, 0.42F, 0.26F)};
}

void add_small_jar(BuildingArchetypeDesc& desc,
                   const QVector3D& base,
                   float radius,
                   float height,
                   const QVector3D& clay) {
  BuildingPartMaterial ceramic(desc, k_building_material_ceramic);
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, height * 0.78F, 0.0F),
                    radius,
                    clay,
                    BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, height * 0.74F, 0.0F),
                base + QVector3D(0.0F, height, 0.0F),
                radius * 1.02F,
                clay,
                BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, height * 0.90F, 0.0F),
                    base + QVector3D(0.0F, height * 1.06F, 0.0F),
                    radius * 0.52F,
                    clay * 0.66F,
                    BuildingStateMask::Normal);
}

void add_lean_to_roof(BuildingArchetypeDesc& desc,
                      float low_x,
                      float high_x,
                      float low_y,
                      float high_y,
                      float half_z,
                      const RomanMarketPalette& c) {
  const float run = high_x - low_x;
  const float rise = high_y - low_y;
  const float angle = std::atan2(rise, run) * 180.0F / 3.14159265F;
  const float slope = std::sqrt(run * run + rise * rise);
  {
    BuildingPartMaterial clay(desc, k_building_material_ceramic);
    desc.add_rotated_box(
        QVector3D((low_x + high_x) * 0.5F, (low_y + high_y) * 0.5F, 0.0F),
        QVector3D(slope * 0.5F + 0.03F, 0.018F, half_z + 0.04F),
        QVector3D(0.0F, 0.0F, angle),
        c.terracotta,
        k_building_state_mask_intact);
    const int rolls = static_cast<int>(half_z * 2.0F / 0.16F);
    for (int i = 0; i <= rolls; ++i) {
      const float z =
          -half_z + static_cast<float>(i) * (half_z * 2.0F / static_cast<float>(rolls));
      const float tone = 0.94F + 0.10F * static_cast<float>((i * 7) % 5) / 4.0F;
      desc.add_cylinder(QVector3D(low_x - 0.02F, low_y + 0.024F, z),
                        QVector3D(high_x + 0.01F, high_y + 0.030F, z),
                        0.022F,
                        ((i % 2 == 0) ? c.terracotta_dark : c.terracotta) * tone,
                        k_building_state_mask_intact);
      desc.add_cone(QVector3D(low_x - 0.034F, low_y + 0.012F, z),
                    QVector3D(low_x - 0.040F, low_y + 0.078F, z),
                    0.024F,
                    c.terracotta_dark * 1.05F,
                    BuildingStateMask::Normal);
    }
    desc.add_box(QVector3D(high_x - 0.02F, high_y + 0.035F, 0.0F),
                 QVector3D(0.035F, 0.022F, half_z + 0.045F),
                 c.terracotta_dark,
                 k_building_state_mask_intact);
  }
  BuildingPartMaterial wood(desc, k_building_material_wood);
  for (float z = -half_z + 0.10F; z <= half_z - 0.05F; z += 0.34F) {
    desc.add_box(QVector3D(low_x + 0.02F, low_y - 0.030F, z),
                 QVector3D(0.035F, 0.018F, 0.018F),
                 c.cedar_dark,
                 k_building_state_mask_intact);
  }
  desc.add_cylinder(QVector3D(low_x - 0.005F, low_y - 0.008F, -half_z - 0.04F),
                    QVector3D(low_x - 0.005F, low_y - 0.008F, half_z + 0.04F),
                    0.016F,
                    c.cedar_dark,
                    k_building_state_mask_intact);
}

void add_shop_bays(BuildingArchetypeDesc& desc,
                   float wall_top,
                   const RomanMarketPalette& c,
                   const MarketGoodsPalette& goods) {
  for (float const z : {-1.17F, -0.40F, 0.40F, 1.17F}) {
    const float part_top = wall_top - 0.008F;
    desc.add_box(QVector3D(0.85F, (k_wall_base + part_top) * 0.5F, z),
                 QVector3D(0.27F, (part_top - k_wall_base) * 0.5F, 0.04F),
                 c.limestone_shade);
    desc.add_box(QVector3D(0.585F, (k_wall_base + wall_top) * 0.5F, z),
                 QVector3D(0.012F, (wall_top - k_wall_base) * 0.5F, 0.052F),
                 c.marble,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.585F, wall_top - 0.016F, z),
                 QVector3D(0.018F, 0.011F, 0.058F),
                 c.marble * 0.95F,
                 k_building_state_mask_intact);
  }

  for (float const zc : {-0.79F, 0.0F, 0.79F}) {
    {
      BuildingPartMaterial paint(desc, k_building_material_cloth);
      desc.add_box(QVector3D(1.0965F, (k_wall_base + 0.32F) * 0.5F, zc),
                   QVector3D(0.0045F, (0.32F - k_wall_base) * 0.5F, 0.34F),
                   c.dado_black,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(1.0975F, (0.32F + wall_top - 0.01F) * 0.5F, zc),
                   QVector3D(0.0035F, (wall_top - 0.01F - 0.32F) * 0.5F, 0.34F),
                   c.pompeian_red,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(1.0950F, 0.32F, zc),
                   QVector3D(0.004F, 0.006F, 0.34F),
                   c.wall_ochre,
                   k_building_state_mask_intact);
    }
    {
      BuildingPartMaterial wood(desc, k_building_material_wood);
      desc.add_box(QVector3D(1.072F, 0.56F, zc),
                   QVector3D(0.019F, 0.006F, 0.30F),
                   c.cedar_dark,
                   k_building_state_mask_intact);
    }
    int jar = 0;
    for (float const dz : {-0.20F, 0.0F, 0.20F}) {
      const QVector3D clay = (jar % 2 == 0) ? c.terracotta : c.terracotta_dark * 1.12F;
      add_small_jar(desc,
                    QVector3D(1.070F, 0.566F, zc + dz),
                    0.020F + 0.004F * static_cast<float>(jar % 2),
                    0.058F + 0.012F * static_cast<float>(jar),
                    clay);
      ++jar;
    }

    desc.add_box(QVector3D(0.67F, (k_wall_base + 0.358F) * 0.5F, zc),
                 QVector3D(0.07F, (0.358F - k_wall_base) * 0.5F, 0.30F),
                 c.limestone_shade,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.366F, zc),
                 QVector3D(0.09F, 0.008F, 0.32F),
                 c.marble,
                 k_building_state_mask_intact);
    {
      BuildingPartMaterial paint(desc, k_building_material_cloth);
      desc.add_box(QVector3D(0.5965F, 0.255F, zc),
                   QVector3D(0.0035F, 0.060F, 0.25F),
                   c.pompeian_red,
                   k_building_state_mask_intact);
    }
  }

  {
    BuildingPartMaterial ceramic(desc, k_building_material_ceramic);
    for (float const z : {-0.93F, -0.65F}) {
      desc.add_cylinder(QVector3D(0.67F, 0.372F, z),
                        QVector3D(0.67F, 0.378F, z),
                        0.055F,
                        c.terracotta_dark,
                        k_building_state_mask_intact);
      desc.add_cylinder(QVector3D(0.67F, 0.3745F, z),
                        QVector3D(0.67F, 0.3790F, z),
                        0.042F,
                        c.wine,
                        k_building_state_mask_intact);
    }
  }
  add_market_amphora(
      desc, QVector3D(0.67F, 0.374F, -0.79F), c.terracotta, c.cloth_gold, 0.75F);

  add_market_scale(desc, QVector3D(0.67F, 0.374F, 0.0F), goods);
  add_market_basket(
      desc, QVector3D(0.67F, 0.374F, -0.19F), 0.072F, goods, c.pear, c.apple);
  add_market_basket(
      desc, QVector3D(0.67F, 0.374F, 0.19F), 0.072F, goods, c.grape, c.fig);

  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    const std::array<QVector3D, 4> dyes{
        c.cloth_red, c.cloth_gold, c.blue_accent, c.cloth_cream};
    int fold = 0;
    for (float const z : {0.60F, 0.98F}) {
      for (int layer = 0; layer < 4; ++layer) {
        const float shrink = 0.004F * static_cast<float>((layer + fold) % 2);
        desc.add_box(
            QVector3D(0.67F, 0.374F + 0.012F + 0.024F * static_cast<float>(layer), z),
            QVector3D(0.068F - shrink, 0.012F, 0.062F - shrink),
            dyes[static_cast<std::size_t>((layer + fold) % 4)],
            BuildingStateMask::Normal);
      }
      ++fold;
    }
    int bolt = 0;
    for (const QVector3D& at : {QVector3D(0.67F, 0.402F, 0.735F),
                                QVector3D(0.67F, 0.402F, 0.845F),
                                QVector3D(0.67F, 0.450F, 0.790F)}) {
      desc.add_cylinder(at - QVector3D(0.075F, 0.0F, 0.0F),
                        at + QVector3D(0.075F, 0.0F, 0.0F),
                        0.027F,
                        dyes[static_cast<std::size_t>(bolt % 4)] * 0.96F,
                        BuildingStateMask::Normal);
      ++bolt;
    }
  }

  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    for (float const z : {-1.08F, -0.56F}) {
      desc.add_cylinder(QVector3D(1.035F, k_walk_y, z),
                        QVector3D(1.035F, 0.40F, z),
                        0.012F,
                        c.cedar_dark,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(1.035F, 0.36F, -1.09F),
                      QVector3D(1.035F, 0.36F, -0.55F),
                      0.011F,
                      c.cedar,
                      k_building_state_mask_intact);
  }
  for (float const z : {-0.98F, -0.82F, -0.66F}) {
    add_market_amphora(desc,
                       QVector3D(0.95F, k_walk_y, z),
                       (z < -0.9F) ? c.terracotta_dark * 1.08F : c.terracotta,
                       c.terracotta_dark);
  }
  add_market_basket(
      desc, QVector3D(0.95F, k_walk_y, -0.19F), 0.085F, goods, c.cabbage, c.olive);
  add_market_crate(desc,
                   QVector3D(0.96F, k_walk_y + 0.07F, 0.17F),
                   QVector3D(0.075F, 0.07F, 0.075F),
                   goods,
                   true);
  add_market_sack(
      desc, QVector3D(0.94F, k_walk_y, 0.64F), 0.068F, 0.16F, goods, c.wheat, true);
  add_market_sack(
      desc, QVector3D(1.00F, k_walk_y, 0.80F), 0.070F, 0.19F, goods, c.wheat, false);
  add_market_sack(
      desc, QVector3D(0.94F, k_walk_y, 0.95F), 0.062F, 0.15F, goods, c.lentil, true);
}

void add_forum_fountain(BuildingArchetypeDesc& desc,
                        float hm,
                        const RomanMarketPalette& c) {
  const float cx = -0.30F;
  desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                    QVector3D(cx, 0.166F, 0.0F),
                    0.30F,
                    c.limestone_dark,
                    BuildingStateMask::All);
  desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                    QVector3D(cx, 0.26F * std::max(hm, 0.65F), 0.0F),
                    0.20F,
                    c.marble);
  desc.add_cylinder(QVector3D(cx, 0.255F, 0.0F),
                    QVector3D(cx, 0.268F, 0.0F),
                    0.212F,
                    c.marble * 1.03F,
                    k_building_state_mask_intact);
  {
    BuildingPartMaterial water(desc, k_building_material_metal);
    desc.add_cylinder(QVector3D(cx, k_slab_top, 0.0F),
                      QVector3D(cx, 0.252F, 0.0F),
                      0.172F,
                      c.water,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(QVector3D(cx, 0.24F, 0.0F),
                    QVector3D(cx, 0.29F, 0.0F),
                    0.058F,
                    c.marble * 0.96F,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(cx, 0.28F, 0.0F),
                    QVector3D(cx, 0.46F, 0.0F),
                    0.033F,
                    c.marble,
                    k_building_state_mask_intact);
  desc.add_cone(QVector3D(cx, 0.44F, 0.0F),
                QVector3D(cx, 0.40F, 0.0F),
                0.10F,
                c.marble,
                k_building_state_mask_intact);
  desc.add_cone(QVector3D(cx, 0.452F, 0.0F),
                QVector3D(cx, 0.505F, 0.0F),
                0.018F,
                c.marble * 1.02F,
                k_building_state_mask_intact);
  BuildingPartMaterial water(desc, k_building_material_metal);
  desc.add_cylinder(QVector3D(cx, 0.44F, 0.0F),
                    QVector3D(cx, 0.455F, 0.0F),
                    0.09F,
                    c.water_light,
                    BuildingStateMask::Normal);
  for (int i = 0; i < 4; ++i) {
    const float angle = 0.785398F + static_cast<float>(i) * 1.570796F;
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    desc.add_cylinder(QVector3D(cx + ca * 0.095F, 0.446F, sa * 0.095F),
                      QVector3D(cx + ca * 0.140F, 0.253F, sa * 0.140F),
                      0.0055F,
                      c.water_light,
                      BuildingStateMask::Normal);
  }
}

void add_gateway(BuildingArchetypeDesc& desc, float hm, const RomanMarketPalette& c) {
  const float col_h = 0.84F * hm;
  const float col_top = 0.16F + col_h;
  for (float const z : {-0.52F, 0.52F}) {
    desc.add_box(QVector3D(-1.12F, (k_slab_top + 0.214F) * 0.5F, z),
                 QVector3D(0.075F, (0.214F - k_slab_top) * 0.5F, 0.075F),
                 c.marble,
                 BuildingStateMask::All);
    desc.add_cylinder(QVector3D(-1.12F, 0.214F, z),
                      QVector3D(-1.12F, 0.236F, z),
                      0.064F,
                      c.marble,
                      BuildingStateMask::All);
    desc.add_cylinder(QVector3D(-1.12F, 0.236F, z),
                      QVector3D(-1.12F, 0.250F, z),
                      0.057F,
                      c.marble * 0.97F,
                      BuildingStateMask::All);
    desc.add_cylinder(QVector3D(-1.12F, 0.250F, z),
                      QVector3D(-1.12F, col_top, z),
                      0.052F,
                      c.marble * 0.93F);
    for (int f = 0; f < 8; ++f) {
      const float angle = static_cast<float>(f) * 0.785398F;
      desc.add_cylinder(QVector3D(-1.12F + std::cos(angle) * 0.049F,
                                  0.265F,
                                  z + std::sin(angle) * 0.049F),
                        QVector3D(-1.12F + std::cos(angle) * 0.049F,
                                  col_top - 0.07F,
                                  z + std::sin(angle) * 0.049F),
                        0.0055F,
                        c.marble * 0.80F,
                        k_building_state_mask_intact);
    }
    desc.add_cylinder(QVector3D(-1.12F, col_top - 0.068F, z),
                      QVector3D(-1.12F, col_top - 0.056F, z),
                      0.057F,
                      c.marble,
                      k_building_state_mask_intact);
    desc.add_cone(QVector3D(-1.12F, col_top, z),
                  QVector3D(-1.12F, col_top - 0.060F, z),
                  0.074F,
                  c.marble,
                  k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.12F, col_top + 0.018F, z),
                 QVector3D(0.078F, 0.018F, 0.078F),
                 c.marble * 1.02F,
                 k_building_state_mask_intact);
  }
  const float gate_y = col_top + 0.10F;
  desc.add_box(QVector3D(-1.12F, gate_y, 0.0F),
               QVector3D(0.07F, 0.065F, 0.64F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.12F, gate_y + 0.085F, 0.0F),
               QVector3D(0.085F, 0.02F, 0.68F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.1935F, gate_y - 0.004F, 0.0F),
               QVector3D(0.0035F, 0.036F, 0.37F),
               c.marble,
               BuildingStateMask::Normal);
  {
    BuildingPartMaterial paint(desc, k_building_material_cloth);
    for (int letter = 0; letter < 7; ++letter) {
      const float z = -0.27F + 0.09F * static_cast<float>(letter);
      desc.add_box(QVector3D(-1.1985F, gate_y - 0.004F, z),
                   QVector3D(0.0015F, 0.013F, 0.026F),
                   c.pompeian_red * 0.85F,
                   BuildingStateMask::Normal);
    }
  }
  add_roman_aquila_relief(desc,
                          QVector3D(-1.15F, gate_y + 0.21F, 0.0F),
                          BuildingFacadePlane::ZY,
                          0.30F,
                          c.gold,
                          c.terracotta_dark);
  BuildingPartMaterial cloth(desc, k_building_material_cloth);
  desc.add_palette_box(QVector3D(-1.10F, gate_y - 0.15F, 0.0F),
                       QVector3D(0.006F, 0.085F, 0.075F),
                       k_marketplace_team_slot,
                       BuildingStateMask::Normal);
}

void add_mensa_ponderaria(BuildingArchetypeDesc& desc,
                          const RomanMarketPalette& c,
                          const MarketGoodsPalette& goods) {
  const QVector3D at(0.22F, 0.0F, -0.80F);
  desc.add_box(QVector3D(at.x(), 0.330F, at.z()),
               QVector3D(0.19F, 0.022F, 0.09F),
               c.marble,
               k_building_state_mask_intact);
  for (float const dx : {-0.14F, 0.14F}) {
    desc.add_box(QVector3D(at.x() + dx, (k_slab_top + 0.308F) * 0.5F, at.z()),
                 QVector3D(0.028F, (0.308F - k_slab_top) * 0.5F, 0.07F),
                 c.limestone_shade,
                 k_building_state_mask_intact);
  }
  int cup = 0;
  for (float const dx : {-0.12F, 0.0F, 0.12F}) {
    desc.add_cylinder(QVector3D(at.x() + dx, 0.3505F, at.z()),
                      QVector3D(at.x() + dx, 0.3535F, at.z()),
                      0.040F - 0.008F * static_cast<float>(cup),
                      c.limestone_dark * 0.55F,
                      k_building_state_mask_intact);
    ++cup;
  }
  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_cylinder(QVector3D(0.46F, k_walk_y, -0.92F),
                      QVector3D(0.46F, 0.24F, -0.92F),
                      0.055F,
                      c.cedar,
                      BuildingStateMask::Normal);
  }
  BuildingPartMaterial metal(desc, k_building_material_metal);
  for (float const y : {0.175F, 0.222F}) {
    desc.add_cylinder(QVector3D(0.46F, y, -0.92F),
                      QVector3D(0.46F, y + 0.010F, -0.92F),
                      0.058F,
                      goods.metal * 0.85F,
                      BuildingStateMask::Normal);
  }
}

void add_stall_goods(BuildingArchetypeDesc& desc,
                     const RomanMarketPalette& c,
                     const MarketGoodsPalette& goods) {
  constexpr float k_counter = 0.36F;
  add_market_basket(
      desc, QVector3D(-0.58F, k_counter, 0.81F), 0.070F, goods, c.apple, c.pear);
  add_market_basket(
      desc, QVector3D(-0.40F, k_counter, 0.81F), 0.070F, goods, c.cabbage, c.olive);
  add_market_basket(
      desc, QVector3D(-0.22F, k_counter, 0.81F), 0.070F, goods, c.grape, c.fig);
  {
    BuildingPartMaterial produce(desc, k_building_material_leather);
    add_market_fruit(
        desc, QVector3D(-0.49F, k_counter + 0.018F, 0.71F), 0.020F, c.apple);
    add_market_fruit(
        desc, QVector3D(-0.46F, k_counter + 0.018F, 0.70F), 0.019F, c.pear);
    add_market_fruit(
        desc, QVector3D(-0.31F, k_counter + 0.018F, 0.71F), 0.020F, c.apple);
  }
  add_small_jar(desc, QVector3D(-0.52F, 0.259F, 0.82F), 0.030F, 0.060F, c.terracotta);
  add_small_jar(
      desc, QVector3D(-0.28F, 0.259F, 0.82F), 0.028F, 0.055F, c.terracotta_dark);
  add_market_sack(
      desc, QVector3D(-0.56F, k_walk_y, 1.00F), 0.058F, 0.14F, goods, c.olive, true);
  add_market_sack(
      desc, QVector3D(-0.24F, k_walk_y, 1.00F), 0.058F, 0.14F, goods, c.lentil, true);

  {
    BuildingPartMaterial food(desc, k_building_material_leather);
    for (float const x : {-0.60F, -0.50F, -0.40F}) {
      for (float const z : {-0.75F, -0.85F}) {
        const QVector3D loaf(x, k_counter, z);
        desc.add_cylinder(loaf,
                          loaf + QVector3D(0.0F, 0.018F, 0.0F),
                          0.038F,
                          c.bread_crust,
                          BuildingStateMask::Normal);
        desc.add_cone(loaf + QVector3D(0.0F, 0.018F, 0.0F),
                      loaf + QVector3D(0.0F, 0.042F, 0.0F),
                      0.038F,
                      c.bread,
                      BuildingStateMask::Normal);
      }
    }
    for (int wheel = 0; wheel < 3; ++wheel) {
      const float y = k_counter + 0.036F * static_cast<float>(wheel);
      const float r = 0.056F - 0.004F * static_cast<float>(wheel);
      desc.add_cylinder(QVector3D(-0.24F, y, -0.80F),
                        QVector3D(-0.24F, y + 0.034F, -0.80F),
                        r,
                        c.cheese_rind,
                        BuildingStateMask::Normal);
      desc.add_cylinder(QVector3D(-0.24F, y + 0.004F, -0.80F),
                        QVector3D(-0.24F, y + 0.030F, -0.80F),
                        r + 0.002F,
                        c.cheese,
                        BuildingStateMask::Normal);
    }
  }
  add_small_jar(desc, QVector3D(-0.52F, 0.259F, -0.82F), 0.030F, 0.060F, c.terracotta);
  add_small_jar(desc, QVector3D(-0.28F, 0.259F, -0.82F), 0.030F, 0.062F, c.terracotta);
  add_market_sack(
      desc, QVector3D(-0.56F, k_walk_y, -1.00F), 0.060F, 0.16F, goods, c.wheat, false);
  add_market_sack(
      desc, QVector3D(-0.24F, k_walk_y, -1.00F), 0.060F, 0.15F, goods, c.wheat, true);
}

void add_yard_goods(BuildingArchetypeDesc& desc,
                    const RomanMarketPalette& c,
                    const MarketGoodsPalette& goods) {
  add_market_crate(desc,
                   QVector3D(-0.98F, k_walk_y + 0.080F, 1.02F),
                   QVector3D(0.090F, 0.080F, 0.090F),
                   goods,
                   true);
  add_market_crate(desc,
                   QVector3D(-0.97F, k_walk_y + 0.168F + 0.055F, 1.02F),
                   QVector3D(0.065F, 0.055F, 0.065F),
                   goods,
                   true);
  add_market_crate(desc,
                   QVector3D(-0.80F, k_walk_y + 0.060F, 1.07F),
                   QVector3D(0.070F, 0.060F, 0.070F),
                   goods,
                   false);
  {
    BuildingPartMaterial produce(desc, k_building_material_leather);
    const float top = k_walk_y + 0.120F;
    add_market_fruit(desc, QVector3D(-0.83F, top + 0.012F, 1.04F), 0.028F, c.apple);
    add_market_fruit(
        desc, QVector3D(-0.77F, top + 0.012F, 1.05F), 0.028F, c.apple * 0.9F);
    add_market_fruit(desc, QVector3D(-0.80F, top + 0.012F, 1.10F), 0.027F, c.pear);
    add_market_fruit(desc, QVector3D(-0.80F, top + 0.040F, 1.06F), 0.026F, c.apple);
  }
  add_market_amphora(
      desc, QVector3D(-0.98F, k_walk_y, -1.04F), c.terracotta, c.cloth_gold);
  add_market_amphora(desc,
                     QVector3D(-0.84F, k_walk_y, -1.08F),
                     c.terracotta_dark * 1.1F,
                     c.terracotta);
  add_market_sack(
      desc, QVector3D(-1.10F, k_walk_y, -0.92F), 0.066F, 0.18F, goods, c.wheat, false);

  add_market_sack(
      desc, QVector3D(0.12F, k_walk_y, 1.02F), 0.072F, 0.19F, goods, c.wheat, false);
  add_market_sack(
      desc, QVector3D(0.29F, k_walk_y, 1.05F), 0.068F, 0.17F, goods, c.lentil, true);
  add_market_crate(desc,
                   QVector3D(0.22F, k_walk_y + 0.070F, 0.86F),
                   QVector3D(0.080F, 0.070F, 0.070F),
                   goods,
                   true);
  add_market_basket(
      desc, QVector3D(0.22F, k_walk_y + 0.148F, 0.86F), 0.060F, goods, c.pear, c.apple);
}

auto build_marketplace_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  RomanMarketPalette const c;
  MarketGoodsPalette const goods = goods_palette(c);
  float hm = 1.0F;
  if (state == BuildingState::Damaged) {
    hm = 0.75F;
  } else if (state == BuildingState::Destroyed) {
    hm = 0.35F;
  }
  BuildingArchetypeDesc desc("roman_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.40F, 0.04F, 1.40F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.32F, 0.02F, 1.32F), c.limestone_shade);
  desc.add_box(QVector3D(0.0F, (0.12F + k_slab_top) * 0.5F, 0.0F),
               QVector3D(1.24F, (k_slab_top - 0.12F) * 0.5F, 1.24F),
               c.mortar);
  add_market_paving(
      desc,
      MarketPaving{.min_x = -1.21F,
                   .max_x = 0.52F,
                   .min_z = -1.21F,
                   .max_z = 1.21F,
                   .cell = 0.30F,
                   .gap = 0.010F,
                   .base_y = k_slab_top,
                   .min_rise = 0.008F,
                   .max_rise = 0.012F,
                   .tones = {c.limestone, c.limestone_shade, c.marble * 0.93F},
                   .seed = 181});
  desc.add_box(QVector3D(0.875F, (k_slab_top + k_walk_y) * 0.5F, 0.0F),
               QVector3D(0.34F, (k_walk_y - k_slab_top) * 0.5F, 1.212F),
               c.signinum);

  float const wall_h = 0.62F * hm;
  float const wall_top = 0.16F + wall_h;
  desc.add_box(QVector3D(1.16F, (k_wall_base + wall_top) * 0.5F, 0.0F),
               QVector3D(0.06F, (wall_top - k_wall_base) * 0.5F, 1.22F),
               c.limestone);
  desc.add_box(QVector3D(1.174F, k_wall_base + 0.035F, 0.0F),
               QVector3D(0.054F, 0.035F, 1.228F),
               c.limestone_dark);
  for (float y = 0.30F; y < wall_top - 0.03F; y += 0.11F) {
    desc.add_box(QVector3D(1.2235F, y, 0.0F),
                 QVector3D(0.0035F, 0.0035F, 1.205F),
                 c.limestone_dark * 0.92F,
                 k_building_state_mask_intact);
  }
  add_shop_bays(desc, wall_top, c, goods);
  desc.add_box(QVector3D(0.58F, wall_top + 0.03F, 0.0F),
               QVector3D(0.05F, 0.035F, 1.22F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.522F, wall_top + 0.005F, 0.0F),
               QVector3D(0.006F, 0.012F, 1.20F),
               c.blue_accent,
               BuildingStateMask::Normal);
  add_lean_to_roof(desc, 0.50F, 1.24F, wall_top + 0.08F, wall_top + 0.30F, 1.22F, c);

  add_market_stall(desc,
                   MarketStall{.cx = -0.40F,
                               .cz = 0.80F,
                               .side = 1.0F,
                               .ground_y = 0.152F,
                               .counter_top = 0.36F,
                               .back_post_top = 0.875F,
                               .front_post_top = 0.755F,
                               .board = c.cedar_light,
                               .apron = c.cedar,
                               .apron_panel = c.pompeian_red},
                   goods);
  add_market_stall(desc,
                   MarketStall{.cx = -0.40F,
                               .cz = -0.80F,
                               .side = -1.0F,
                               .ground_y = 0.152F,
                               .counter_top = 0.36F,
                               .back_post_top = 0.875F,
                               .front_post_top = 0.755F,
                               .board = c.cedar_light,
                               .apron = c.cedar,
                               .apron_panel = c.blue_accent},
                   goods);
  add_stall_goods(desc, c, goods);

  add_forum_fountain(desc, hm, c);
  add_gateway(desc, hm, c);
  add_mensa_ponderaria(desc, c, goods);
  add_yard_goods(desc, c, goods);

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.16F, 0.0F, 1.16F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.cedar_dark,
                                 .ground_y = 0.16F,
                                 .scale = 1.0F,
                                 .seed = 181});

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
    TorchMount{.at = QVector3D(-1.18F, 0.66F, 0.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-1.18F, 0.66F, -0.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.60F, 0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.60F, -0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
}};

const std::array<MarketAwning, 2> k_awnings{{
    MarketAwning{.back = QVector3D(-0.40F, 0.875F, 1.03F),
                 .front = QVector3D(-0.40F, 0.755F, 0.57F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.60F, 0.10F, 0.07F),
                 .stripe_b = QVector3D(0.86F, 0.80F, 0.66F),
                 .stripes = 6},
    MarketAwning{.back = QVector3D(-0.40F, 0.875F, -1.03F),
                 .front = QVector3D(-0.40F, 0.755F, -0.57F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.22F, 0.30F, 0.52F),
                 .stripe_b = QVector3D(0.86F, 0.80F, 0.66F),
                 .stripes = 6},
}};

const std::array<MarketHanging, 10> k_hangings{{
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.98F),
                  .length = 0.14F,
                  .radius = 0.022F,
                  .color = QVector3D(0.86F, 0.82F, 0.70F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.62F),
                  .length = 0.16F,
                  .radius = 0.024F,
                  .color = QVector3D(0.46F, 0.18F, 0.12F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.20F),
                  .length = 0.12F,
                  .radius = 0.030F,
                  .color = QVector3D(0.30F, 0.36F, 0.12F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, 0.22F),
                  .length = 0.15F,
                  .radius = 0.022F,
                  .color = QVector3D(0.84F, 0.64F, 0.20F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.80F, 0.96F),
                  .length = 0.10F,
                  .radius = 0.020F,
                  .color = QVector3D(0.86F, 0.82F, 0.70F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.80F, -0.96F),
                  .length = 0.10F,
                  .radius = 0.024F,
                  .color = QVector3D(0.46F, 0.18F, 0.12F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.728F, 0.585F),
                  .length = 0.13F,
                  .radius = 0.019F,
                  .color = QVector3D(0.88F, 0.84F, 0.72F),
                  .beads = 4},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.728F, 0.585F),
                  .length = 0.12F,
                  .radius = 0.022F,
                  .color = QVector3D(0.58F, 0.30F, 0.12F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.728F, -0.585F),
                  .length = 0.15F,
                  .radius = 0.021F,
                  .color = QVector3D(0.50F, 0.20F, 0.14F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.728F, -0.585F),
                  .length = 0.11F,
                  .radius = 0.024F,
                  .color = QVector3D(0.72F, 0.50F, 0.22F),
                  .beads = 2},
}};

const std::array<QVector3D, 5> k_buyer_a{QVector3D(-1.80F, 0.0F, -0.20F),
                                         QVector3D(-1.34F, 0.0F, -0.18F),
                                         QVector3D(-1.28F, 0.16F, -0.18F),
                                         QVector3D(-0.78F, 0.16F, -0.40F),
                                         QVector3D(-0.40F, 0.16F, -0.44F)};
const std::array<QVector3D, 5> k_buyer_b{QVector3D(-1.80F, 0.0F, 0.22F),
                                         QVector3D(-1.34F, 0.0F, 0.20F),
                                         QVector3D(-1.28F, 0.16F, 0.22F),
                                         QVector3D(0.06F, 0.16F, 0.34F),
                                         QVector3D(0.42F, 0.16F, 0.10F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(-0.90F, 0.16F, 0.36F),
                                              QVector3D(0.05F, 0.16F, 0.32F),
                                              QVector3D(0.08F, 0.16F, -0.32F),
                                              QVector3D(-0.90F, 0.16F, -0.34F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(-1.80F, 0.0F, 0.40F),
                                              QVector3D(-1.34F, 0.0F, 0.40F),
                                              QVector3D(-1.28F, 0.16F, 0.40F),
                                              QVector3D(0.05F, 0.16F, 0.40F),
                                              QVector3D(0.46F, 0.16F, 0.62F)};
const std::array<QVector3D, 1> k_seller_a{QVector3D(-0.40F, 0.16F, 1.03F)};
const std::array<QVector3D, 1> k_seller_b{QVector3D(-0.40F, 0.16F, -1.03F)};
const std::array<QVector3D, 1> k_shopkeeper{QVector3D(0.86F, 0.16F, 0.02F)};
const std::array<QVector3D, 1> k_fountain_rest{QVector3D(-0.30F, 0.16F, 0.30F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{
        .min_x = -1.24F, .max_x = 1.24F, .min_z = -1.24F, .max_z = 1.24F, .top = 0.16F},
}};
const std::array<AmbientPerson, 8> k_people{{
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_a,
                  .facing = QVector3D(-0.40F, 0.4F, -0.80F),
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
                  .facing = QVector3D(0.67F, 0.35F, 0.79F)},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_fountain_rest,
                  .facing = QVector3D(-0.30F, 0.3F, 0.0F)},
}};

} // namespace

auto build_marketplace_desc(BuildingState state) -> BuildingArchetypeDesc {
  return build_marketplace_desc_impl(state);
}

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_marketplace_renderer_variant(
      registry,
      MarketplaceRendererConfig{.nation_slug = "roman",
                                .archetype = &marketplace_archetype,
                                .palette_slots = &marketplace_palette_slots,
                                .selection = BuildingSelectionStyle{1.8F, 1.8F},
                                .torches = k_torches,
                                .people = k_people,
                                .walk_surfaces = k_walk_surfaces,
                                .awnings = k_awnings,
                                .hangings = k_hangings});
}

} // namespace Render::GL::Roman
