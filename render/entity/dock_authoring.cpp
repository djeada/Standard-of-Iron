#include "render/entity/dock_authoring.h"

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

constexpr float k_quay = 0.10F;
constexpr float k_shed_front = -0.18F;
constexpr float k_shed_height = 0.92F;
constexpr float k_wall = 0.08F;

constexpr float k_keel = -0.62F;

constexpr float k_waterline = -0.32F;
constexpr QVector3D k_void{0.11F, 0.095F, 0.08F};
constexpr QVector3D k_rope{0.52F, 0.43F, 0.28F};
constexpr QVector3D k_sack{0.74F, 0.64F, 0.46F};
constexpr QVector3D k_charred{0.15F, 0.12F, 0.10F};
constexpr QVector3D k_eye{0.92F, 0.90F, 0.84F};

class DockBuilder {
public:
  DockBuilder(BuildingArchetypeDesc& desc,
              const DockLook& look,
              BuildingState state,
              int variant)
      : m_parts(desc)
      , m_desc(desc)
      , m_look(look)
      , m_state(state)
      , m_variant(variant) {}

  void build() {
    add_quay();
    add_shed();
    if (m_state != BuildingState::Destroyed) {
      add_goods();
      add_crane();
    }
    switch (m_variant) {
    case 0:
      add_ship(0.0F, k_half + 0.14F, 1.72F, 0.46F, true);
      break;
    case 1:
      add_ship(0.42F, k_half + 0.10F, 0.74F, 0.24F, false);
      break;
    default:
      add_net_rack();
      break;
    }
    add_ruin_dressing(m_desc,
                      RuinDressing{.center = QVector3D(0.0F, 0.0F, -0.4F),
                                   .extent = QVector3D(1.15F, 0.0F, 0.9F),
                                   .stone = m_look.stone,
                                   .stone_dark = m_look.stone_dark,
                                   .timber = m_look.timber_dark,
                                   .ground_y = k_quay,
                                   .apron_y = k_quay + 0.01F,
                                   .scale = 1.1F,
                                   .seed = 523 + (m_variant * 5)});
  }

private:
  [[nodiscard]] auto intact() const -> bool { return m_state == BuildingState::Normal; }
  [[nodiscard]] auto destroyed() const -> bool {
    return m_state == BuildingState::Destroyed;
  }
  [[nodiscard]] auto roll(int salt) const -> float {
    return decay_hash((m_variant * 449) + (salt * 83) + 11);
  }

  void add_quay() {
    BuildingPartLabel const label(m_desc, "quay");
    m_parts.box(-k_half, k_half, 0.0F, k_quay, -k_half, k_half - 0.10F, m_look.stone);
    m_parts.box(-k_half,
                k_half,
                k_keel,
                k_quay + 0.012F,
                k_half - 0.10F,
                k_half,
                m_look.stone_shade);
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      for (const float x : {-1.20F, -0.40F, 0.40F, 1.20F}) {
        m_parts.post(
            x,
            k_half + 0.045F,
            k_keel,
            k_quay + 0.10F,
            0.040F,
            weathered(m_look.timber_dark, static_cast<int>(x * 10.0F) + 20, 0.10F));
      }
    }
    for (const float x : {-0.80F, 0.80F}) {
      m_parts.post(
          x, k_half - 0.20F, k_quay, k_quay + 0.13F, 0.055F, m_look.stone_dark);
      m_parts.post(x,
                   k_half - 0.20F,
                   k_quay + 0.13F,
                   k_quay + 0.16F,
                   0.07F,
                   m_look.stone_dark * 1.1F);
    }
  }

  void add_shed() {
    BuildingPartLabel const label(m_desc, "warehouse");
    const float inner = k_half - k_wall;
    if (destroyed()) {
      for (int strip = 0; strip < 6; ++strip) {
        const float x0 = -k_half + (0.468F * static_cast<float>(strip));
        m_parts.slot(x0,
                     x0 + 0.468F,
                     k_quay,
                     k_quay + 0.16F + (0.40F * roll(strip + 30)),
                     -k_half,
                     -k_half + k_wall,
                     k_lot_wall_slot);
      }
      return;
    }
    const float top = k_quay + k_shed_height;
    m_parts.box(
        -inner, inner, k_quay, top, -k_half + k_wall, k_shed_front - k_wall, k_void);
    m_parts.slot(
        -k_half, k_half, k_quay, top, -k_half, -k_half + k_wall, k_lot_wall_slot);
    m_parts.slot(
        -k_half, -inner, k_quay, top, -k_half + k_wall, k_shed_front, k_lot_wall_slot);
    m_parts.slot(inner,
                 k_half,
                 k_quay,
                 intact() ? top : k_quay + (k_shed_height * 0.5F),
                 -k_half + k_wall,
                 k_shed_front,
                 k_lot_wall_slot);

    const float door_top = k_quay + 0.70F;
    m_parts.slot(-inner,
                 -0.52F,
                 k_quay,
                 top,
                 k_shed_front - k_wall,
                 k_shed_front,
                 k_lot_wall_slot);
    m_parts.slot(0.52F,
                 inner,
                 k_quay,
                 intact() ? top : k_quay + (k_shed_height * 0.6F),
                 k_shed_front - k_wall,
                 k_shed_front,
                 k_lot_wall_slot);
    m_parts.slot(-0.52F,
                 0.52F,
                 door_top,
                 top,
                 k_shed_front - k_wall,
                 k_shed_front,
                 k_lot_wall_slot);
    m_parts.slot(-inner,
                 -0.52F,
                 k_quay,
                 k_quay + 0.30F,
                 k_shed_front,
                 k_shed_front + 0.012F,
                 k_lot_accent_slot);
    m_parts.slot(0.52F,
                 inner,
                 k_quay,
                 k_quay + 0.30F,
                 k_shed_front,
                 k_shed_front + 0.012F,
                 k_lot_accent_slot);
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      m_parts.box(-0.58F,
                  0.58F,
                  door_top,
                  door_top + 0.06F,
                  k_shed_front - 0.03F,
                  k_shed_front + 0.02F,
                  m_look.timber_dark);
      m_parts.box(-0.52F,
                  -0.50F,
                  k_quay,
                  door_top,
                  k_shed_front - 0.02F,
                  k_shed_front + 0.30F,
                  m_look.timber);
      m_parts.box(0.50F,
                  0.52F,
                  k_quay,
                  door_top,
                  k_shed_front - 0.02F,
                  k_shed_front + 0.28F,
                  m_look.timber);
    }
    if (intact()) {
      m_desc.add_palette_box(QVector3D(0.0F, door_top + 0.13F, k_shed_front + 0.006F),
                             QVector3D(0.14F, 0.04F, 0.008F),
                             k_lot_team_slot);
    }

    BuildingPartLabel const roof(m_desc, "warehouse_roof");
    const float right = intact() ? k_half : 0.25F;
    if (m_look.tiled_roof) {
      add_pantile_roof(m_desc,
                       PantileRoof{.x0 = -k_half,
                                   .x1 = right,
                                   .eave_y = top,
                                   .front_z = k_shed_front + 0.10F,
                                   .rear_z = -k_half - 0.03F,
                                   .rise = 0.30F,
                                   .tile = m_look.tile,
                                   .tile_light = m_look.tile_light,
                                   .tile_dark = m_look.tile_dark,
                                   .cover_pitch = 0.20F});
    } else {
      m_parts.box(
          -k_half, right, top, top + 0.03F, -k_half, k_shed_front + 0.02F, m_look.deck);
      m_parts.slot(-k_half,
                   right,
                   top + 0.03F,
                   top + 0.16F,
                   k_shed_front - 0.05F,
                   k_shed_front + 0.02F,
                   k_lot_wall_slot);
      m_parts.slot(-k_half,
                   right,
                   top + 0.03F,
                   top + 0.16F,
                   -k_half,
                   -k_half + 0.07F,
                   k_lot_wall_slot);
      m_parts.slot(-k_half,
                   right,
                   top - 0.05F,
                   top - 0.01F,
                   k_shed_front,
                   k_shed_front + 0.012F,
                   k_lot_accent_slot);
    }
    if (!intact()) {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      for (const float x : {0.48F, 0.84F, 1.20F}) {
        m_parts.box(x - 0.02F,
                    x + 0.02F,
                    top - 0.02F,
                    top + 0.02F,
                    -k_half + 0.05F,
                    k_shed_front,
                    k_charred * (0.9F + (0.5F * roll(static_cast<int>(x * 10.0F)))));
      }
    }
  }

  void add_amphora(float x, float z, float base, float scale, int salt) {
    const QVector3D clay = weathered(m_look.clay, salt, 0.10F);
    const float r = 0.055F * scale;
    m_desc.add_cone(QVector3D(x, base + (0.05F * scale), z),
                    QVector3D(x, base, z),
                    r * 0.85F,
                    clay);
    m_parts.post(x, z, base + (0.045F * scale), base + (0.19F * scale), r, clay);
    m_desc.add_cone(QVector3D(x, base + (0.185F * scale), z),
                    QVector3D(x, base + (0.27F * scale), z),
                    r,
                    clay);
    m_parts.post(
        x, z, base + (0.24F * scale), base + (0.31F * scale), r * 0.36F, clay * 0.9F);
  }

  void add_goods() {
    BuildingPartLabel const label(m_desc, "cargo");
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 3; ++col) {
        add_amphora(-1.22F + (0.14F * static_cast<float>(col)),
                    0.10F + (0.15F * static_cast<float>(row)),
                    k_quay,
                    1.0F,
                    (row * 3) + col);
      }
    }
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      m_parts.box(0.86F,
                  1.12F,
                  k_quay,
                  k_quay + 0.20F,
                  0.10F,
                  0.36F,
                  weathered(m_look.timber_light, 3, 0.08F));
      m_parts.box(1.14F,
                  1.36F,
                  k_quay,
                  k_quay + 0.17F,
                  0.12F,
                  0.34F,
                  weathered(m_look.timber, 5, 0.08F));
      m_parts.box(0.94F,
                  1.18F,
                  k_quay + 0.20F,
                  k_quay + 0.36F,
                  0.13F,
                  0.33F,
                  weathered(m_look.timber_light, 7, 0.08F));
    }
    {
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_parts.box(-0.46F, -0.20F, k_quay, k_quay + 0.07F, 0.50F, 0.68F, k_sack);
      m_parts.box(
          -0.42F, -0.16F, k_quay + 0.07F, k_quay + 0.13F, 0.44F, 0.62F, k_sack * 0.94F);
      m_parts.box(-0.16F, 0.08F, k_quay, k_quay + 0.07F, 0.46F, 0.64F, k_sack * 0.97F);
    }
    m_parts.post(0.20F, 1.02F, k_quay, k_quay + 0.035F, 0.10F, k_rope);
    m_parts.post(0.20F, 1.02F, k_quay + 0.035F, k_quay + 0.06F, 0.075F, k_rope * 0.92F);
  }

  void add_crane() {
    if (!intact()) {
      return;
    }
    BuildingPartLabel const label(m_desc, "crane");
    BuildingPartMaterial const timber(m_desc, k_building_material_wood);
    const QVector3D foot(1.06F, k_quay, 0.86F);
    const QVector3D head(1.06F, k_quay + 1.36F, 0.86F);
    const QVector3D tip(0.74F, k_quay + 1.10F, k_half + 0.50F);
    m_parts.cylinder(foot, head, 0.035F, m_look.timber_dark);
    m_parts.cylinder(foot + QVector3D(-0.26F, 0.0F, -0.20F),
                     foot + QVector3D(0.0F, 0.62F, 0.0F),
                     0.022F,
                     m_look.timber);
    m_parts.cylinder(foot + QVector3D(0.16F, 0.0F, -0.26F),
                     foot + QVector3D(0.0F, 0.62F, 0.0F),
                     0.022F,
                     m_look.timber);
    m_parts.cylinder(foot + QVector3D(0.0F, 0.52F, 0.0F), tip, 0.028F, m_look.timber);
    m_parts.cylinder(head, tip, 0.007F, k_rope);
    m_parts.cylinder(tip, tip - QVector3D(0.0F, 0.46F, 0.0F), 0.007F, k_rope);
    m_parts.box(tip.x() - 0.07F,
                tip.x() + 0.07F,
                tip.y() - 0.62F,
                tip.y() - 0.46F,
                tip.z() - 0.07F,
                tip.z() + 0.07F,
                m_look.timber_light);
  }

  void add_net_rack() {
    if (!intact()) {
      return;
    }
    BuildingPartLabel const label(m_desc, "net_rack");
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      for (const float x : {-0.30F, 0.46F}) {
        m_parts.post(x, 1.10F, k_quay, k_quay + 0.50F, 0.016F, m_look.timber_dark);
      }
      m_parts.cylinder(QVector3D(-0.34F, k_quay + 0.49F, 1.10F),
                       QVector3D(0.50F, k_quay + 0.49F, 1.10F),
                       0.012F,
                       m_look.timber_dark);
    }
    m_parts.slot(-0.26F,
                 0.42F,
                 k_quay + 0.12F,
                 k_quay + 0.48F,
                 1.094F,
                 1.106F,
                 k_lot_cloth_slot,
                 k_building_material_cloth);
    m_parts.post(-0.62F,
                 0.96F,
                 k_quay,
                 k_quay + 0.12F,
                 0.09F,
                 weathered(m_look.timber_light, 9, 0.08F));
    m_parts.post(-0.80F,
                 1.10F,
                 k_quay,
                 k_quay + 0.10F,
                 0.08F,
                 weathered(m_look.timber_light, 11, 0.08F));
  }

  void add_ship(
      float centre_x, float quay_gap_z, float half_length, float half_beam, bool ship) {
    BuildingPartLabel const label(m_desc, ship ? "ship" : "boat");
    const float zc = quay_gap_z + half_beam;
    const int sections = ship ? 9 : 5;
    const float step = (2.0F * half_length) / static_cast<float>(sections);
    const float sheer = ship ? 0.16F : 0.07F;
    const float freeboard = ship ? 0.37F : 0.17F;
    const auto gunwale = [&](float t) {
      return k_waterline + freeboard + (sheer * t * t);
    };
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      for (int index = 0; index < sections; ++index) {
        const float x =
            centre_x - half_length + (step * (static_cast<float>(index) + 0.5F));
        const float t = (x - centre_x) / half_length;
        const float beam = half_beam * std::sqrt(std::max(0.06F, 1.0F - (t * t)));
        const float rail = gunwale(t);
        const float deck = rail - 0.055F;
        const float keel = k_keel + (0.20F * t * t);
        const QVector3D plank = weathered(m_look.hull, index + 40, 0.08F);
        if (destroyed()) {

          if ((index % 2) == 0) {
            m_parts.box(x - (step * 0.2F),
                        x + (step * 0.2F),
                        keel,
                        keel + 0.30F + (0.20F * roll(index)),
                        zc - beam,
                        zc + beam,
                        k_charred * 1.3F);
          }
          continue;
        }
        m_parts.box(x - (step * 0.5F),
                    x + (step * 0.5F),
                    keel,
                    deck,
                    zc - beam,
                    zc + beam,
                    plank);
        for (const float side : {-1.0F, 1.0F}) {
          const float outer = zc + (side * beam);
          const float inner = outer - (side * 0.035F);
          m_parts.box(x - (step * 0.5F),
                      x + (step * 0.5F),
                      deck,
                      rail,
                      std::min(outer, inner),
                      std::max(outer, inner),
                      plank * 1.08F);
          m_parts.box(x - (step * 0.5F),
                      x + (step * 0.5F),
                      rail,
                      rail + 0.022F,
                      std::min(outer, inner) - 0.006F,
                      std::max(outer, inner) + 0.006F,
                      m_look.timber_light);
        }

        {
          BuildingPartMaterial const paint(m_desc, k_building_material_wood);
          for (const float side : {-1.0F, 1.0F}) {
            const float face = zc + (side * (beam + 0.006F));
            m_desc.add_palette_box(QVector3D(x, rail - 0.085F, face),
                                   QVector3D(step * 0.5F, 0.020F, 0.006F),
                                   k_lot_accent_slot);
          }
        }
      }
      if (destroyed()) {
        return;
      }
      const float bow = centre_x + half_length;
      const float stern = centre_x - half_length;
      const float post = ship ? 0.30F : 0.14F;

      m_desc.add_rotated_box(QVector3D(bow + 0.03F, gunwale(1.0F) + (post * 0.45F), zc),
                             QVector3D(0.045F, post, 0.045F),
                             QVector3D(0.0F, 0.0F, -18.0F),
                             m_look.hull * 0.9F);
      m_desc.add_rotated_box(
          QVector3D(stern - 0.03F, gunwale(1.0F) + (post * 0.55F), zc),
          QVector3D(0.045F, post * 1.15F, 0.045F),
          QVector3D(0.0F, 0.0F, 16.0F),
          m_look.hull * 0.9F);
      if (ship) {
        if (m_look.horse_prow) {
          m_desc.add_rotated_box(QVector3D(bow + 0.19F, gunwale(1.0F) + 0.62F, zc),
                                 QVector3D(0.11F, 0.055F, 0.045F),
                                 QVector3D(0.0F, 0.0F, -32.0F),
                                 m_look.timber_light);
          m_parts.box(bow + 0.06F,
                      bow + 0.14F,
                      gunwale(1.0F) + 0.56F,
                      gunwale(1.0F) + 0.74F,
                      zc - 0.012F,
                      zc + 0.012F,
                      m_look.hull * 0.8F);
        } else {

          m_desc.add_rotated_box(QVector3D(stern + 0.02F, gunwale(1.0F) + 0.70F, zc),
                                 QVector3D(0.04F, 0.13F, 0.04F),
                                 QVector3D(0.0F, 0.0F, -38.0F),
                                 m_look.timber_light);
        }
      }
    }
    const float mid_deck = gunwale(0.0F) - 0.055F;
    if (!ship) {

      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      m_parts.box(centre_x - 0.5F,
                  centre_x + 0.5F,
                  mid_deck + 0.02F,
                  mid_deck + 0.035F,
                  zc - 0.10F,
                  zc - 0.07F,
                  m_look.timber_light);
      m_parts.box(centre_x - 0.46F,
                  centre_x + 0.54F,
                  mid_deck + 0.02F,
                  mid_deck + 0.035F,
                  zc + 0.05F,
                  zc + 0.08F,
                  m_look.timber_light);
      m_parts.post(centre_x + 0.2F,
                   zc,
                   mid_deck,
                   mid_deck + 0.07F,
                   0.07F,
                   weathered(m_look.timber_light, 13, 0.08F));
      if (intact()) {
        m_parts.slot(centre_x - 0.38F,
                     centre_x - 0.06F,
                     mid_deck,
                     mid_deck + 0.03F,
                     zc - 0.16F,
                     zc + 0.16F,
                     k_lot_cloth_slot,
                     k_building_material_cloth);
      }
      return;
    }

    const float mast_x = centre_x + 0.12F;
    const float mast_top = intact() ? k_quay + 2.05F : k_quay + 0.95F;
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      m_parts.post(mast_x, zc, mid_deck, mast_top, 0.034F, m_look.timber);

      const float stern = centre_x - half_length;
      m_parts.box(stern + 0.36F,
                  stern + 0.80F,
                  mid_deck,
                  mid_deck + 0.24F,
                  zc - 0.22F,
                  zc + 0.22F,
                  m_look.timber);
      m_parts.box(stern + 0.33F,
                  stern + 0.83F,
                  mid_deck + 0.24F,
                  mid_deck + 0.265F,
                  zc - 0.25F,
                  zc + 0.25F,
                  m_look.timber_dark);

      for (const float side : {-1.0F, 1.0F}) {
        m_desc.add_rotated_box(QVector3D(stern + 0.30F,
                                         k_waterline + 0.14F,
                                         zc + (side * (half_beam * 0.62F))),
                               QVector3D(0.012F, 0.36F, 0.035F),
                               QVector3D(0.0F, 0.0F, 22.0F),
                               m_look.timber_light);
      }

      m_parts.box(centre_x - 0.62F,
                  centre_x - 0.40F,
                  k_quay + 0.012F,
                  k_quay + 0.032F,
                  k_half - 0.16F,
                  zc - (half_beam * 0.55F),
                  m_look.timber_light);
    }
    for (int jar = 0; jar < 4; ++jar) {
      add_amphora(centre_x + 0.50F + (0.14F * static_cast<float>(jar % 2)),
                  zc - 0.10F + (0.16F * static_cast<float>(jar / 2)),
                  mid_deck,
                  0.9F,
                  jar + 60);
    }
    {
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_parts.box(centre_x - 0.36F,
                  centre_x - 0.10F,
                  mid_deck,
                  mid_deck + 0.07F,
                  zc - 0.12F,
                  zc + 0.10F,
                  k_sack);
    }

    const float bow_t = 0.74F;
    const float bow_x = centre_x + (half_length * bow_t);
    const float bow_beam = half_beam * std::sqrt(1.0F - (bow_t * bow_t));
    for (const float side : {-1.0F, 1.0F}) {
      const float face = zc + (side * bow_beam);
      m_parts.cylinder(QVector3D(bow_x, gunwale(bow_t) - 0.10F, face),
                       QVector3D(bow_x, gunwale(bow_t) - 0.10F, face + (side * 0.014F)),
                       0.045F,
                       k_eye);
      m_parts.cylinder(
          QVector3D(bow_x + 0.01F, gunwale(bow_t) - 0.10F, face),
          QVector3D(bow_x + 0.01F, gunwale(bow_t) - 0.10F, face + (side * 0.020F)),
          0.020F,
          k_void);
    }
    if (!intact()) {
      return;
    }
    const float yard_y = mast_top - 0.30F;
    {
      BuildingPartMaterial const timber(m_desc, k_building_material_wood);
      m_parts.cylinder(QVector3D(mast_x - 0.86F, yard_y, zc),
                       QVector3D(mast_x + 0.86F, yard_y, zc),
                       0.020F,
                       m_look.timber_light);
    }
    {
      BuildingPartMaterial const cloth(m_desc, k_building_material_cloth);
      m_desc.add_palette_cylinder(QVector3D(mast_x - 0.80F, yard_y - 0.045F, zc),
                                  QVector3D(mast_x + 0.80F, yard_y - 0.045F, zc),
                                  0.050F,
                                  k_lot_cloth_slot);
      m_desc.add_palette_box(QVector3D(mast_x + 0.07F, mast_top + 0.04F, zc),
                             QVector3D(0.075F, 0.035F, 0.004F),
                             k_lot_team_slot);
    }

    const QVector3D masthead(mast_x, mast_top - 0.04F, zc);
    m_parts.cylinder(masthead,
                     QVector3D(centre_x + half_length, gunwale(1.0F) + 0.30F, zc),
                     0.006F,
                     k_rope);
    m_parts.cylinder(masthead,
                     QVector3D(centre_x - half_length, gunwale(1.0F) + 0.34F, zc),
                     0.006F,
                     k_rope);
    for (const float side : {-1.0F, 1.0F}) {
      m_parts.cylinder(
          masthead,
          QVector3D(mast_x - 0.18F, gunwale(0.0F), zc + (side * (half_beam - 0.02F))),
          0.006F,
          k_rope);
    }
  }

  LotParts m_parts;
  BuildingArchetypeDesc& m_desc;
  const DockLook& m_look;
  BuildingState m_state;
  int m_variant;
};

} // namespace

auto roman_dock_look() -> const DockLook& {
  static const DockLook k_look{
      .name_prefix = "roman_dock_variant_",
      .tiled_roof = true,
      .horse_prow = false,
      .stone = QVector3D(0.74F, 0.72F, 0.66F),
      .stone_shade = QVector3D(0.62F, 0.60F, 0.55F),
      .stone_dark = QVector3D(0.46F, 0.45F, 0.42F),
      .timber = QVector3D(0.45F, 0.34F, 0.24F),
      .timber_light = QVector3D(0.60F, 0.48F, 0.33F),
      .timber_dark = QVector3D(0.29F, 0.24F, 0.20F),
      .hull = QVector3D(0.33F, 0.24F, 0.17F),
      .tile = QVector3D(0.68F, 0.32F, 0.20F),
      .tile_light = QVector3D(0.81F, 0.43F, 0.26F),
      .tile_dark = QVector3D(0.43F, 0.27F, 0.21F),
      .deck = QVector3D(0.66F, 0.47F, 0.38F),
      .clay = QVector3D(0.72F, 0.38F, 0.24F),
  };
  return k_look;
}

auto carthage_dock_look() -> const DockLook& {
  static const DockLook k_look{
      .name_prefix = "carthage_dock_variant_",
      .tiled_roof = false,
      .horse_prow = true,
      .stone = QVector3D(0.82F, 0.75F, 0.61F),
      .stone_shade = QVector3D(0.70F, 0.62F, 0.49F),
      .stone_dark = QVector3D(0.48F, 0.42F, 0.34F),
      .timber = QVector3D(0.42F, 0.33F, 0.24F),
      .timber_light = QVector3D(0.57F, 0.46F, 0.32F),
      .timber_dark = QVector3D(0.26F, 0.21F, 0.17F),
      .hull = QVector3D(0.24F, 0.18F, 0.15F),
      .tile = QVector3D(0.60F, 0.41F, 0.30F),
      .tile_light = QVector3D(0.70F, 0.50F, 0.36F),
      .tile_dark = QVector3D(0.40F, 0.28F, 0.21F),
      .deck = QVector3D(0.74F, 0.66F, 0.52F),
      .clay = QVector3D(0.66F, 0.42F, 0.29F),
  };
  return k_look;
}

auto build_dock_desc(const DockLook& look,
                     BuildingState state,
                     int variant) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc(std::string(look.name_prefix) + std::to_string(variant));
  DockBuilder(desc, look, state, variant).build();

  const Motif::Setter set(desc, state);
  const QVector3D corner = set.at(-1.24F, 1.14F, 0.5F);
  if (look.tiled_roof) {
    Motif::add_roman_tripod_fire(desc, corner, 1.25F);
    Motif::add_roman_raven(
        desc, set.highest(-0.9F, -0.5F, -k_half, k_shed_front), 70.0F, 1.0F);
  } else {
    Motif::add_punic_incense_stand(desc, corner, 1.25F);
    for (const float side : {-1.0F, 1.0F}) {
      Motif::add_punic_horns(
          desc, set.at(side * (k_half - 0.19F), k_shed_front - 0.03F), 1.3F);
    }
  }
  return desc;
}

} // namespace Render::GL
