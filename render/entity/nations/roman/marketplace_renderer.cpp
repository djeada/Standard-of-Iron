#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
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
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_light = BuildingPalette::k_cedar_light;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D cloth_red_faded{0.68F, 0.16F, 0.10F};
  QVector3D cloth_gold{0.72F, 0.52F, 0.16F};
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D iron{0.12F, 0.13F, 0.13F};
  QVector3D olive{0.25F, 0.31F, 0.10F};
  QVector3D grape{0.24F, 0.08F, 0.18F};
  QVector3D ochre{0.66F, 0.38F, 0.09F};
  QVector3D gold = BuildingPalette::k_gold;
};

constexpr std::uint8_t k_marketplace_team_slot = 0;

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

void add_amphora(BuildingArchetypeDesc& desc,
                 const QVector3D& base,
                 const QVector3D& clay,
                 const QVector3D& painted_band) {
  QVector3D const dark_clay = clay * 0.58F;
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.035F, 0.0F),
                    0.035F,
                    dark_clay,
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.035F, 0.0F),
                    base + QVector3D(0.0F, 0.145F, 0.0F),
                    0.070F,
                    clay,
                    BuildingStateMask::Normal);
  desc.add_cone(base + QVector3D(0.0F, 0.135F, 0.0F),
                base + QVector3D(0.0F, 0.205F, 0.0F),
                0.072F,
                clay,
                BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.19F, 0.0F),
                    base + QVector3D(0.0F, 0.265F, 0.0F),
                    0.025F,
                    dark_clay,
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.245F, 0.0F),
                    base + QVector3D(0.0F, 0.265F, 0.0F),
                    0.037F,
                    painted_band,
                    BuildingStateMask::Normal);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.027F, 0.205F, 0.0F),
                      base + QVector3D(side * 0.073F, 0.145F, 0.0F),
                      0.010F,
                      dark_clay,
                      BuildingStateMask::Normal);
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
                    BuildingStateMask::Normal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.085F, 0.0F),
                    base + QVector3D(0.0F, 0.105F, 0.0F),
                    0.115F,
                    c.cedar_dark,
                    BuildingStateMask::Normal);
  int index = 0;
  for (float const x : {-0.055F, 0.0F, 0.055F}) {
    for (float const z : {-0.040F, 0.040F}) {
      QVector3D const fruit_base = base + QVector3D(x, 0.10F, z);
      QVector3D const fruit_tip =
          fruit_base +
          QVector3D(0.0F, 0.055F + 0.008F * static_cast<float>(index), 0.0F);
      QVector3D const& fruit_colour = (index % 2 == 0) ? produce_a : produce_b;
      desc.add_cone(
          fruit_base, fruit_tip, 0.035F, fruit_colour, BuildingStateMask::Normal);
      ++index;
    }
  }
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
    BuildingPartMaterial clay(desc, k_building_material_stone);
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
      desc.add_cylinder(QVector3D(low_x - 0.02F, low_y + 0.024F, z),
                        QVector3D(high_x + 0.01F, high_y + 0.030F, z),
                        0.022F,
                        (i % 2 == 0) ? c.terracotta_dark : c.terracotta,
                        k_building_state_mask_intact);
    }
  }
  BuildingPartMaterial wood(desc, k_building_material_wood);
  for (float z = -half_z + 0.10F; z <= half_z - 0.05F; z += 0.34F) {
    desc.add_box(QVector3D(low_x + 0.02F, low_y - 0.030F, z),
                 QVector3D(0.035F, 0.018F, 0.018F),
                 c.cedar_dark,
                 k_building_state_mask_intact);
  }
}

void add_stall(BuildingArchetypeDesc& desc,
               float cx,
               float cz,
               float side,
               const QVector3D& produce_a,
               const QVector3D& produce_b,
               const RomanMarketPalette& c) {
  BuildingPartMaterial wood(desc, k_building_material_wood);
  desc.add_box(QVector3D(cx, 0.352F, cz),
               QVector3D(0.25F, 0.012F, 0.13F),
               c.cedar,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_box(QVector3D(cx, 0.330F, cz + side * 0.125F),
               QVector3D(0.25F, 0.018F, 0.006F),
               c.cedar_dark,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);
  for (float const lx : {-0.21F, 0.21F}) {
    for (float const lz : {-0.10F, 0.10F}) {
      desc.add_cylinder(QVector3D(cx + lx, 0.16F, cz + lz),
                        QVector3D(cx + lx, 0.342F, cz + lz),
                        0.014F,
                        c.cedar_dark,
                        k_building_state_mask_intact);
    }
  }
  for (float const lx : {-0.26F, 0.26F}) {
    desc.add_cylinder(QVector3D(cx + lx, 0.16F, cz + side * 0.22F),
                      QVector3D(cx + lx, 0.87F, cz + side * 0.22F),
                      0.016F,
                      c.cedar_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(cx + lx, 0.16F, cz - side * 0.22F),
                      QVector3D(cx + lx, 0.75F, cz - side * 0.22F),
                      0.016F,
                      c.cedar_dark,
                      k_building_state_mask_intact);
  }
  add_produce_basket(desc, QVector3D(cx - 0.12F, 0.364F, cz), produce_a, produce_b, c);
  add_produce_basket(desc, QVector3D(cx + 0.12F, 0.364F, cz), produce_b, c.olive, c);
}

auto build_marketplace_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  RomanMarketPalette const c;
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
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.24F, 0.02F, 1.24F), c.limestone);
  for (float g = -0.90F; g <= 0.91F; g += 0.30F) {
    desc.add_box(QVector3D(g, 0.1615F, -0.02F),
                 QVector3D(0.005F, 0.0015F, 1.18F),
                 c.mortar,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.33F, 0.1615F, g),
                 QVector3D(0.86F, 0.0015F, 0.005F),
                 c.mortar,
                 k_building_state_mask_intact);
  }

  float const wall_h = 0.62F * hm;
  float const wall_top = 0.16F + wall_h;
  desc.add_box(QVector3D(1.16F, 0.16F + wall_h * 0.5F, 0.0F),
               QVector3D(0.06F, wall_h * 0.5F, 1.22F),
               c.limestone);
  for (float const z : {-1.17F, -0.40F, 0.40F, 1.17F}) {
    desc.add_box(QVector3D(0.85F, 0.16F + wall_h * 0.5F - 0.004F, z),
                 QVector3D(0.27F, wall_h * 0.5F - 0.004F, 0.04F),
                 c.limestone_shade);
    desc.add_box(QVector3D(0.585F, 0.16F + wall_h * 0.5F, z),
                 QVector3D(0.012F, wall_h * 0.5F, 0.052F),
                 c.marble,
                 k_building_state_mask_intact);
  }
  for (float const zc : {-0.79F, 0.0F, 0.79F}) {
    desc.add_box(QVector3D(1.097F, 0.16F + wall_h * 0.5F, zc),
                 QVector3D(0.004F, wall_h * 0.5F - 0.01F, 0.34F),
                 c.limestone_dark * 0.72F,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.26F, zc),
                 QVector3D(0.07F, 0.10F, 0.30F),
                 c.limestone_shade,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.67F, 0.366F, zc),
                 QVector3D(0.09F, 0.008F, 0.32F),
                 c.marble,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(0.58F, wall_top + 0.03F, 0.0F),
               QVector3D(0.05F, 0.035F, 1.22F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.522F, wall_top + 0.005F, 0.0F),
               QVector3D(0.006F, 0.012F, 1.20F),
               c.blue_accent,
               BuildingStateMask::Normal);
  add_lean_to_roof(desc, 0.50F, 1.24F, wall_top + 0.08F, wall_top + 0.30F, 1.22F, c);

  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    for (float const z : {-0.98F, -0.82F, -0.66F}) {
      add_amphora(desc, QVector3D(0.95F, 0.16F, z), c.terracotta, c.terracotta_dark);
    }
    add_amphora(
        desc, QVector3D(0.67F, 0.374F, -0.94F), c.terracotta_dark, c.cloth_gold);
  }
  add_produce_basket(desc, QVector3D(0.67F, 0.374F, -0.17F), c.ochre, c.terracotta, c);
  add_produce_basket(desc, QVector3D(0.67F, 0.374F, 0.17F), c.olive, c.grape, c);
  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    int bolt = 0;
    for (float const z : {0.60F, 0.74F, 0.88F}) {
      for (float const y : {0.40F, 0.455F}) {
        QVector3D const colour = (bolt % 3 == 0)   ? c.cloth_red
                                 : (bolt % 3 == 1) ? c.cloth_gold
                                                   : c.blue_accent;
        desc.add_cylinder(QVector3D(0.61F, y, z),
                          QVector3D(0.73F, y, z),
                          0.028F,
                          colour,
                          BuildingStateMask::Normal);
        ++bolt;
      }
    }
  }
  {
    BuildingPartMaterial metal(desc, k_building_material_metal);
    float const top = 0.374F;
    desc.add_cylinder(QVector3D(0.67F, top, 0.0F),
                      QVector3D(0.67F, top + 0.22F, 0.0F),
                      0.010F,
                      c.bronze,
                      BuildingStateMask::Normal);
    desc.add_cylinder(QVector3D(0.67F, top + 0.20F, -0.12F),
                      QVector3D(0.67F, top + 0.20F, 0.12F),
                      0.008F,
                      c.bronze,
                      BuildingStateMask::Normal);
    for (float const z : {-0.11F, 0.11F}) {
      desc.add_cylinder(QVector3D(0.67F, top + 0.20F, z),
                        QVector3D(0.67F, top + 0.11F, z),
                        0.003F,
                        c.iron,
                        BuildingStateMask::Normal);
      desc.add_cylinder(QVector3D(0.67F, top + 0.10F, z),
                        QVector3D(0.67F, top + 0.11F, z),
                        0.045F,
                        c.bronze,
                        BuildingStateMask::Normal);
    }
  }

  add_stall(desc, -0.40F, 0.80F, 1.0F, c.ochre, c.terracotta, c);
  add_stall(desc, -0.40F, -0.80F, -1.0F, c.grape, c.olive, c);

  desc.add_cylinder(QVector3D(-0.30F, 0.16F, 0.0F),
                    QVector3D(-0.30F, 0.165F, 0.0F),
                    0.30F,
                    c.limestone_dark,
                    BuildingStateMask::All);
  desc.add_cylinder(QVector3D(-0.30F, 0.16F, 0.0F),
                    QVector3D(-0.30F, 0.26F * std::max(hm, 0.6F), 0.0F),
                    0.20F,
                    c.marble);
  desc.add_cylinder(QVector3D(-0.30F, 0.16F, 0.0F),
                    QVector3D(-0.30F, 0.255F, 0.0F),
                    0.17F,
                    QVector3D(0.22F, 0.38F, 0.44F),
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(-0.30F, 0.16F, 0.0F),
                    QVector3D(-0.30F, 0.46F, 0.0F),
                    0.035F,
                    c.marble,
                    k_building_state_mask_intact);
  desc.add_cone(QVector3D(-0.30F, 0.44F, 0.0F),
                QVector3D(-0.30F, 0.40F, 0.0F),
                0.10F,
                c.marble,
                k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(-0.30F, 0.44F, 0.0F),
                    QVector3D(-0.30F, 0.455F, 0.0F),
                    0.09F,
                    QVector3D(0.30F, 0.48F, 0.54F),
                    BuildingStateMask::Normal);

  float const col_h = 0.84F * hm;
  for (float const z : {-0.52F, 0.52F}) {
    desc.add_box(QVector3D(-1.12F, 0.19F, z),
                 QVector3D(0.075F, 0.03F, 0.075F),
                 c.marble,
                 BuildingStateMask::All);
    desc.add_cylinder(QVector3D(-1.12F, 0.16F, z),
                      QVector3D(-1.12F, 0.16F + col_h, z),
                      0.052F,
                      c.limestone_shade);
    desc.add_box(QVector3D(-1.12F, 0.16F + col_h + 0.03F, z),
                 QVector3D(0.075F, 0.03F, 0.075F),
                 c.marble,
                 k_building_state_mask_intact);
  }
  float const gate_y = 0.16F + col_h + 0.10F;
  desc.add_box(QVector3D(-1.12F, gate_y, 0.0F),
               QVector3D(0.07F, 0.065F, 0.64F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.12F, gate_y + 0.085F, 0.0F),
               QVector3D(0.085F, 0.02F, 0.68F),
               c.limestone_shade,
               k_building_state_mask_intact);
  add_roman_aquila_relief(desc,
                          QVector3D(-1.15F, gate_y + 0.21F, 0.0F),
                          BuildingFacadePlane::ZY,
                          0.30F,
                          c.gold,
                          c.terracotta_dark);
  {
    BuildingPartMaterial cloth(desc, k_building_material_cloth);
    desc.add_palette_box(QVector3D(-1.10F, gate_y - 0.15F, 0.0F),
                         QVector3D(0.006F, 0.085F, 0.075F),
                         k_marketplace_team_slot,
                         BuildingStateMask::Normal);
  }

  {
    BuildingPartMaterial wood(desc, k_building_material_wood);
    desc.add_box(QVector3D(-0.98F, 0.24F, 1.02F),
                 QVector3D(0.09F, 0.08F, 0.09F),
                 c.cedar_dark,
                 BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-0.80F, 0.22F, 1.06F),
                 QVector3D(0.07F, 0.06F, 0.07F),
                 c.cedar,
                 BuildingStateMask::Normal);
    desc.add_box(QVector3D(-0.97F, 0.37F, 1.02F),
                 QVector3D(0.07F, 0.05F, 0.07F),
                 c.cedar_light,
                 BuildingStateMask::Normal);
  }
  {
    BuildingPartMaterial clay(desc, k_building_material_stone);
    add_amphora(desc, QVector3D(-0.98F, 0.16F, -1.04F), c.terracotta, c.cloth_gold);
    add_amphora(
        desc, QVector3D(-0.84F, 0.16F, -1.08F), c.terracotta_dark, c.terracotta);
  }

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

const std::array<MarketHanging, 6> k_hangings{{
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
