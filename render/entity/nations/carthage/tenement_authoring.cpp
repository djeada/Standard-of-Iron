#include "render/entity/nations/carthage/tenement_authoring.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/nation_motifs.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/tenement_shell.h"

namespace Render::GL::Carthage {
namespace {

using namespace TenementLot;
using namespace TenementShellDims;

using Opening = TenementOpening;
using Side = TenementSide;

constexpr float k_pi = 3.14159265F;
// Deep enough that the shop reads as a room behind the counter, not a niche.
constexpr float k_shop_depth = 0.76F;
constexpr float k_parapet = 0.17F;

constexpr QVector3D k_void{0.105F, 0.090F, 0.080F};
constexpr QVector3D k_interior{0.52F, 0.45F, 0.35F};
constexpr QVector3D k_deck{0.74F, 0.66F, 0.52F};
constexpr QVector3D k_reed{0.72F, 0.62F, 0.36F};
constexpr QVector3D k_leaf{0.26F, 0.43F, 0.18F};
constexpr QVector3D k_leaf_light{0.38F, 0.52F, 0.22F};
constexpr QVector3D k_charred{0.15F, 0.12F, 0.10F};
constexpr QVector3D k_paving{0.70F, 0.63F, 0.50F};
constexpr QVector3D k_paving_edge{0.55F, 0.49F, 0.39F};
constexpr QVector3D k_whitewash{0.93F, 0.90F, 0.82F};

class PunicTenementBuilder : public TenementShell {
public:
  PunicTenementBuilder(BuildingArchetypeDesc& desc, BuildingState state, int variant)
      : TenementShell(
            desc, state, variant, k_tenement_wall_slot, k_tenement_wall_slot) {}

  void build() {
    add_pavement();
    add_core();
    {
      BuildingPartLabel const label(m_desc, "party_wall");
      add_side_wall(-1.0F);
      add_side_wall(1.0F);
    }
    for (int storey = 0; storey < storeys(); ++storey) {
      {
        BuildingPartLabel const label(m_desc, "street_wall");
        wall_storey(Side::Front, storey, front_openings(storey));
      }
      BuildingPartLabel const label(m_desc, "court_wall");
      wall_storey(Side::Rear, storey, rear_openings(storey));
    }
    if (m_state != BuildingState::Destroyed) {
      add_street_storey();
      add_upper_storeys();
      add_court_side();
      add_roof();
    }
    add_ruin_dressing(m_desc,
                      RuinDressing{.center = QVector3D(0.0F, 0.0F, k_mid_z),
                                   .extent = QVector3D(1.02F, 0.0F, 1.02F),
                                   .stone = BuildingPalette::k_sandstone,
                                   .stone_dark = BuildingPalette::k_sandstone_dark,
                                   .timber = BuildingPalette::k_wood_dark,
                                   .ground_y = k_pavement_top + 0.14F,
                                   .apron_y = 0.02F,
                                   .scale = 1.15F,
                                   .seed = 307 + (m_variant * 11)});
  }

private:
  [[nodiscard]] auto front_openings(int storey) const -> std::vector<Opening> {
    const float p = k_pavement_top;
    if (storey == 0) {
      switch (m_traits.ground) {
      case TenementGround::Door:
        return {{-0.20F, 0.20F, p, p + 0.82F}, {0.62F, 0.84F, p + 0.60F, p + 0.80F}};
      case TenementGround::Passage:
        return {{-0.90F, -0.68F, p + 0.60F, p + 0.80F}, {-0.30F, 0.30F, p, p + 0.84F}};
      case TenementGround::Shop:
      default:
        return {{-0.96F, 0.10F, p, p + 0.74F}, {0.50F, 0.90F, p, p + 0.80F}};
      }
    }
    // The street sees little of a Punic house: small square lights set high.
    const float fy = floor_y(storey);
    const bool bay = storey == m_traits.balcony_storey;
    const float sill = fy + 0.30F;
    const float head = fy + 0.54F;
    if (bay) {
      return {{-0.86F, -0.66F, sill, head}, {-0.20F, 0.20F, fy + 0.20F, fy + 0.58F}};
    }
    if (((m_variant + storey) % 2) == 0) {
      return {{-0.66F, -0.44F, sill, head}, {0.44F, 0.66F, sill, head}};
    }
    return {{-0.86F, -0.66F, sill, head},
            {-0.11F, 0.11F, sill, head},
            {0.66F, 0.86F, sill, head}};
  }

  [[nodiscard]] auto rear_openings(int storey) const -> std::vector<Opening> {
    const float p = k_pavement_top;
    if (storey == 0) {
      if (m_traits.ground == TenementGround::Passage) {
        return {{-0.30F, 0.30F, 0.0F, p + 0.82F}, {0.56F, 0.88F, 0.02F, 0.78F}};
      }
      return {{-0.80F, -0.48F, 0.02F, 0.78F}, {0.36F, 0.58F, p + 0.56F, p + 0.76F}};
    }
    const float fy = floor_y(storey);
    return {{-0.62F, -0.40F, fy + 0.30F, fy + 0.54F},
            {0.40F, 0.62F, fy + 0.30F, fy + 0.54F}};
  }

  void add_pavement() {
    BuildingPartLabel const label(m_desc, "pavement");
    box(-k_half_width,
        k_half_width,
        0.0F,
        k_pavement_top,
        k_front,
        k_pavement_reach - 0.04F,
        k_paving);
    box(-k_half_width,
        k_half_width,
        0.0F,
        k_pavement_top + 0.008F,
        k_pavement_reach - 0.04F,
        k_pavement_reach,
        k_paving_edge);
  }

  void add_core() {
    BuildingPartLabel const label(m_desc, "core");
    const float inner_rear = k_rear + k_wall;
    const float inner_front = k_front - k_wall;
    const float ground_top = ceiling_y(0);
    if (m_state == BuildingState::Destroyed) {
      box(-k_inner_half,
          k_inner_half,
          0.0F,
          k_pavement_top + 0.14F,
          inner_rear,
          inner_front,
          BuildingPalette::k_sandstone_dark * 0.85F);
      return;
    }
    switch (m_traits.ground) {
    case TenementGround::Shop: {
      const float back = k_front - k_shop_depth;
      box(-k_inner_half, k_inner_half, 0.0F, ground_top, inner_rear, back, k_void);
      box(0.30F, k_inner_half, 0.0F, ground_top, back, inner_front, k_void);
      box(-k_inner_half,
          0.30F,
          0.0F,
          k_pavement_top + 0.006F,
          back,
          inner_front,
          k_interior * 0.8F);
      box(-k_inner_half,
          0.285F,
          k_pavement_top + 0.006F,
          ground_top,
          back,
          back + 0.012F,
          k_interior);
      box(0.285F,
          0.30F,
          k_pavement_top + 0.006F,
          ground_top,
          back + 0.012F,
          inner_front,
          k_interior * 0.92F);
      break;
    }
    case TenementGround::Passage:
      box(-k_inner_half, -0.30F, 0.0F, ground_top, inner_rear, inner_front, k_void);
      box(0.30F, k_inner_half, 0.0F, ground_top, inner_rear, inner_front, k_void);
      box(-0.30F,
          0.30F,
          0.0F,
          k_pavement_top * 0.6F,
          k_rear,
          k_front,
          k_paving_edge * 0.92F);
      break;
    case TenementGround::Door:
    default:
      box(-k_inner_half,
          k_inner_half,
          0.0F,
          ground_top,
          inner_rear,
          inner_front,
          k_void);
      break;
    }
    if (intact()) {
      box(-k_inner_half,
          k_inner_half,
          ground_top,
          m_top,
          inner_rear,
          inner_front,
          k_void);
      return;
    }
    const float broken_floor = floor_y(m_traits.storeys - 1);
    box(-k_inner_half,
        k_inner_half,
        ground_top,
        broken_floor,
        inner_rear,
        inner_front,
        k_void);
    box(-k_inner_half, k_break_x, broken_floor, m_top, inner_rear, inner_front, k_void);
    box(k_break_x,
        k_inner_half,
        broken_floor,
        broken_floor + 0.10F,
        inner_rear,
        inner_front,
        k_charred * 1.5F);
  }

  void
  accent(Side side, float x0, float x1, float y0, float y1, float proud, float depth) {
    const float outer = face(side) + (outward(side) * proud);
    const float inner = outer - (outward(side) * depth);
    // Paint on plaster: mineral, whatever the pigment.
    BuildingPartMaterial const paint(m_desc, k_building_material_stone);
    slot_box(x0,
             x1,
             y0,
             y1,
             std::min(outer, inner),
             std::max(outer, inner),
             k_tenement_accent_slot);
  }

  [[nodiscard]] auto standing(const Opening& opening) const -> bool {
    const float mid = (opening.x0 + opening.x1) * 0.5F;
    return intact() || mid < k_break_x || cap(mid) > opening.y1 + 0.06F;
  }

  void add_jar(const QVector3D& foot, float scale, const QVector3D& clay) {
    const auto at = [&](float y) {
      return foot + QVector3D(0.0F, y * scale, 0.0F);
    };
    const float r = 0.058F * scale;
    m_desc.add_cone(at(0.045F), at(-0.004F), r * 0.9F, clay);
    m_desc.add_cylinder(at(0.043F), at(0.165F), r, clay);
    m_desc.add_cone(at(0.163F), at(0.235F), r, clay);
    m_desc.add_cylinder(at(0.205F), at(0.262F), r * 0.38F, clay * 0.9F);
  }

  void add_pot(const QVector3D& foot, int salt) {
    m_desc.add_cylinder(foot,
                        foot + QVector3D(0.0F, 0.07F, 0.0F),
                        0.045F,
                        weathered(BuildingPalette::k_brick, salt, 0.08F));
    BuildingPartMaterial const plant(m_desc, k_building_material_plant);
    m_desc.add_cone(foot + QVector3D(0.0F, 0.06F, 0.0F),
                    foot + QVector3D(0.0F, 0.22F + (0.08F * roll(salt)), 0.0F),
                    0.062F,
                    (salt % 2) == 0 ? k_leaf : k_leaf_light);
  }

  // A painted frame round a small light, and a wooden lattice in it.
  void add_light(Side side, const Opening& opening, int salt) {
    if (!standing(opening)) {
      return;
    }
    constexpr float k_frame = 0.028F;
    accent(side,
           opening.x0 - k_frame,
           opening.x1 + k_frame,
           opening.y1,
           opening.y1 + k_frame,
           0.012F,
           0.04F);
    accent(side,
           opening.x0 - k_frame,
           opening.x1 + k_frame,
           opening.y0 - k_frame,
           opening.y0,
           0.020F,
           0.05F);
    accent(
        side, opening.x0 - k_frame, opening.x0, opening.y0, opening.y1, 0.012F, 0.04F);
    accent(
        side, opening.x1, opening.x1 + k_frame, opening.y0, opening.y1, 0.012F, 0.04F);
    if (roll(salt) < 0.45F) {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      const float mid_x = (opening.x0 + opening.x1) * 0.5F;
      const float mid_y = (opening.y0 + opening.y1) * 0.5F;
      trim(side,
           mid_x - 0.008F,
           mid_x + 0.008F,
           opening.y0,
           opening.y1,
           -0.030F,
           0.014F,
           BuildingPalette::k_wood_dark);
      trim(side,
           opening.x0,
           opening.x1,
           mid_y - 0.008F,
           mid_y + 0.008F,
           -0.030F,
           0.014F,
           BuildingPalette::k_wood_dark);
    }
  }

  void add_door(Side side, const Opening& opening, bool painted) {
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    trim(side,
         opening.x0,
         opening.x1,
         opening.y0,
         opening.y1,
         -0.044F,
         0.020F,
         BuildingPalette::k_wood);
    const float mid = (opening.x0 + opening.x1) * 0.5F;
    trim(side,
         mid - 0.007F,
         mid + 0.007F,
         opening.y0,
         opening.y1,
         -0.036F,
         0.014F,
         BuildingPalette::k_wood_dark);
    if (painted) {
      for (const float y : {opening.y0 + 0.22F, opening.y0 + 0.56F}) {
        accent(side,
               opening.x0 + 0.02F,
               opening.x1 - 0.02F,
               y,
               y + 0.03F,
               -0.030F,
               0.010F);
      }
    }
  }

  // The low painted band that runs along a Punic street front.
  void add_base_band(const std::vector<Opening>& openings) {
    BuildingPartLabel const label(m_desc, "base_band");
    std::vector<Opening> sorted = openings;
    std::sort(sorted.begin(), sorted.end(), [](const Opening& a, const Opening& b) {
      return a.x0 < b.x0;
    });
    float cursor = -k_inner_half;
    const float y0 = k_pavement_top;
    const float y1 = k_pavement_top + 0.16F;
    for (const Opening& opening : sorted) {
      if (opening.y0 > y1) {
        continue;
      }
      accent(Side::Front, cursor, opening.x0, y0, y1, 0.010F, 0.03F);
      cursor = opening.x1;
    }
    accent(Side::Front, cursor, k_inner_half, y0, y1, 0.010F, 0.03F);
  }

  void
  add_reed_awning(float x0, float x1, float wall_y, float edge_y, float reach = 0.38F) {
    BuildingPartLabel const label(m_desc, "awning");
    const float wall_z = k_front + 0.012F;
    const float edge_z = k_front + reach;
    const float run = edge_z - wall_z;
    const float drop = wall_y - edge_y;
    const float length = std::sqrt((run * run) + (drop * drop));
    const float angle = std::atan2(drop, run) * 180.0F / k_pi;
    {
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_desc.add_palette_rotated_box(QVector3D((x0 + x1) * 0.5F,
                                               (wall_y + edge_y) * 0.5F,
                                               (wall_z + edge_z) * 0.5F),
                                     QVector3D((x1 - x0) * 0.5F, 0.006F, length * 0.5F),
                                     QVector3D(angle, 0.0F, 0.0F),
                                     k_tenement_cloth_slot);
    }
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    // Carried on struts off the wall, over the heads of the street: a post
    // on the pavement would stand where the lot has no footprint.
    for (const float x : {x0 + 0.04F, x1 - 0.04F}) {
      m_desc.add_cylinder(QVector3D(x, edge_y - 0.15F, wall_z),
                          QVector3D(x, edge_y, edge_z - 0.02F),
                          0.014F,
                          BuildingPalette::k_wood_dark);
    }
    m_desc.add_cylinder(QVector3D(x0, edge_y, edge_z - 0.02F),
                        QVector3D(x1, edge_y, edge_z - 0.02F),
                        0.011F,
                        BuildingPalette::k_wood_dark);
  }

  void add_street_storey() {
    const float p = k_pavement_top;
    const auto openings = front_openings(0);
    add_base_band(openings);
    BuildingPartLabel const label(m_desc, "street_front");
    switch (m_traits.ground) {
    case TenementGround::Door: {
      const Opening door = openings[0];
      add_door(Side::Front, door, true);
      accent(Side::Front,
             door.x0 - 0.07F,
             door.x1 + 0.07F,
             door.y1,
             door.y1 + 0.07F,
             0.022F,
             0.06F);
      accent(Side::Front, door.x0 - 0.07F, door.x0, p, door.y1, 0.016F, 0.05F);
      accent(Side::Front, door.x1, door.x1 + 0.07F, p, door.y1, 0.016F, 0.05F);
      box(door.x0 - 0.10F,
          door.x1 + 0.10F,
          p,
          p + 0.03F,
          k_front,
          k_front + 0.12F,
          BuildingPalette::k_sandstone_light);
      if (intact()) {
        Detail::add_tanit_sign(m_desc,
                               QVector3D(0.0F, p + 1.08F, k_front + 0.006F),
                               BuildingFacadePlane::XY,
                               0.42F,
                               1,
                               0,
                               BuildingPalette::k_bronze,
                               BuildingPalette::k_sandstone_dark,
                               BuildingStateMask::All,
                               1.0F);
        m_desc.add_palette_box(QVector3D(0.44F, p + 0.62F, k_front + 0.008F),
                               QVector3D(0.07F, 0.09F, 0.010F),
                               k_tenement_team_slot);
      }
      add_light(Side::Front, openings[1], 61);
      add_jar(QVector3D(-0.46F, p, k_front + 0.10F), 1.15F, BuildingPalette::k_brick);
      add_pot(QVector3D(-0.78F, p, k_front + 0.10F), 5);
      break;
    }
    case TenementGround::Passage: {
      const Opening way = openings[1];
      {
        BuildingPartMaterial const timber(m_desc, k_building_material_wood);
        trim(Side::Front,
             way.x0 - 0.08F,
             way.x1 + 0.08F,
             way.y1,
             way.y1 + 0.075F,
             0.020F,
             0.10F,
             BuildingPalette::k_wood_dark);
      }
      accent(Side::Front, way.x0 - 0.06F, way.x0, p, way.y1, 0.014F, 0.05F);
      accent(Side::Front, way.x1, way.x1 + 0.06F, p, way.y1, 0.014F, 0.05F);
      add_light(Side::Front, openings[0], 63);
      if (intact()) {
        m_desc.add_palette_box(QVector3D(0.0F, p + 0.975F, k_front + 0.006F),
                               QVector3D(0.10F, 0.03F, 0.010F),
                               k_tenement_team_slot);
      }
      add_jar(QVector3D(0.62F, p, k_front + 0.10F), 1.0F, BuildingPalette::k_brick);
      add_jar(
          QVector3D(0.80F, p, k_front + 0.12F), 0.8F, BuildingPalette::k_brick_dark);
      break;
    }
    case TenementGround::Shop:
    default: {
      const Opening shop = openings[0];
      const Opening door = openings[1];
      const float back = k_front - k_shop_depth;
      {
        BuildingPartMaterial const timber(m_desc, k_building_material_wood);
        trim(Side::Front,
             shop.x0 - 0.06F,
             shop.x1 + 0.06F,
             shop.y1,
             shop.y1 + 0.06F,
             0.018F,
             0.09F,
             BuildingPalette::k_wood_dark);
        // A plank counter on trestles, and the day's wares on it.
        box(shop.x0 + 0.04F,
            shop.x1 - 0.20F,
            p + 0.25F,
            p + 0.275F,
            k_front - 0.24F,
            k_front - 0.03F,
            BuildingPalette::k_wood_light);
        for (const float x : {shop.x0 + 0.10F, shop.x1 - 0.26F}) {
          box(x - 0.02F,
              x + 0.02F,
              p,
              p + 0.25F,
              k_front - 0.22F,
              k_front - 0.05F,
              BuildingPalette::k_wood_dark);
        }
        box(shop.x0 + 0.06F,
            shop.x1 - 0.06F,
            p + 0.46F,
            p + 0.478F,
            back + 0.012F,
            back + 0.10F,
            BuildingPalette::k_wood);
      }
      for (int ware = 0; ware < 4; ++ware) {
        const float x = shop.x0 + 0.16F + (0.19F * static_cast<float>(ware));
        m_desc.add_cylinder(
            QVector3D(x, p + 0.275F, k_front - 0.14F),
            QVector3D(x, p + 0.33F + (0.04F * roll(ware + 3)), k_front - 0.14F),
            0.040F,
            weathered((ware % 2) == 0 ? BuildingPalette::k_brick
                                      : BuildingPalette::k_bronze,
                      ware + 21,
                      0.10F));
        m_desc.add_cylinder(QVector3D(x + 0.04F, p + 0.478F, back + 0.055F),
                            QVector3D(x + 0.04F, p + 0.56F, back + 0.055F),
                            0.030F,
                            weathered(BuildingPalette::k_brick, ware + 31, 0.12F));
      }
      if (intact()) {
        // Dyed cloth hung out for sale.
        BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
        slot_box(shop.x1 - 0.17F,
                 shop.x1 - 0.03F,
                 p + 0.30F,
                 shop.y1 - 0.02F,
                 k_front - 0.05F,
                 k_front - 0.035F,
                 k_tenement_cloth_slot);
        accent(Side::Front,
               shop.x1 - 0.34F,
               shop.x1 - 0.20F,
               p + 0.36F,
               shop.y1 - 0.02F,
               -0.036F,
               0.014F);
        // Each variant hangs its awning at its own height and reach.
        constexpr std::array<float, k_tenement_variant_count> k_lift{
            0.0F, 0.09F, -0.015F, 0.13F, 0.04F, -0.01F, 0.11F, 0.06F};
        constexpr std::array<float, k_tenement_variant_count> k_reach{
            0.38F, 0.52F, 0.30F, 0.46F, 0.42F, 0.28F, 0.50F, 0.34F};
        const auto look =
            static_cast<std::size_t>(m_variant % k_tenement_variant_count);
        add_reed_awning(shop.x0 - 0.06F,
                        shop.x1 + 0.08F,
                        p + 0.92F + k_lift[look],
                        p + 0.76F + (k_lift[look] * 0.8F),
                        k_reach[look]);
      }
      add_door(Side::Front, door, true);
      accent(Side::Front,
             door.x0 - 0.05F,
             door.x1 + 0.05F,
             door.y1,
             door.y1 + 0.05F,
             0.016F,
             0.05F);
      add_jar(QVector3D(0.30F, p, k_front + 0.10F), 1.1F, BuildingPalette::k_brick);
      break;
    }
    }
  }

  // A screened timber bay hung on the street wall: the women's window.
  void add_bay(int storey) {
    BuildingPartLabel const label(m_desc, "screened_bay");
    const float fy = floor_y(storey);
    const float y0 = fy + 0.14F;
    const float y1 = fy + 0.62F;
    const float reach = k_front + 0.20F;
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    box(-0.30F, 0.30F, y0 - 0.03F, y0, k_front, reach, BuildingPalette::k_wood_dark);
    box(-0.32F,
        0.32F,
        y1,
        y1 + 0.03F,
        k_front,
        reach + 0.02F,
        BuildingPalette::k_wood_dark);
    for (const float x : {-0.28F, 0.0F, 0.28F}) {
      box(x - 0.016F,
          x + 0.016F,
          y0,
          y1,
          reach - 0.03F,
          reach,
          BuildingPalette::k_wood);
    }
    for (const float x : {-0.29F, 0.29F}) {
      box(x - 0.014F,
          x + 0.014F,
          y0,
          y1,
          k_front,
          reach - 0.03F,
          BuildingPalette::k_wood);
      m_desc.add_cylinder(QVector3D(x, y0 - 0.22F, k_front + 0.004F),
                          QVector3D(x, y0 - 0.03F, reach - 0.04F),
                          0.014F,
                          BuildingPalette::k_wood_dark);
    }
    for (int bar = 0; bar < 4; ++bar) {
      const float y = y0 + 0.07F + (0.11F * static_cast<float>(bar));
      box(-0.28F,
          0.28F,
          y,
          y + 0.014F,
          reach - 0.022F,
          reach - 0.008F,
          BuildingPalette::k_wood_light);
    }
    for (const float x : {-0.14F, 0.14F}) {
      box(x - 0.007F,
          x + 0.007F,
          y0,
          y1,
          reach - 0.024F,
          reach - 0.010F,
          BuildingPalette::k_wood_light);
    }
  }

  void add_upper_storeys() {
    for (int storey = 1; storey < m_traits.storeys; ++storey) {
      BuildingPartLabel const label(m_desc, "street_lights");
      const bool bay = storey == m_traits.balcony_storey;
      int index = 0;
      for (const Opening& opening : front_openings(storey)) {
        if (bay && index == 1) {
          if (intact()) {
            add_bay(storey);
          }
        } else {
          add_light(Side::Front, opening, (storey * 13) + index);
        }
        ++index;
      }
      index = 0;
      for (const Opening& opening : rear_openings(storey)) {
        add_light(Side::Rear, opening, (storey * 17) + index + 40);
        ++index;
      }
    }
  }

  void add_court_side() {
    BuildingPartLabel const label(m_desc, "court_side");
    const auto ground = rear_openings(0);
    const Opening& door =
        m_traits.ground == TenementGround::Passage ? ground[1] : ground[0];
    add_door(Side::Rear, door, false);
    box(door.x0 - 0.04F,
        door.x1 + 0.04F,
        0.0F,
        0.028F,
        k_rear - 0.10F,
        k_rear,
        BuildingPalette::k_sandstone_dark);
    if (m_traits.ground != TenementGround::Passage) {
      add_light(Side::Rear, ground[1], 71);
    }
    // The spout that carries the roof's rain to the court cistern.
    m_desc.add_cylinder(QVector3D(0.98F, m_top - 0.04F, k_rear + 0.02F),
                        QVector3D(0.98F, m_top - 0.07F, k_rear - 0.02F),
                        0.020F,
                        BuildingPalette::k_brick);
    add_jar(QVector3D(0.86F, 0.0F, k_rear - 0.10F), 1.25F, BuildingPalette::k_brick);
    if ((m_variant % 2) == 0) {
      add_pot(QVector3D(0.14F, 0.0F, k_rear - 0.08F), 9);
      add_jar(
          QVector3D(-0.22F, 0.0F, k_rear - 0.09F), 0.9F, BuildingPalette::k_brick_dark);
    }
  }

  void stepped_merlon(float x, float z, float base) {
    slot_box(x - 0.075F,
             x + 0.075F,
             base,
             base + 0.055F,
             z - 0.035F,
             z + 0.035F,
             k_tenement_wall_slot);
    slot_box(x - 0.040F,
             x + 0.040F,
             base + 0.055F,
             base + 0.105F,
             z - 0.035F,
             z + 0.035F,
             k_tenement_wall_slot);
  }

  void add_roof() {
    BuildingPartLabel const label(m_desc, "roof_terrace");
    const float deck = m_top + 0.022F;
    const float right = intact() ? k_half_width : k_break_x;
    box(-k_half_width, right, m_top - 0.012F, deck, k_rear, k_front, k_deck);
    const float parapet = deck + k_parapet;
    const auto run = [&](float x0, float x1, float z0, float z1) {
      slot_box(x0, x1, deck, parapet, z0, z1, k_tenement_wall_slot);
    };
    run(-k_half_width, right, k_front - 0.07F, k_front);
    run(-k_half_width, right, k_rear, k_rear + 0.07F);
    run(-k_half_width, -k_half_width + 0.07F, k_rear + 0.07F, k_front - 0.07F);
    if (intact()) {
      run(k_half_width - 0.07F, k_half_width, k_rear + 0.07F, k_front - 0.07F);
    }
    // A painted line under the parapet, and stepped merlons at the corners.
    accent(
        Side::Front, -k_half_width, right, m_top - 0.06F, m_top - 0.02F, 0.012F, 0.04F);
    for (const float x : {-k_half_width + 0.08F, 0.0F, k_half_width - 0.08F}) {
      if (x > right) {
        continue;
      }
      stepped_merlon(x, k_front - 0.035F, parapet);
      stepped_merlon(x, k_rear + 0.035F, parapet);
    }
    if (!intact()) {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      for (int beam = 0; beam < 4; ++beam) {
        const float x = 0.02F + (0.30F * static_cast<float>(beam));
        if (roll(beam + 90) < 0.3F) {
          continue;
        }
        box(x - 0.02F,
            x + 0.02F,
            m_top - 0.03F,
            m_top + 0.005F,
            k_rear + 0.08F,
            k_front - 0.08F,
            k_charred * (0.9F + (0.5F * roll(beam + 95))));
      }
      return;
    }

    const QVector3D vent = tenement_vent(m_variant);
    m_desc.add_cylinder(QVector3D(vent.x(), deck, vent.z()),
                        QVector3D(vent.x(), vent.y(), vent.z()),
                        0.05F,
                        BuildingPalette::k_brick);

    if (m_traits.roof == TenementRoof::Terrace) {
      // The upper room and its shaded sitting place.
      const float room_top = deck + 0.50F;
      slot_box(-1.04F,
               -0.30F,
               deck,
               room_top,
               k_rear + 0.07F,
               k_rear + 0.80F,
               k_tenement_wall_slot);
      box(-1.06F,
          -0.28F,
          room_top,
          room_top + 0.03F,
          k_rear + 0.05F,
          k_rear + 0.82F,
          k_deck * 0.94F);
      {
        BuildingPartMaterial const timber(m_desc, k_building_material_wood);
        box(-0.62F,
            -0.42F,
            deck,
            deck + 0.38F,
            k_rear + 0.792F,
            k_rear + 0.812F,
            BuildingPalette::k_wood);
        for (const float x : {0.02F, 1.02F}) {
          for (const float z : {k_mid_z + 0.06F, k_front - 0.16F}) {
            m_desc.add_cylinder(QVector3D(x, deck, z),
                                QVector3D(x, deck + 0.46F, z),
                                0.016F,
                                BuildingPalette::k_wood_dark);
          }
        }
      }
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      slot_box(-0.02F,
               1.06F,
               deck + 0.46F,
               deck + 0.472F,
               k_mid_z + 0.02F,
               k_front - 0.12F,
               k_tenement_cloth_slot);
    } else {
      // A cistern head, whitewashed, and the reed mats laid out to dry.
      m_desc.add_cylinder(QVector3D(-0.62F, deck, k_rear + 0.44F),
                          QVector3D(-0.62F, deck + 0.10F, k_rear + 0.44F),
                          0.22F,
                          k_whitewash);
      m_desc.add_cylinder(QVector3D(-0.62F, deck + 0.10F, k_rear + 0.44F),
                          QVector3D(-0.62F, deck + 0.17F, k_rear + 0.44F),
                          0.17F,
                          k_whitewash * 0.98F);
      m_desc.add_cylinder(QVector3D(-0.62F, deck + 0.17F, k_rear + 0.44F),
                          QVector3D(-0.62F, deck + 0.21F, k_rear + 0.44F),
                          0.10F,
                          k_whitewash * 0.96F);
      BuildingPartMaterial const reed(m_desc, k_building_material_cloth);
      box(0.10F, 0.56F, deck, deck + 0.012F, k_mid_z + 0.10F, k_front - 0.20F, k_reed);
      box(0.62F,
          1.00F,
          deck,
          deck + 0.010F,
          k_mid_z + 0.22F,
          k_front - 0.26F,
          k_reed * 0.9F);
    }
    add_pot(QVector3D(0.94F, deck, k_rear + 0.20F), 31);
    add_pot(QVector3D(0.74F, deck, k_rear + 0.18F), 32);
    add_jar(QVector3D(-0.14F, deck, k_rear + 0.22F), 0.95F, BuildingPalette::k_brick);
    add_jar(
        QVector3D(0.06F, deck, k_rear + 0.20F), 0.8F, BuildingPalette::k_brick_dark);
  }
};

} // namespace

auto tenement_vent(int variant) -> QVector3D {
  const TenementTraits& traits = tenement_traits(variant);
  return {0.46F, tenement_wall_top(traits) + 0.30F, k_rear + 0.36F};
}

auto build_tenement_desc(BuildingState state, int variant) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc("carthage_tenement_variant_" + std::to_string(variant));
  PunicTenementBuilder(desc, state, variant).build();
  // The dark course the whole street stands on: black basalt under the
  // whitewash, knee-high and standing proud of the wall all round, with a
  // dressed lip, so the white house sits on the street rather than on air.
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    const float foot = TenementLot::k_pavement_top;
    constexpr float k_course = 0.30F;
    constexpr float k_proud = 0.03F;
    constexpr float k_side_proud = 0.015F;
    for (const float sign : {1.0F, -1.0F}) {
      const float face = sign > 0.0F ? TenementLot::k_front : TenementLot::k_rear;
      desc.add_box(
          QVector3D(0.0F, foot + (k_course * 0.5F), face + (sign * k_proud * 0.5F)),
          QVector3D(TenementLot::k_half_width + k_side_proud,
                    k_course * 0.5F,
                    k_proud * 0.5F + 0.004F),
          Motif::Carthage::k_basalt);
      desc.add_box(
          QVector3D(0.0F, foot + k_course + 0.012F, face + (sign * k_proud * 0.6F)),
          QVector3D(TenementLot::k_half_width + k_side_proud,
                    0.012F,
                    k_proud * 0.6F + 0.004F),
          Motif::Carthage::k_basalt * 1.35F);
    }
    for (const float side : {-1.0F, 1.0F}) {
      desc.add_box(
          QVector3D(side * (TenementLot::k_half_width + (k_side_proud * 0.5F)),
                    foot + (k_course * 0.5F),
                    (TenementLot::k_front + TenementLot::k_rear) * 0.5F),
          QVector3D(k_side_proud * 0.5F + 0.002F,
                    k_course * 0.5F,
                    (TenementLot::k_front - TenementLot::k_rear) * 0.5F + k_proud),
          Motif::Carthage::k_basalt);
    }
  }
  // Horns on the corners of the roof and cedar beam ends under the parapet:
  // from above, a Punic street is a line of them.
  const Motif::Setter set(desc, state);
  for (const float side : {-1.0F, 1.0F}) {
    for (const float z : {TenementLot::k_front - 0.11F, TenementLot::k_rear + 0.11F}) {
      Motif::add_punic_horns(
          desc, set.at(side * (TenementLot::k_half_width - 0.17F), z), 1.25F);
    }
  }
  Motif::add_punic_beam_ends(desc,
                             -TenementLot::k_half_width + 0.12F,
                             TenementLot::k_half_width - 0.12F,
                             tenement_wall_top(tenement_traits(variant)) - 0.10F,
                             TenementLot::k_front,
                             1.0F,
                             0.24F);
  return desc;
}

} // namespace Render::GL::Carthage
