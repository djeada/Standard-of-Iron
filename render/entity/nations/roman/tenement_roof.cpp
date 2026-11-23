#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/tenement_builder.h"

// The insula's roof: pantiled gable or walled terrace, and what a fire leaves
// of it.
namespace Render::GL::Roman::TenementDetail {

using namespace TenementLot;

// The party wall carried up into the gable: coarse courses fill the
// triangle, and a raking slab under each roof slope hides their steps.
void TenementBuilder::add_gable_end(float sign, float rise) {
  constexpr int k_courses = 5;
  const float x0 = sign > 0.0F ? k_inner_half : -k_half_width;
  const float x1 = sign > 0.0F ? k_half_width : -k_inner_half;
  const float course = rise / static_cast<float>(k_courses);
  for (int index = 0; index + 1 < k_courses; ++index) {
    const float top = static_cast<float>(index + 1) * course;
    const float half = k_half_depth * (1.0F - (top / rise));
    slot_box(x0,
             x1,
             m_top + (static_cast<float>(index) * course),
             m_top + top,
             k_mid_z - half,
             k_mid_z + half,
             k_tenement_wall_slot);
  }
  constexpr float k_rake_half = 0.055F;
  const float slope = std::sqrt((k_half_depth * k_half_depth) + (rise * rise));
  const float angle = std::atan2(rise, k_half_depth) * 180.0F / k_pi;
  const float normal_y = k_half_depth / slope;
  const float normal_z = rise / slope;
  BuildingPartMaterial const plaster(m_desc, k_building_material_stone);
  for (const float side : {-1.0F, 1.0F}) {
    m_desc.add_palette_rotated_box(
        QVector3D((x0 + x1) * 0.5F,
                  m_top + (rise * 0.5F) - (normal_y * k_rake_half),
                  k_mid_z +
                      (side * ((k_half_depth * 0.5F) - (normal_z * k_rake_half)))),
        QVector3D((x1 - x0) * 0.5F, k_rake_half, slope * 0.5F),
        QVector3D(side * angle, 0.0F, 0.0F),
        k_tenement_wall_slot);
  }
}

void TenementBuilder::add_chimney(const QVector3D& vent) {
  BuildingPartLabel const label(m_desc, "chimney");
  const float base = vent.y() - 0.26F;
  box(vent.x() - 0.06F,
      vent.x() + 0.06F,
      base,
      vent.y() - 0.06F,
      vent.z() - 0.06F,
      vent.z() + 0.06F,
      BuildingPalette::k_limestone_shade);
  box(vent.x() - 0.076F,
      vent.x() + 0.076F,
      vent.y() - 0.06F,
      vent.y() - 0.04F,
      vent.z() - 0.076F,
      vent.z() + 0.076F,
      BuildingPalette::k_terracotta_dark);
  m_desc.add_cylinder(QVector3D(vent.x(), vent.y() - 0.04F, vent.z()),
                      QVector3D(vent.x(), vent.y(), vent.z()),
                      0.036F,
                      BuildingPalette::k_terracotta);
}

// Tegulae laid in overlapping courses with a cover tile over every joint and
// an antefix closing each cover tile at the eave.
void TenementBuilder::add_pantiles(float x0, float x1, float rise) {
  BuildingPartMaterial const tiles(m_desc, k_building_material_ceramic);
  const float half_depth = k_half_depth + k_eave_overhang;
  const float slope = std::sqrt((half_depth * half_depth) + (rise * rise));
  const float angle = std::atan2(rise, half_depth) * 180.0F / k_pi;
  const float centre_x = (x0 + x1) * 0.5F;
  const float half_x = (x1 - x0) * 0.5F;
  constexpr int k_courses = 4;
  constexpr float k_thickness = 0.014F;
  constexpr float k_course_lift = 2.4F;
  constexpr float k_cover_radius = 0.030F;
  const int covers = std::max(2, static_cast<int>(std::lround((x1 - x0) / 0.215F)));
  for (const float side : {-1.0F, 1.0F}) {
    for (int course = 0; course < k_courses; ++course) {
      const float mid =
          (static_cast<float>(course) + 0.5F) / static_cast<float>(k_courses);
      m_desc.add_rotated_box(
          QVector3D(centre_x,
                    m_top + (rise * (1.0F - mid)) + 0.010F,
                    k_mid_z + (side * half_depth * mid)),
          QVector3D(half_x,
                    k_thickness,
                    (slope / static_cast<float>(k_courses) * 0.5F) + 0.014F),
          QVector3D(side * (angle - k_course_lift), 0.0F, 0.0F),
          weathered(
              BuildingPalette::k_terracotta, course + (side < 0.0F ? 3 : 9), 0.05F));
    }
    for (int cover = 0; cover <= covers; ++cover) {
      const float x =
          x0 + 0.03F +
          ((x1 - x0 - 0.06F) * static_cast<float>(cover) / static_cast<float>(covers));
      const QVector3D eave(x, m_top + 0.034F, k_mid_z + (side * (half_depth - 0.012F)));
      m_desc.add_cylinder(eave,
                          QVector3D(x, m_top + rise + 0.030F, k_mid_z),
                          k_cover_radius,
                          weathered(BuildingPalette::k_terracotta_light,
                                    cover + (side < 0.0F ? 17 : 53),
                                    0.075F));
      m_desc.add_box(eave + QVector3D(0.0F, 0.004F, side * 0.006F),
                     QVector3D(0.034F, 0.036F, 0.010F),
                     weathered(BuildingPalette::k_terracotta_light, cover + 5, 0.05F));
    }
    m_desc.add_box(
        QVector3D(centre_x, m_top - 0.006F, k_mid_z + (side * (half_depth - 0.02F))),
        QVector3D(half_x, 0.016F, 0.022F),
        BuildingPalette::k_terracotta_dark);
  }
  m_desc.add_cylinder(QVector3D(x0, m_top + rise + 0.034F, k_mid_z),
                      QVector3D(x1, m_top + rise + 0.034F, k_mid_z),
                      0.040F,
                      BuildingPalette::k_terracotta_dark);
}

void TenementBuilder::add_gable_roof() {
  const float rise = m_traits.roof_rise;
  const float half_depth = k_half_depth + k_eave_overhang;
  {
    BuildingPartLabel const label(m_desc, "cornice");
    for (const Side side : {Side::Front, Side::Rear}) {
      trim(side,
           -k_half_width,
           k_half_width,
           m_top - 0.05F,
           m_top - 0.004F,
           0.040F,
           0.07F,
           BuildingPalette::k_limestone);
    }
  }
  if (intact()) {
    {
      BuildingPartLabel const label(m_desc, "tiled_roof");
      add_pantiles(-k_half_width, k_half_width, rise);
    }
    BuildingPartLabel const label(m_desc, "gable_end");
    add_gable_end(-1.0F, rise);
    add_gable_end(1.0F, rise);
    add_chimney(tenement_vent(m_variant));
    return;
  }

  // Damaged: the roof survives over the left bays; charred rafters span the
  // rest.
  const float kept_end = k_break_x - 0.04F;
  {
    BuildingPartLabel const label(m_desc, "tiled_roof");
    add_pantiles(-k_half_width, kept_end, rise);
  }
  {
    BuildingPartLabel const label(m_desc, "gable_end");
    add_gable_end(-1.0F, rise);
  }
  BuildingPartLabel const label(m_desc, "burnt_rafters");
  BuildingPartMaterial const timber(m_desc, k_building_material_wood);
  const float slope = std::sqrt((half_depth * half_depth) + (rise * rise));
  const float angle = std::atan2(rise, half_depth) * 180.0F / k_pi;
  int index = 0;
  for (const float x : {0.02F, 0.36F, 0.70F, 1.04F}) {
    for (const float side : {-1.0F, 1.0F}) {
      ++index;
      if (roll(index + 150) < 0.28F) {
        continue;
      }
      m_desc.add_rotated_box(
          QVector3D(x, m_top + (rise * 0.5F), k_mid_z + (side * half_depth * 0.5F)),
          QVector3D(0.020F, 0.018F, slope * 0.5F),
          QVector3D(side * angle, 0.0F, 0.0F),
          k_charred * (0.9F + (0.5F * roll(index + 170))));
    }
  }
  m_desc.add_cylinder(QVector3D(kept_end, m_top + rise, k_mid_z),
                      QVector3D(k_half_width - 0.12F, m_top + rise - 0.05F, k_mid_z),
                      0.022F,
                      k_charred * 1.2F);
}

void TenementBuilder::add_terrace_roof() {
  BuildingPartLabel const label(m_desc, "roof_terrace");
  const float deck = m_top + 0.022F;
  const float right = intact() ? k_half_width : k_break_x;
  box(-k_half_width, right, m_top - 0.012F, deck, k_rear, k_front, k_signinum);
  const float parapet = deck + 0.17F;
  const auto wall_run = [&](float x0, float x1, float z0, float z1) {
    slot_box(x0, x1, deck, parapet, z0, z1, k_tenement_wall_slot);
    box(x0 - 0.008F,
        x1 + 0.008F,
        parapet,
        parapet + 0.018F,
        z0 - 0.008F,
        z1 + 0.008F,
        BuildingPalette::k_limestone);
  };
  wall_run(-k_half_width, right, k_front - 0.07F, k_front);
  wall_run(-k_half_width, right, k_rear, k_rear + 0.07F);
  wall_run(-k_half_width, -k_half_width + 0.07F, k_rear + 0.07F, k_front - 0.07F);
  if (intact()) {
    wall_run(k_half_width - 0.07F, k_half_width, k_rear + 0.07F, k_front - 0.07F);
  }

  // The stair head, with its own little pent roof.
  const float hut_top = deck + 0.46F;
  slot_box(-1.02F,
           -0.42F,
           deck,
           hut_top,
           k_rear + 0.07F,
           k_rear + 0.62F,
           k_tenement_wall_slot);
  box(-0.60F,
      -0.44F,
      deck,
      deck + 0.36F,
      k_rear + 0.612F,
      k_rear + 0.636F,
      BuildingPalette::k_cedar);
  {
    BuildingPartMaterial const tiles(m_desc, k_building_material_ceramic);
    m_desc.add_rotated_box(QVector3D(-0.72F, hut_top + 0.04F, k_rear + 0.35F),
                           QVector3D(0.34F, 0.014F, 0.32F),
                           QVector3D(9.0F, 0.0F, 0.0F),
                           BuildingPalette::k_terracotta);
    for (int roll_index = 0; roll_index < 4; ++roll_index) {
      const float x = -0.99F + (0.18F * static_cast<float>(roll_index));
      m_desc.add_cylinder(
          QVector3D(x, hut_top + 0.105F, k_rear + 0.05F),
          QVector3D(x, hut_top + 0.005F, k_rear + 0.65F),
          0.013F,
          weathered(BuildingPalette::k_terracotta_light, roll_index + 23, 0.07F));
    }
  }

  if (!intact()) {
    return;
  }

  // A vine pergola over the street side.
  {
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    const float beam_y = deck + 0.44F;
    for (const float x : {0.08F, 1.00F}) {
      for (const float z : {k_front - 0.16F, k_mid_z + 0.10F}) {
        m_desc.add_cylinder(QVector3D(x, deck, z),
                            QVector3D(x, beam_y, z),
                            0.016F,
                            BuildingPalette::k_cedar_dark);
      }
      box(x - 0.016F,
          x + 0.016F,
          beam_y,
          beam_y + 0.026F,
          k_mid_z + 0.04F,
          k_front - 0.10F,
          BuildingPalette::k_cedar_dark);
    }
    for (int slat = 0; slat < 5; ++slat) {
      const float z = k_mid_z + 0.14F + (0.19F * static_cast<float>(slat));
      box(0.02F,
          1.06F,
          beam_y + 0.026F,
          beam_y + 0.044F,
          z - 0.012F,
          z + 0.012F,
          BuildingPalette::k_cedar_light);
    }
  }
  {
    BuildingPartMaterial const plant(m_desc, k_building_material_plant);
    const float leaf_y = deck + 0.484F;
    box(0.10F,
        0.62F,
        leaf_y,
        leaf_y + 0.035F,
        k_mid_z + 0.20F,
        k_front - 0.22F,
        k_leaf);
    box(0.50F,
        1.04F,
        leaf_y + 0.006F,
        leaf_y + 0.046F,
        k_mid_z + 0.34F,
        k_front - 0.14F,
        k_leaf_light);
  }
  add_pot(QVector3D(0.92F, deck, k_rear + 0.22F), 31);
  add_pot(QVector3D(0.70F, deck, k_rear + 0.20F), 32);
  add_pot(QVector3D(-0.24F, deck, k_front - 0.20F), 33);
  add_jar(QVector3D(-0.20F, deck, k_rear + 0.24F), 0.9F, BuildingPalette::k_terracotta);
}

void TenementBuilder::add_roof() {
  if (m_traits.roof == TenementRoof::Terrace) {
    add_terrace_roof();
  } else {
    add_gable_roof();
  }
}

} // namespace Render::GL::Roman::TenementDetail
