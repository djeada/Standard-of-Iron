#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/tenement_builder.h"

namespace Render::GL::Roman::TenementDetail {

using namespace TenementLot;

void TenementBuilder::add_jar(const QVector3D& foot,
                              float scale,
                              const QVector3D& clay) {
  const auto at = [&](float y) {
    return foot + QVector3D(0.0F, y * scale, 0.0F);
  };
  const float r = 0.058F * scale;
  m_desc.add_cone(at(0.045F), at(-0.004F), r * 0.9F, clay);
  m_desc.add_cylinder(at(0.043F), at(0.165F), r, clay);
  m_desc.add_cone(at(0.163F), at(0.235F), r, clay);
  m_desc.add_cylinder(at(0.205F), at(0.262F), r * 0.38F, clay * 0.9F);
}

void TenementBuilder::add_pot(const QVector3D& foot, int salt) {
  m_desc.add_cylinder(foot,
                      foot + QVector3D(0.0F, 0.07F, 0.0F),
                      0.045F,
                      weathered(BuildingPalette::k_terracotta, salt, 0.08F));
  BuildingPartMaterial const plant(m_desc, k_building_material_plant);
  m_desc.add_cone(foot + QVector3D(0.0F, 0.06F, 0.0F),
                  foot + QVector3D(0.0F, 0.24F + (0.06F * roll(salt)), 0.0F),
                  0.062F,
                  (salt % 2) == 0 ? k_leaf : k_leaf_light);
}

void TenementBuilder::add_window_dressing(Side side,
                                          const Opening& opening,
                                          int salt,
                                          bool shutters) {
  if (!intact() && (opening.x0 + opening.x1) * 0.5F > k_break_x &&
      cap((opening.x0 + opening.x1) * 0.5F) < opening.y1 + 0.06F) {
    return;
  }
  trim(side,
       opening.x0 - 0.035F,
       opening.x1 + 0.035F,
       opening.y0 - 0.034F,
       opening.y0,
       0.036F,
       0.08F,
       BuildingPalette::k_limestone);
  trim(side,
       opening.x0 - 0.02F,
       opening.x1 + 0.02F,
       opening.y1,
       opening.y1 + 0.03F,
       0.014F,
       0.05F,
       BuildingPalette::k_limestone_shade);
  if (!shutters) {
    return;
  }
  const float pick = roll(salt);
  const float leaf = (opening.x1 - opening.x0) * 0.5F;
  const QVector3D timber = weathered(BuildingPalette::k_cedar_light, salt + 3, 0.10F) *
                           (intact() ? 1.0F : 0.8F);
  if (pick < 0.22F) {

    trim(side, opening.x0, opening.x1, opening.y0, opening.y1, -0.026F, 0.016F, timber);
    trim(side,
         ((opening.x0 + opening.x1) * 0.5F) - 0.006F,
         ((opening.x0 + opening.x1) * 0.5F) + 0.006F,
         opening.y0,
         opening.y1,
         -0.020F,
         0.012F,
         BuildingPalette::k_cedar_dark);
    return;
  }
  if (pick < 0.82F) {
    trim(side,
         opening.x0 - leaf,
         opening.x0 - 0.004F,
         opening.y0,
         opening.y1,
         0.014F,
         0.014F,
         timber);
  }
  if (pick > 0.36F) {
    trim(side,
         opening.x1 + 0.004F,
         opening.x1 + leaf,
         opening.y0,
         opening.y1,
         0.014F,
         0.014F,
         timber * 0.94F);
  }
}

void TenementBuilder::add_door_leaf(Side side,
                                    const Opening& opening,
                                    const QVector3D& timber) {
  trim(side, opening.x0, opening.x1, opening.y0, opening.y1, -0.044F, 0.020F, timber);
  trim(side,
       ((opening.x0 + opening.x1) * 0.5F) - 0.007F,
       ((opening.x0 + opening.x1) * 0.5F) + 0.007F,
       opening.y0,
       opening.y1,
       -0.036F,
       0.014F,
       timber * 0.72F);
  trim(side,
       opening.x0,
       opening.x1,
       opening.y0 + ((opening.y1 - opening.y0) * 0.56F),
       opening.y0 + ((opening.y1 - opening.y0) * 0.60F),
       -0.036F,
       0.014F,
       timber * 0.72F);
}

void TenementBuilder::add_shop_front() {
  const float p = k_pavement_top;
  const float shop_back = k_front - k_shop_depth;
  {
    BuildingPartLabel const label(m_desc, "shop_front");
    box(-0.98F,
        0.26F,
        p,
        p + 0.018F,
        k_front - k_wall,
        k_front + 0.024F,
        BuildingPalette::k_limestone_dark);
    trim(Side::Front,
         -1.04F,
         0.32F,
         p + 0.78F,
         p + 0.845F,
         0.018F,
         0.09F,
         BuildingPalette::k_cedar_dark);

    const float counter_top = p + 0.31F;
    slot_box(-0.96F,
             -0.16F,
             p + 0.018F,
             counter_top,
             k_front - 0.27F,
             k_front - 0.035F,
             k_tenement_dado_slot);
    slot_box(-0.30F,
             -0.16F,
             p + 0.018F,
             counter_top,
             shop_back + 0.14F,
             k_front - 0.27F,
             k_tenement_dado_slot);
    box(-0.985F,
        -0.135F,
        counter_top,
        counter_top + 0.022F,
        k_front - 0.295F,
        k_front - 0.012F,
        BuildingPalette::k_marble);
    for (int jar = 0; jar < 3; ++jar) {
      const QVector3D mouth(-0.82F + (0.26F * static_cast<float>(jar)),
                            counter_top + 0.022F,
                            k_front - 0.15F);
      m_desc.add_cylinder(mouth - QVector3D(0.0F, 0.01F, 0.0F),
                          mouth + QVector3D(0.0F, 0.006F, 0.0F),
                          0.062F,
                          weathered(BuildingPalette::k_terracotta, jar + 11, 0.08F));
      m_desc.add_cylinder(mouth, mouth + QVector3D(0.0F, 0.009F, 0.0F), 0.044F, k_void);
    }

    for (int shelf = 0; shelf < 2; ++shelf) {
      const float y = p + 0.42F + (0.20F * static_cast<float>(shelf));
      box(-0.92F,
          0.20F,
          y,
          y + 0.018F,
          shop_back + 0.012F,
          shop_back + 0.10F,
          BuildingPalette::k_cedar);
      for (int ware = 0; ware < 5; ++ware) {
        const float x = -0.80F + (0.22F * static_cast<float>(ware));
        const QVector3D foot(x, y + 0.018F, shop_back + 0.055F);
        m_desc.add_cylinder(
            foot,
            foot + QVector3D(0.0F, 0.07F + (0.04F * roll(ware + (shelf * 9))), 0.0F),
            0.030F,
            weathered(((ware + shelf) % 3) == 0 ? BuildingPalette::k_bronze
                                                : BuildingPalette::k_terracotta_light,
                      ware + 31,
                      0.10F));
      }
    }

    for (int board = 0; board < 3; ++board) {
      const float z = k_front - 0.070F - (0.022F * static_cast<float>(board));
      box(0.10F,
          0.25F,
          p + 0.018F,
          p + 0.74F - (0.03F * static_cast<float>(board)),
          z - 0.009F,
          z + 0.009F,
          weathered(BuildingPalette::k_cedar_light, board + 61, 0.10F));
    }
  }

  add_door_leaf(
      Side::Front, Opening{0.52F, 0.94F, p, p + 0.80F}, BuildingPalette::k_cedar);
  trim(Side::Front,
       0.48F,
       0.98F,
       p + 0.80F,
       p + 0.84F,
       0.016F,
       0.06F,
       BuildingPalette::k_limestone_shade);
  box(0.50F,
      0.96F,
      p,
      p + 0.022F,
      k_front - 0.03F,
      k_front + 0.07F,
      BuildingPalette::k_limestone);

  {
    BuildingPartLabel const label(m_desc, "pavement_wares");
    add_jar(QVector3D(0.39F, p, k_front + 0.11F), 1.0F, BuildingPalette::k_terracotta);
    add_jar(QVector3D(0.30F, p, k_front + 0.20F),
            0.82F,
            BuildingPalette::k_terracotta_light);
  }

  if (!intact()) {
    return;
  }

  {
    BuildingPartLabel const label(m_desc, "awning");

    constexpr std::array<float, k_tenement_variant_count> k_lift{
        0.0F, 0.08F, -0.04F, 0.12F, 0.03F, -0.02F, 0.10F, 0.05F};
    constexpr std::array<float, k_tenement_variant_count> k_reach{
        0.40F, 0.54F, 0.32F, 0.48F, 0.44F, 0.30F, 0.52F, 0.36F};
    const auto look = static_cast<std::size_t>(m_variant % k_tenement_variant_count);
    const float wall_y = p + 0.95F + k_lift[look];
    const float edge_y = p + 0.75F + (k_lift[look] * 0.8F);
    const float wall_z = k_front + 0.012F;
    const float edge_z = k_front + k_reach[look];
    const float run = edge_z - wall_z;
    const float drop = wall_y - edge_y;
    const float length = std::sqrt((run * run) + (drop * drop));
    const float angle = std::atan2(drop, run) * 180.0F / k_pi;
    {
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_desc.add_palette_rotated_box(
          QVector3D(-0.36F, (wall_y + edge_y) * 0.5F, (wall_z + edge_z) * 0.5F),
          QVector3D(0.70F, 0.006F, length * 0.5F),
          QVector3D(angle, 0.0F, 0.0F),
          k_tenement_cloth_slot);
      for (int stripe = 0; stripe < 4; ++stripe) {
        m_desc.add_rotated_box(QVector3D(-0.885F + (0.35F * static_cast<float>(stripe)),
                                         ((wall_y + edge_y) * 0.5F) + 0.0075F,
                                         (wall_z + edge_z) * 0.5F),
                               QVector3D(0.058F, 0.003F, length * 0.5F),
                               QVector3D(angle, 0.0F, 0.0F),
                               k_linen);
      }
      slot_box(-1.06F,
               0.34F,
               edge_y - 0.062F,
               edge_y - 0.004F,
               edge_z - 0.006F,
               edge_z + 0.006F,
               k_tenement_cloth_slot);
    }
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    for (const float x : {-1.02F, 0.30F}) {
      m_desc.add_cylinder(QVector3D(x, p + 0.60F, k_front),
                          QVector3D(x, edge_y - 0.004F, edge_z - 0.01F),
                          0.012F,
                          BuildingPalette::k_cedar_dark);
    }
    m_desc.add_cylinder(QVector3D(-1.05F, edge_y - 0.004F, edge_z - 0.01F),
                        QVector3D(0.33F, edge_y - 0.004F, edge_z - 0.01F),
                        0.010F,
                        BuildingPalette::k_cedar_dark);
  }

  {
    BuildingPartLabel const label(m_desc, "shop_sign");
    {
      BuildingPartMaterial const iron(m_desc, k_building_material_metal);
      m_desc.add_cylinder(QVector3D(0.39F, p + 0.99F, k_front),
                          QVector3D(0.39F, p + 0.99F, k_front + 0.20F),
                          0.008F,
                          k_iron);
    }
    m_desc.add_palette_box(QVector3D(0.39F, p + 0.915F, k_front + 0.13F),
                           QVector3D(0.012F, 0.058F, 0.066F),
                           k_tenement_team_slot);
  }
}

void TenementBuilder::add_door_front() {
  const float p = k_pavement_top;
  BuildingPartLabel const label(m_desc, "doorway");
  add_door_leaf(
      Side::Front, Opening{-0.24F, 0.24F, p, p + 0.86F}, BuildingPalette::k_cedar);
  for (const float side : {-1.0F, 1.0F}) {
    trim(Side::Front,
         side > 0.0F ? 0.24F : -0.33F,
         side > 0.0F ? 0.33F : -0.24F,
         p,
         p + 0.90F,
         0.030F,
         0.07F,
         BuildingPalette::k_limestone);
    trim(Side::Front,
         side > 0.0F ? 0.225F : -0.345F,
         side > 0.0F ? 0.345F : -0.225F,
         p,
         p + 0.07F,
         0.042F,
         0.08F,
         BuildingPalette::k_limestone_shade);
  }
  trim(Side::Front,
       -0.37F,
       0.37F,
       p + 0.90F,
       p + 0.955F,
       0.044F,
       0.09F,
       BuildingPalette::k_marble);
  if (intact()) {
    for (const float side : {-1.0F, 1.0F}) {
      m_desc.add_rotated_box(QVector3D(side * 0.18F, p + 0.995F, k_front + 0.012F),
                             QVector3D(0.20F, 0.016F, 0.034F),
                             QVector3D(0.0F, 0.0F, -side * 13.0F),
                             BuildingPalette::k_limestone);
    }
    m_desc.add_palette_box(QVector3D(0.0F, p + 0.975F, k_front + 0.002F),
                           QVector3D(0.10F, 0.014F, 0.014F),
                           k_tenement_team_slot);
  }
  box(-0.40F,
      0.40F,
      p,
      p + 0.03F,
      k_front,
      k_front + 0.15F,
      BuildingPalette::k_limestone);
  box(-0.34F,
      0.34F,
      p + 0.03F,
      p + 0.055F,
      k_front - 0.03F,
      k_front + 0.075F,
      BuildingPalette::k_marble_shade);

  for (const float x : {-0.72F, 0.72F}) {
    const Opening window{x - 0.14F, x + 0.14F, p + 0.56F, p + 0.82F};
    add_window_dressing(Side::Front, window, static_cast<int>(x * 10.0F) + 80, false);
    BuildingPartMaterial const iron(m_desc, k_building_material_metal);
    for (const float bar : {-0.07F, 0.0F, 0.07F}) {
      m_desc.add_cylinder(QVector3D(x + bar, window.y0, k_front - 0.03F),
                          QVector3D(x + bar, window.y1, k_front - 0.03F),
                          0.007F,
                          k_iron);
    }
  }

  box(0.44F,
      0.98F,
      p,
      p + 0.105F,
      k_front,
      k_front + 0.11F,
      BuildingPalette::k_limestone_shade);
  box(0.42F,
      1.00F,
      p + 0.105F,
      p + 0.128F,
      k_front,
      k_front + 0.125F,
      BuildingPalette::k_limestone);
  add_pot(QVector3D(-0.52F, p, k_front + 0.09F), 3);
  add_pot(QVector3D(-0.94F, p, k_front + 0.09F), 4);
}

void TenementBuilder::add_passage_dressing() {
  const float p = k_pavement_top;
  BuildingPartLabel const label(m_desc, "passage_dressing");
  const Opening hatch{0.56F, 0.96F, p + 0.34F, p + 0.76F};
  trim(Side::Front,
       hatch.x0 - 0.04F,
       hatch.x1 + 0.04F,
       hatch.y0 - 0.03F,
       hatch.y0,
       0.07F,
       0.15F,
       BuildingPalette::k_marble);
  trim(Side::Front,
       hatch.x0 - 0.03F,
       hatch.x1 + 0.03F,
       hatch.y1,
       hatch.y1 + 0.045F,
       0.016F,
       0.08F,
       BuildingPalette::k_cedar_dark);
  for (int ware = 0; ware < 3; ++ware) {
    const QVector3D foot(
        0.64F + (0.12F * static_cast<float>(ware)), hatch.y0, k_front + 0.02F);
    m_desc.add_cylinder(
        foot,
        foot + QVector3D(0.0F, 0.06F, 0.0F),
        0.034F,
        weathered(BuildingPalette::k_terracotta_light, ware + 7, 0.10F));
  }
  add_window_dressing(
      Side::Front, Opening{-0.92F, -0.64F, p + 0.56F, p + 0.82F}, 77, false);

  for (const float side : {-1.0F, 1.0F}) {
    m_desc.add_cone(QVector3D(side * 0.37F, p, k_front + 0.04F),
                    QVector3D(side * 0.37F, p + 0.20F, k_front + 0.04F),
                    0.055F,
                    BuildingPalette::k_limestone_dark);
  }
  if (intact()) {

    m_desc.add_palette_box(QVector3D(0.0F, p + 0.975F, k_front - 0.004F),
                           QVector3D(0.11F, 0.035F, 0.014F),
                           k_tenement_team_slot);
  }
}

void TenementBuilder::add_ground_storey_dressing() {
  switch (m_traits.ground) {
  case TenementGround::Door:
    add_door_front();
    break;
  case TenementGround::Passage:
    add_passage_dressing();
    break;
  case TenementGround::Shop:
  default:
    add_shop_front();
    break;
  }
}

void TenementBuilder::add_balcony(int storey) {
  BuildingPartLabel const label(m_desc, "balcony");
  BuildingPartMaterial const timber(m_desc, k_building_material_wood);
  const float fy = floor_y(storey);
  const float reach = k_front + 0.29F;
  for (const float x : {-0.84F, -0.28F, 0.28F, 0.84F}) {
    box(x - 0.022F,
        x + 0.022F,
        fy - 0.058F,
        fy - 0.012F,
        k_front - 0.03F,
        reach - 0.01F,
        BuildingPalette::k_cedar_dark);
  }
  for (const float x : {-0.84F, 0.84F}) {
    m_desc.add_cylinder(QVector3D(x, fy - 0.27F, k_front + 0.005F),
                        QVector3D(x, fy - 0.058F, reach - 0.05F),
                        0.016F,
                        BuildingPalette::k_cedar_dark);
  }
  box(-0.93F,
      0.93F,
      fy - 0.012F,
      fy + 0.010F,
      k_front,
      reach,
      BuildingPalette::k_cedar);

  const float rail_top = fy + 0.29F;
  for (const float x : {-0.91F, -0.30F, 0.30F, 0.91F}) {
    box(x - 0.014F,
        x + 0.014F,
        fy + 0.010F,
        rail_top,
        reach - 0.034F,
        reach - 0.006F,
        BuildingPalette::k_cedar_dark);
  }
  box(-0.93F,
      0.93F,
      rail_top,
      rail_top + 0.020F,
      reach - 0.040F,
      reach,
      BuildingPalette::k_cedar);
  box(-0.91F,
      0.91F,
      fy + 0.070F,
      fy + 0.086F,
      reach - 0.030F,
      reach - 0.010F,
      BuildingPalette::k_cedar_light);
  for (int bay = 0; bay < 3; ++bay) {
    const float centre = -0.605F + (0.605F * static_cast<float>(bay));
    for (const float lean : {-1.0F, 1.0F}) {
      m_desc.add_rotated_box(QVector3D(centre, fy + 0.185F, reach - 0.020F),
                             QVector3D(0.30F, 0.007F, 0.006F),
                             QVector3D(0.0F, 0.0F, lean * 19.0F),
                             BuildingPalette::k_cedar_light);
    }
  }
  for (const float x : {-0.92F, 0.92F}) {
    box(x - 0.012F,
        x + 0.012F,
        rail_top - 0.012F,
        rail_top + 0.012F,
        k_front,
        reach - 0.04F,
        BuildingPalette::k_cedar);
  }
  if (intact()) {
    add_pot(QVector3D(-0.70F, fy + 0.010F, k_front + 0.19F), storey + 20);
    add_pot(QVector3D(0.62F, fy + 0.010F, k_front + 0.20F), storey + 21);
    BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
    slot_box(0.02F,
             0.26F,
             fy + 0.13F,
             rail_top + 0.022F,
             reach,
             reach + 0.010F,
             k_tenement_cloth_slot);
  }
}

auto TenementBuilder::timber_framed(int storey) const -> bool {
  return storey == m_traits.storeys - 1 && m_traits.roof == TenementRoof::Gable &&
         (m_variant % 3) == 1;
}

void TenementBuilder::add_timber_frame(int storey) {
  BuildingPartLabel const label(m_desc, "timber_frame");
  BuildingPartMaterial const timber(m_desc, k_building_material_wood);
  const float fy = floor_y(storey);
  const float top = ceiling_y(storey) - 0.05F;
  constexpr float k_proud = 0.014F;
  for (const float x : {-1.11F, -0.44F, 0.44F, 1.11F}) {
    trim(Side::Front,
         x - 0.026F,
         x + 0.026F,
         fy + 0.014F,
         top,
         k_proud,
         0.02F,
         BuildingPalette::k_cedar_dark);
  }
  for (const float y : {fy + 0.16F, top - 0.045F}) {
    trim(Side::Front,
         -1.11F,
         1.11F,
         y,
         y + 0.036F,
         k_proud,
         0.02F,
         BuildingPalette::k_cedar_dark);
  }
  for (const float side : {-1.0F, 1.0F}) {
    m_desc.add_rotated_box(
        QVector3D(side * 0.99F, fy + 0.10F, k_front + (k_proud * 0.5F)),
        QVector3D(0.14F, 0.016F, 0.010F),
        QVector3D(0.0F, 0.0F, side * 38.0F),
        BuildingPalette::k_cedar_dark);
  }
}

void TenementBuilder::add_upper_storey_dressing() {
  for (int storey = 1; storey < m_traits.storeys; ++storey) {
    const float fy = floor_y(storey);
    const bool broken = !intact() && storey == m_traits.storeys - 1;
    {
      BuildingPartLabel const label(m_desc, "string_course");
      for (const Side side : {Side::Front, Side::Rear}) {
        trim(side,
             -k_half_width,
             k_half_width,
             fy - 0.022F,
             fy + 0.014F,
             0.022F,
             0.05F,
             BuildingPalette::k_limestone_shade);
      }
    }
    if (storey == 1 && storey != m_traits.balcony_storey) {

      BuildingPartLabel const label(m_desc, "joist_ends");
      for (int joist = 0; joist < 9; ++joist) {
        const float x = -0.96F + (0.24F * static_cast<float>(joist));
        trim(Side::Front,
             x - 0.022F,
             x + 0.022F,
             fy - 0.064F,
             fy - 0.022F,
             0.030F,
             0.06F,
             weathered(BuildingPalette::k_cedar_dark, joist + 41, 0.10F));
      }
    }
    if (timber_framed(storey) && !broken) {
      add_timber_frame(storey);
    }
    BuildingPartLabel const label(m_desc, "street_windows");
    int index = 0;
    for (const Opening& opening : front_openings(storey)) {
      const bool balcony_door =
          storey == m_traits.balcony_storey && opening.y0 < fy + 0.05F;
      if (balcony_door) {
        trim(Side::Front,
             opening.x0 - 0.02F,
             opening.x1 + 0.02F,
             opening.y1,
             opening.y1 + 0.03F,
             0.014F,
             0.05F,
             BuildingPalette::k_limestone_shade);
        trim(Side::Front,
             opening.x0 - 0.15F,
             opening.x0 - 0.004F,
             opening.y0,
             opening.y1,
             0.014F,
             0.014F,
             BuildingPalette::k_cedar_light);
      } else {
        add_window_dressing(Side::Front,
                            opening,
                            (storey * 13) + index,
                            !broken || opening.x1 < k_break_x);
      }
      ++index;
    }
    if (storey == m_traits.balcony_storey) {
      add_balcony(storey);
    }
  }
}

void TenementBuilder::add_rear_dressing() {
  const float p = k_pavement_top;
  BuildingPartLabel const label(m_desc, "court_side");
  const auto ground = rear_openings(0);
  const Opening& door =
      m_traits.ground == TenementGround::Passage ? ground[2] : ground[0];
  add_door_leaf(Side::Rear, door, BuildingPalette::k_cedar_light * 0.92F);
  trim(Side::Rear,
       door.x0 - 0.03F,
       door.x1 + 0.03F,
       door.y1,
       door.y1 + 0.035F,
       0.014F,
       0.05F,
       BuildingPalette::k_cedar_dark);
  box(door.x0 - 0.04F,
      door.x1 + 0.04F,
      0.0F,
      0.028F,
      k_rear - 0.10F,
      k_rear,
      BuildingPalette::k_limestone_dark);

  for (int storey = 1; storey < m_traits.storeys; ++storey) {
    int index = 0;
    for (const Opening& opening : rear_openings(storey)) {
      add_window_dressing(Side::Rear, opening, (storey * 17) + index + 50, intact());
      ++index;
    }
  }

  m_desc.add_cylinder(
      QVector3D(1.02F, 0.06F, k_rear - 0.002F),
      QVector3D(1.02F,
                intact() ? m_top - 0.03F : floor_y(m_traits.storeys - 1),
                k_rear - 0.002F),
      0.018F,
      BuildingPalette::k_terracotta_dark);

  if ((m_variant % 2) == 0) {

    if (intact()) {
      const float high = p + 0.90F;
      const float low = p + 0.80F;
      const float run = 0.24F;
      const float angle = std::atan2(high - low, run) * 180.0F / k_pi;
      BuildingPartMaterial const tiles(m_desc, k_building_material_ceramic);
      m_desc.add_rotated_box(
          QVector3D(0.44F, (high + low) * 0.5F, k_rear - (run * 0.5F)),
          QVector3D(0.42F, 0.012F, std::sqrt((run * run) + 0.01F) * 0.5F),
          QVector3D(-angle, 0.0F, 0.0F),
          BuildingPalette::k_terracotta);
    }
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    for (int log = 0; log < 5; ++log) {
      const int row = log < 3 ? 0 : 1;
      const float x =
          0.22F + (0.085F * static_cast<float>(log - (row * 3))) + (0.04F * row);
      const float y = 0.035F + (0.062F * static_cast<float>(row));
      m_desc.add_cylinder(QVector3D(x, y, k_rear - 0.03F),
                          QVector3D(x, y, k_rear - 0.19F),
                          0.034F,
                          weathered(BuildingPalette::k_cedar_light, log + 91, 0.12F));
    }
    add_jar(
        QVector3D(0.70F, 0.0F, k_rear - 0.10F), 1.1F, BuildingPalette::k_terracotta);
  } else {
    add_jar(QVector3D(-0.20F, 0.0F, k_rear - 0.09F),
            1.0F,
            BuildingPalette::k_terracotta_light);
    add_pot(QVector3D(0.10F, 0.0F, k_rear - 0.08F), 9);
  }
}

} // namespace Render::GL::Roman::TenementDetail
