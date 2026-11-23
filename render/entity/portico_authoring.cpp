#include "render/entity/portico_authoring.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <string>

#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/lot_parts.h"
#include "render/entity/nation_motifs.h"
#include "render/entity/town_roof.h"

namespace Render::GL {
namespace {

using MarketLot::k_half;

constexpr float k_pi = 3.14159265F;
constexpr float k_floor = 0.08F;
constexpr float k_shop_front = 0.30F;
constexpr float k_column_z = 1.14F;
constexpr float k_column_height = 0.88F;
constexpr float k_entablature = 0.16F;
constexpr float k_gallery_height = 0.62F;
// How far a basilica's hall rises over its aisle roof, for the clerestory.
constexpr float k_clerestory = 0.62F;
constexpr float k_aisle_roof_rise = 0.20F;
constexpr float k_wall = 0.08F;
constexpr QVector3D k_void{0.11F, 0.095F, 0.08F};
constexpr QVector3D k_charred{0.15F, 0.12F, 0.10F};

class PorticoBuilder {
public:
  PorticoBuilder(BuildingArchetypeDesc& desc,
                 const PorticoLook& look,
                 BuildingState state,
                 int variant)
      : m_parts(desc)
      , m_desc(desc)
      , m_look(look)
      , m_state(state)
      , m_variant(variant)
      , m_basilica(variant == k_portico_basilica_variant)
      , m_two_storey(variant == 1 || m_basilica) {}

  void build() {
    add_platform();
    if (m_state == BuildingState::Destroyed) {
      add_ruin();
      return;
    }
    add_shops();
    add_colonnade(k_floor, k_column_height, 0);
    const float first_top = k_floor + k_column_height + k_entablature;
    const float gallery_top = first_top + k_gallery_height + 0.10F;
    if (m_basilica) {
      // The hall keeps its own roof; the walk in front of it is an aisle
      // roofed lower, so the hall's upper windows clear it.
      add_gallery(first_top);
      add_clerestory(gallery_top);
      add_roof(gallery_top, k_shop_front, k_column_z + 0.16F, true);
      add_roof(
          gallery_top + k_clerestory, -k_half - 0.03F, k_shop_front + 0.07F, false);
    } else if (m_two_storey) {
      add_gallery(first_top);
      add_roof(gallery_top, -k_half - 0.03F, k_column_z + 0.16F, false);
    } else {
      add_roof(first_top, -k_half - 0.03F, k_column_z + 0.16F, false);
    }
    add_ruin();
  }

private:
  [[nodiscard]] auto intact() const -> bool { return m_state == BuildingState::Normal; }

  [[nodiscard]] auto roll(int salt) const -> float {
    return decay_hash((m_variant * 613) + (salt * 97) + 7);
  }

  // Fire takes the right-hand bay first.
  [[nodiscard]] auto standing(float x) const -> bool { return intact() || x < 0.3F; }

  void add_platform() {
    BuildingPartLabel const label(m_desc, "stylobate");
    m_parts.box(-k_half, k_half, 0.0F, 0.04F, 1.22F, k_half, m_look.stone_dark);
    m_parts.box(-k_half, k_half, 0.0F, k_floor, -k_half, 1.26F, m_look.stone);
    m_parts.box(-k_half,
                k_half,
                k_floor,
                k_floor + 0.006F,
                k_shop_front,
                1.02F,
                m_look.marble * 0.94F);
  }

  void add_column(float x, float base, float height, int salt) {
    const float top = base + height;
    if (!standing(x)) {
      // A broken drum left on its base.
      m_parts.box(x - 0.075F,
                  x + 0.075F,
                  base,
                  base + 0.035F,
                  k_column_z - 0.075F,
                  k_column_z + 0.075F,
                  m_look.stone_shade);
      m_parts.post(x,
                   k_column_z,
                   base + 0.035F,
                   base + (height * (0.22F + (0.3F * roll(salt)))),
                   0.052F,
                   m_look.marble * 0.92F);
      return;
    }
    if (m_look.round_columns) {
      m_parts.box(x - 0.075F,
                  x + 0.075F,
                  base,
                  base + 0.035F,
                  k_column_z - 0.075F,
                  k_column_z + 0.075F,
                  m_look.stone_shade);
      m_parts.post(x, k_column_z, base + 0.035F, base + 0.06F, 0.064F, m_look.marble);
      m_parts.post(x,
                   k_column_z,
                   base + 0.06F,
                   top - 0.07F,
                   0.052F,
                   weathered(m_look.marble, salt, 0.04F));
      m_parts.post(x, k_column_z, top - 0.07F, top - 0.04F, 0.066F, m_look.marble);
      m_parts.box(x - 0.082F,
                  x + 0.082F,
                  top - 0.04F,
                  top,
                  k_column_z - 0.082F,
                  k_column_z + 0.082F,
                  m_look.marble);
    } else {
      m_parts.box(x - 0.07F,
                  x + 0.07F,
                  base,
                  top - 0.10F,
                  k_column_z - 0.07F,
                  k_column_z + 0.07F,
                  weathered(m_look.stone, salt, 0.04F));
      m_parts.slot(x - 0.078F,
                   x + 0.078F,
                   top - 0.10F,
                   top - 0.045F,
                   k_column_z - 0.078F,
                   k_column_z + 0.078F,
                   k_lot_accent_slot);
      m_parts.box(x - 0.09F,
                  x + 0.09F,
                  top - 0.045F,
                  top,
                  k_column_z - 0.09F,
                  k_column_z + 0.09F,
                  m_look.stone_shade);
    }
  }

  void add_colonnade(float base, float height, int salt) {
    BuildingPartLabel const label(m_desc, "colonnade");
    for (const float x : {-0.94F, 0.0F, 0.94F}) {
      add_column(x, base, height, salt + static_cast<int>(x * 10.0F));
    }
    const float top = base + height;
    const float right = intact() ? k_half : 0.3F;
    // Architrave, a painted frieze, and the cornice over it.
    m_parts.box(-k_half,
                right,
                top,
                top + 0.065F,
                k_column_z - 0.085F,
                k_column_z + 0.085F,
                m_look.marble);
    m_parts.slot(-k_half,
                 right,
                 top + 0.065F,
                 top + 0.115F,
                 k_column_z - 0.075F,
                 k_column_z + 0.075F,
                 k_lot_accent_slot);
    m_parts.box(-k_half,
                right,
                top + 0.115F,
                top + k_entablature,
                k_column_z - 0.10F,
                k_column_z + 0.115F,
                m_look.stone);
    if (m_look.round_columns && intact()) {
      for (int tooth = 0; tooth < 11; ++tooth) {
        const float x = -1.30F + (0.26F * static_cast<float>(tooth));
        m_parts.box(x - 0.03F,
                    x + 0.03F,
                    top + 0.085F,
                    top + 0.115F,
                    k_column_z + 0.075F,
                    k_column_z + 0.10F,
                    m_look.stone_shade);
      }
    }
  }

  // The shops behind the walk: a plastered range with two doors to a lot.
  void add_shops() {
    BuildingPartLabel const label(m_desc, "shops");
    const float top = k_floor + k_column_height + k_entablature +
                      (m_two_storey ? k_gallery_height + 0.10F : 0.0F) +
                      (m_basilica ? k_clerestory : 0.0F);
    const float inner = k_half - k_wall;
    // The range stands on the platform: sharing its outer faces would tie.
    m_parts.box(
        -inner, inner, k_floor, top, -k_half + k_wall, k_shop_front - k_wall, k_void);
    m_parts.slot(
        -k_half, k_half, k_floor, top, -k_half, -k_half + k_wall, k_lot_wall_slot);
    for (const float side : {-1.0F, 1.0F}) {
      m_parts.slot(side > 0.0F ? inner : -k_half,
                   side > 0.0F ? k_half : -inner,
                   k_floor,
                   top,
                   -k_half + k_wall,
                   k_shop_front,
                   k_lot_wall_slot);
    }
    // The back of the range faces a street too: pilasters, a painted dado
    // and a string course, not sixty paces of bare plaster.
    {
      const float back = -k_half;
      m_parts.slot(-k_half,
                   k_half,
                   k_floor,
                   k_floor + 0.30F,
                   back - 0.010F,
                   back,
                   k_lot_accent_slot);
      m_parts.box(-k_half,
                  k_half,
                  k_floor + 0.30F,
                  k_floor + 0.33F,
                  back - 0.022F,
                  back,
                  m_look.stone);
      // Between the hall's windows, never across them.
      for (const float x : {-1.345F, -0.70F, 0.0F, 0.70F, 1.345F}) {
        m_parts.box(x - 0.06F,
                    x + 0.06F,
                    k_floor + 0.33F,
                    top - 0.05F,
                    back - 0.026F,
                    back,
                    weathered(m_look.stone, static_cast<int>(x * 9.0F) + 61, 0.03F));
      }
      m_parts.box(
          -k_half, k_half, top - 0.05F, top, back - 0.035F, back, m_look.stone_shade);
    }
    // The shop front: piers, lintels, and a dado in the house colour.
    const float door_top = k_floor + 0.72F;
    const float dado = k_floor + 0.34F;
    constexpr float k_doors[2][2] = {{-0.92F, -0.42F}, {0.42F, 0.92F}};
    float cursor = -inner;
    const auto pier = [&](float x0, float x1) {
      m_parts.slot(x0,
                   x1,
                   k_floor,
                   dado,
                   k_shop_front - k_wall,
                   k_shop_front,
                   k_lot_accent_slot);
      m_parts.slot(
          x0, x1, dado, top, k_shop_front - k_wall, k_shop_front, k_lot_wall_slot);
    };
    for (const auto& door : k_doors) {
      pier(cursor, door[0]);
      m_parts.slot(door[0],
                   door[1],
                   door_top,
                   top,
                   k_shop_front - k_wall,
                   k_shop_front,
                   k_lot_wall_slot);
      {
        BuildingPartMaterial const timber(m_desc, k_building_material_wood);
        m_parts.box(door[0] - 0.04F,
                    door[1] + 0.04F,
                    door_top,
                    door_top + 0.05F,
                    k_shop_front - 0.02F,
                    k_shop_front + 0.018F,
                    m_look.timber_dark);
        // One leaf stands open.
        m_parts.box(door[0],
                    door[0] + 0.02F,
                    k_floor,
                    door_top,
                    k_shop_front - 0.26F,
                    k_shop_front - 0.02F,
                    m_look.timber);
      }
      m_parts.box(door[0],
                  door[1],
                  k_floor,
                  k_floor + 0.012F,
                  k_shop_front - k_wall,
                  k_shop_front + 0.02F,
                  m_look.stone_dark);
      cursor = door[1];
    }
    pier(cursor, inner);
    if (m_two_storey) {
      // The upper range looks onto the gallery through a row of windows.
      const float sill = k_floor + k_column_height + k_entablature + 0.24F;
      for (const float x : {-0.94F, -0.31F, 0.31F, 0.94F}) {
        m_parts.box(x - 0.10F,
                    x + 0.10F,
                    sill,
                    sill + 0.28F,
                    k_shop_front - 0.012F,
                    k_shop_front + 0.004F,
                    k_void);
        m_parts.box(x - 0.13F,
                    x + 0.13F,
                    sill - 0.03F,
                    sill,
                    k_shop_front,
                    k_shop_front + 0.03F,
                    m_look.stone);
      }
    }
    if (intact()) {
      // Wares put out under the walk.
      m_desc.add_cylinder(QVector3D(-0.20F, k_floor, k_shop_front + 0.12F),
                          QVector3D(-0.20F, k_floor + 0.20F, k_shop_front + 0.12F),
                          0.06F,
                          weathered(m_look.tile, 3, 0.08F));
      m_desc.add_cylinder(QVector3D(-0.05F, k_floor, k_shop_front + 0.10F),
                          QVector3D(-0.05F, k_floor + 0.15F, k_shop_front + 0.10F),
                          0.05F,
                          weathered(m_look.tile_light, 5, 0.08F));
      {
        BuildingPartMaterial const timber(m_desc, k_building_material_wood);
        m_parts.box(0.02F,
                    0.38F,
                    k_floor + 0.14F,
                    k_floor + 0.165F,
                    k_shop_front + 0.03F,
                    k_shop_front + 0.19F,
                    m_look.timber);
        m_parts.box(0.05F,
                    0.09F,
                    k_floor,
                    k_floor + 0.14F,
                    k_shop_front + 0.05F,
                    k_shop_front + 0.17F,
                    m_look.timber_dark);
        m_parts.box(0.31F,
                    0.35F,
                    k_floor,
                    k_floor + 0.14F,
                    k_shop_front + 0.05F,
                    k_shop_front + 0.17F,
                    m_look.timber_dark);
      }
      m_desc.add_palette_box(QVector3D(0.0F, door_top + 0.16F, k_shop_front + 0.006F),
                             QVector3D(0.12F, 0.045F, 0.008F),
                             k_lot_team_slot);
      if (m_variant == 2) {
        // Cloth hung between the columns against the afternoon sun.
        BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
        m_parts.slot(-0.88F,
                     -0.06F,
                     k_floor + 0.46F,
                     k_floor + k_column_height - 0.02F,
                     k_column_z - 0.012F,
                     k_column_z,
                     k_lot_cloth_slot,
                     k_building_material_cloth);
      }
    }
  }

  // The upper walk of a two-storey stoa: shorter columns behind a balustrade.
  void add_gallery(float base) {
    BuildingPartLabel const label(m_desc, "gallery");
    const float right = intact() ? k_half : 0.3F;
    m_parts.box(-k_half,
                right,
                base,
                base + 0.03F,
                k_shop_front,
                k_column_z + 0.10F,
                m_look.stone_shade);
    for (const float x : {-0.94F, 0.0F, 0.94F}) {
      if (!standing(x)) {
        continue;
      }
      if (m_look.round_columns) {
        m_parts.post(x,
                     k_column_z,
                     base + 0.03F,
                     base + k_gallery_height - 0.03F,
                     0.042F,
                     weathered(m_look.marble, static_cast<int>(x * 7.0F) + 31, 0.04F));
        m_parts.box(x - 0.066F,
                    x + 0.066F,
                    base + k_gallery_height - 0.03F,
                    base + k_gallery_height,
                    k_column_z - 0.066F,
                    k_column_z + 0.066F,
                    m_look.marble);
      } else {
        m_parts.box(x - 0.055F,
                    x + 0.055F,
                    base + 0.03F,
                    base + k_gallery_height,
                    k_column_z - 0.055F,
                    k_column_z + 0.055F,
                    m_look.stone);
      }
    }
    // Balustrade between the upper columns.
    m_parts.box(-k_half,
                right,
                base + 0.21F,
                base + 0.235F,
                k_column_z - 0.02F,
                k_column_z + 0.02F,
                m_look.stone);
    for (int post = 0; post < 12; ++post) {
      const float x = -1.32F + (0.24F * static_cast<float>(post));
      if (x > right) {
        break;
      }
      m_parts.box(x - 0.012F,
                  x + 0.012F,
                  base + 0.03F,
                  base + 0.21F,
                  k_column_z - 0.012F,
                  k_column_z + 0.012F,
                  m_look.stone_shade);
    }
    m_parts.box(-k_half,
                right,
                base + k_gallery_height,
                base + k_gallery_height + 0.10F,
                k_column_z - 0.085F,
                k_column_z + 0.10F,
                m_look.marble);
  }

  void add_tiled_roof(float eave, float rear, float front, bool lean_to) {
    const float rise = lean_to ? k_aisle_roof_rise : 0.40F;
    add_pantile_roof(m_desc,
                     PantileRoof{.x0 = -k_half,
                                 .x1 = intact() ? k_half : 0.3F,
                                 .eave_y = eave,
                                 .front_z = front,
                                 .rear_z = rear,
                                 .rise = rise,
                                 .tile = m_look.tile,
                                 .tile_light = m_look.tile_light,
                                 .tile_dark = m_look.tile_dark,
                                 .cover_pitch = 0.20F,
                                 .lean_to = lean_to});
    if (intact() || lean_to) {
      return;
    }
    // What the fire left of the right-hand bay: charred rafters.
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    const float ridge_z = (front + rear) * 0.5F;
    const float half_depth = (front - rear) * 0.5F;
    const float slope = std::sqrt((half_depth * half_depth) + (rise * rise));
    const float angle = std::atan2(rise, half_depth) * 180.0F / k_pi;
    for (const float x : {0.52F, 0.86F, 1.20F}) {
      for (const float side : {-1.0F, 1.0F}) {
        if (roll(static_cast<int>(x * 10.0F) + (side > 0.0F ? 3 : 9)) < 0.3F) {
          continue;
        }
        m_desc.add_rotated_box(
            QVector3D(x, eave + (rise * 0.5F), ridge_z + (side * half_depth * 0.5F)),
            QVector3D(0.020F, 0.018F, slope * 0.5F),
            QVector3D(side * angle, 0.0F, 0.0F),
            k_charred);
      }
    }
  }

  // A flat roof from rear to front with a parapet and stepped merlons along
  // its front edge; `aisle` leaves off the rear parapet and the roof clutter.
  void add_flat_roof(float eave, float rear, float front, bool aisle) {
    const float right = intact() ? k_half : 0.3F;
    m_parts.box(-k_half, right, eave, eave + 0.03F, rear, front, m_look.deck);
    const float deck = eave + 0.03F;
    const float parapet = deck + (aisle ? 0.10F : 0.15F);
    m_parts.slot(-k_half, right, deck, parapet, front - 0.07F, front, k_lot_wall_slot);
    if (!aisle) {
      m_parts.slot(-k_half, right, deck, parapet, rear, rear + 0.07F, k_lot_wall_slot);
    }
    m_parts.slot(-k_half,
                 right,
                 eave - 0.05F,
                 eave - 0.01F,
                 front,
                 front + 0.012F,
                 k_lot_accent_slot);
    for (const float x : {-k_half + 0.09F, -0.47F, 0.47F, k_half - 0.09F}) {
      if (x > right) {
        continue;
      }
      m_parts.slot(x - 0.075F,
                   x + 0.075F,
                   parapet,
                   parapet + 0.055F,
                   front - 0.07F,
                   front,
                   k_lot_wall_slot);
      m_parts.slot(x - 0.04F,
                   x + 0.04F,
                   parapet + 0.055F,
                   parapet + 0.105F,
                   front - 0.07F,
                   front,
                   k_lot_wall_slot);
    }
    if (intact() && !aisle && !m_basilica) {
      m_desc.add_cylinder(QVector3D(0.7F, deck, -0.5F),
                          QVector3D(0.7F, deck + 0.16F, -0.5F),
                          0.07F,
                          weathered(m_look.tile, 9, 0.08F));
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_parts.box(-0.9F, -0.3F, deck, deck + 0.012F, -0.9F, -0.1F, m_look.deck * 0.9F);
    }
  }

  void add_roof(float eave, float rear, float front, bool lean_to) {
    BuildingPartLabel const label(m_desc, lean_to ? "aisle_roof" : "roof");
    if (m_look.tiled_roof) {
      add_tiled_roof(eave, rear, front, lean_to);
    } else {
      add_flat_roof(
          eave, lean_to ? rear : -k_half, lean_to ? front - 0.06F : front, lean_to);
    }
  }

  // The hall's upper windows, over the aisle roof in front and high in the
  // back wall, and the band of paint under them.
  void add_clerestory(float base) {
    BuildingPartLabel const label(m_desc, "clerestory");
    const float right = intact() ? k_half : 0.3F;
    const float sill = base + k_aisle_roof_rise + 0.14F;
    const float head = base + k_clerestory - 0.10F;
    for (const float face : {k_shop_front, -k_half}) {
      const float out = face > 0.0F ? 1.0F : -1.0F;
      m_parts.slot(-k_half,
                   right,
                   sill - 0.07F,
                   sill - 0.035F,
                   std::min(face, face + (out * 0.012F)),
                   std::max(face, face + (out * 0.012F)),
                   k_lot_accent_slot);
      for (const float x : {-1.05F, -0.35F, 0.35F, 1.05F}) {
        if (x > right) {
          continue;
        }
        m_parts.box(x - 0.13F,
                    x + 0.13F,
                    sill,
                    head,
                    std::min(face - (out * 0.010F), face + (out * 0.006F)),
                    std::max(face - (out * 0.010F), face + (out * 0.006F)),
                    k_void);
        m_parts.box(x - 0.16F,
                    x + 0.16F,
                    sill - 0.03F,
                    sill,
                    std::min(face, face + (out * 0.03F)),
                    std::max(face, face + (out * 0.03F)),
                    m_look.marble);
        m_parts.box(x - 0.16F,
                    x + 0.16F,
                    head,
                    head + 0.03F,
                    std::min(face, face + (out * 0.025F)),
                    std::max(face, face + (out * 0.025F)),
                    m_look.marble);
      }
    }
  }

  void add_ruin() {
    if (m_state == BuildingState::Destroyed) {
      BuildingPartLabel const label(m_desc, "ruin");
      int salt = 0;
      for (const float x : {-0.94F, 0.0F, 0.94F}) {
        ++salt;
        m_parts.post(x,
                     k_column_z,
                     k_floor,
                     k_floor + 0.14F + (0.34F * roll(salt)),
                     0.052F,
                     m_look.marble * 0.9F);
      }
      for (int strip = 0; strip < 6; ++strip) {
        const float x0 = -k_half + (0.468F * static_cast<float>(strip));
        m_parts.slot(x0,
                     x0 + 0.468F,
                     k_floor,
                     k_floor + 0.18F + (0.46F * roll(strip + 20)),
                     -k_half,
                     -k_half + k_wall,
                     k_lot_wall_slot);
      }
    }
    add_ruin_dressing(m_desc,
                      RuinDressing{.center = QVector3D(0.0F, 0.0F, 0.0F),
                                   .extent = QVector3D(1.2F, 0.0F, 1.2F),
                                   .stone = m_look.stone,
                                   .stone_dark = m_look.stone_dark,
                                   .timber = m_look.timber_dark,
                                   .ground_y = k_floor,
                                   .apron_y = 0.02F,
                                   .scale = 1.2F,
                                   .seed = 419 + (m_variant * 7)});
  }

  LotParts m_parts;
  BuildingArchetypeDesc& m_desc;
  const PorticoLook& m_look;
  BuildingState m_state;
  int m_variant;
  bool m_basilica;
  bool m_two_storey;
};

} // namespace

auto roman_portico_look() -> const PorticoLook& {
  static const PorticoLook k_look{
      .name_prefix = "roman_portico_variant_",
      .round_columns = true,
      .tiled_roof = true,
      .stone = QVector3D(0.80F, 0.78F, 0.71F),
      .stone_shade = QVector3D(0.70F, 0.68F, 0.62F),
      .stone_dark = QVector3D(0.50F, 0.49F, 0.45F),
      .marble = QVector3D(0.90F, 0.88F, 0.82F),
      .timber = QVector3D(0.45F, 0.34F, 0.24F),
      .timber_dark = QVector3D(0.29F, 0.24F, 0.20F),
      .tile = QVector3D(0.68F, 0.32F, 0.20F),
      .tile_light = QVector3D(0.81F, 0.43F, 0.26F),
      .tile_dark = QVector3D(0.43F, 0.27F, 0.21F),
      .deck = QVector3D(0.66F, 0.47F, 0.38F),
  };
  return k_look;
}

auto carthage_portico_look() -> const PorticoLook& {
  static const PorticoLook k_look{
      .name_prefix = "carthage_portico_variant_",
      .round_columns = false,
      .tiled_roof = false,
      .stone = QVector3D(0.86F, 0.79F, 0.65F),
      .stone_shade = QVector3D(0.77F, 0.68F, 0.53F),
      .stone_dark = QVector3D(0.48F, 0.42F, 0.34F),
      .marble = QVector3D(0.90F, 0.85F, 0.74F),
      .timber = QVector3D(0.42F, 0.33F, 0.24F),
      .timber_dark = QVector3D(0.26F, 0.21F, 0.17F),
      .tile = QVector3D(0.60F, 0.41F, 0.30F),
      .tile_light = QVector3D(0.70F, 0.50F, 0.36F),
      .tile_dark = QVector3D(0.40F, 0.28F, 0.21F),
      .deck = QVector3D(0.74F, 0.66F, 0.52F),
  };
  return k_look;
}

auto build_portico_desc(const PorticoLook& look,
                        BuildingState state,
                        int variant) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc(std::string(look.name_prefix) + std::to_string(variant));
  PorticoBuilder(desc, look, state, variant).build();
  const Motif::Setter set(desc, state);
  if (look.tiled_roof) {
    // Rome: palmettes along the ridge, two to a lot.
    const QVector3D ridge = set.highest(-0.05F, 0.05F, -k_half, k_shop_front);
    for (const float x : {-0.70F, 0.70F}) {
      Motif::add_roman_acroterion(desc, set.at(x, ridge.z()), 1.7F, true);
    }
  } else {
    // Carthage: horns over the walk, and on a basilica the disc and crescent
    // standing on the hall's roof.
    Motif::add_punic_horns(desc, set.highest(-0.03F, 0.03F, 0.95F, 1.26F), 1.4F);
    if (variant == k_portico_basilica_variant) {
      Motif::add_punic_standard(desc, set.at(0.0F, -0.55F), 1.0F);
    }
  }
  return desc;
}

} // namespace Render::GL
