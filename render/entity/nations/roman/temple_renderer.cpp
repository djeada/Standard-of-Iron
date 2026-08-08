#include "temple_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "../../../render_archetype.h"
#include "../../building_archetype_desc.h"
#include "../../building_decay.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"
#include "../../temple_renderer_common.h"

namespace Render::GL::Roman {
namespace {

struct RomanTemplePalette {
  QVector3D marble{0.94F, 0.92F, 0.85F};
  QVector3D marble_shade{0.81F, 0.78F, 0.70F};
  QVector3D marble_dark{0.58F, 0.55F, 0.48F};
  QVector3D limestone{0.84F, 0.78F, 0.66F};
  QVector3D limestone_dark{0.50F, 0.46F, 0.38F};
  QVector3D mortar{0.56F, 0.52F, 0.45F};
  QVector3D terracotta{0.66F, 0.29F, 0.14F};
  QVector3D terracotta_dark{0.47F, 0.18F, 0.09F};
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D gold{0.72F, 0.53F, 0.20F};
  QVector3D bronze{0.48F, 0.30F, 0.11F};
  QVector3D blue_accent{0.24F, 0.40F, 0.55F};
  QVector3D soot{0.16F, 0.14F, 0.12F};
  QVector3D flame{0.88F, 0.47F, 0.12F};
  QVector3D cedar{0.40F, 0.25F, 0.13F};
  QVector3D verdigris{0.29F, 0.46F, 0.40F};
};

constexpr std::uint8_t k_temple_team_slot = 1;

void add_fluted_column(BuildingArchetypeDesc& desc,
                       const RomanTemplePalette& c,
                       float x,
                       float z,
                       float base_y,
                       float height) {
  constexpr float shaft_radius = 0.058F;

  desc.add_box(QVector3D(x, base_y + 0.018F, z),
               QVector3D(shaft_radius * 1.55F, 0.018F, shaft_radius * 1.55F),
               c.marble_shade);
  desc.add_cylinder(QVector3D(x, base_y + 0.036F, z),
                    QVector3D(x, base_y + 0.070F, z),
                    shaft_radius * 1.22F,
                    c.marble_shade,
                    BuildingStateMask::All,
                    BuildingLODMask::Full);

  desc.add_cylinder(QVector3D(x, base_y + 0.056F, z),
                    QVector3D(x, base_y + height - 0.06F, z),
                    shaft_radius,
                    c.marble);

  for (float const flute : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(x + flute * shaft_radius * 0.92F, base_y + height * 0.5F, z),
                 QVector3D(0.006F, height * 0.42F, shaft_radius * 0.55F),
                 c.marble_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, base_y + height * 0.5F, z + flute * shaft_radius * 0.92F),
                 QVector3D(shaft_radius * 0.55F, height * 0.42F, 0.006F),
                 c.marble_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  desc.add_cylinder(QVector3D(x, base_y + height - 0.07F, z),
                    QVector3D(x, base_y + height - 0.035F, z),
                    shaft_radius * 1.16F,
                    c.marble_shade,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  desc.add_box(QVector3D(x, base_y + height - 0.012F, z),
               QVector3D(shaft_radius * 1.62F, 0.026F, shaft_radius * 1.62F),
               c.marble,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(
        QVector3D(x + side * shaft_radius * 1.30F, base_y + height - 0.046F, z),
        QVector3D(x + side * shaft_radius * 1.30F, base_y + height - 0.020F, z),
        0.020F,
        c.marble_shade,
        k_building_state_mask_intact,
        BuildingLODMask::Full);
  }
}

void add_votive_altar(BuildingArchetypeDesc& desc,
                      const RomanTemplePalette& c,
                      const QVector3D& base) {
  desc.add_box(base + QVector3D(0.0F, 0.030F, 0.0F),
               QVector3D(0.155F, 0.030F, 0.135F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.115F, 0.0F),
               QVector3D(0.115F, 0.058F, 0.098F),
               c.marble_shade,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.190F, 0.0F),
               QVector3D(0.150F, 0.022F, 0.130F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.220F, 0.0F),
               QVector3D(0.085F, 0.012F, 0.070F),
               c.soot,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.228F, 0.0F),
                base + QVector3D(0.0F, 0.330F, 0.0F),
                0.052F,
                c.flame,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
}

void add_roman_temple_ruin(BuildingArchetypeDesc& desc,
                           const RomanTemplePalette& c,
                           float podium_y,
                           float cella_h) {
  constexpr auto k_ruin = BuildingStateMask::Destroyed;

  struct Drum {
    float x;
    float z;
    float yaw;
    float len;
  };
  constexpr std::array<Drum, 6> k_drums{Drum{-0.86F, 0.42F, 18.0F, 0.30F},
                                        Drum{-0.34F, -0.58F, 74.0F, 0.24F},
                                        Drum{0.28F, 0.66F, 122.0F, 0.27F},
                                        Drum{0.94F, -0.30F, 41.0F, 0.22F},
                                        Drum{-1.28F, -0.70F, 96.0F, 0.26F},
                                        Drum{1.18F, 0.52F, 8.0F, 0.20F}};
  for (const auto& drum : k_drums) {
    float const rad = drum.yaw * 3.14159265F / 180.0F;
    QVector3D const axis(std::cos(rad) * drum.len, 0.0F, std::sin(rad) * drum.len);
    QVector3D const mid(drum.x, podium_y + 0.062F, drum.z);
    desc.add_cylinder(mid - axis, mid + axis, 0.058F, c.marble_shade, k_ruin);
  }

  struct Block {
    float x;
    float y;
    float z;
    float yaw;
    float roll;
    float sx;
    float sy;
    float sz;
  };

  constexpr std::array<Block, 7> k_blocks{
      Block{-1.44F, 0.10F, 0.34F, 22.0F, 9.0F, 0.16F, 0.075F, 0.11F},
      Block{-0.62F, 0.09F, -1.16F, 58.0F, -6.0F, 0.13F, 0.065F, 0.13F},
      Block{0.44F, 0.10F, 1.22F, 12.0F, 14.0F, 0.18F, 0.070F, 0.10F},
      Block{1.42F, 0.09F, -0.86F, 81.0F, -11.0F, 0.14F, 0.060F, 0.12F},
      Block{0.10F, 0.09F, -0.34F, 34.0F, 7.0F, 0.15F, 0.070F, 0.12F},
      Block{-0.96F, 0.08F, 0.88F, 66.0F, -8.0F, 0.12F, 0.058F, 0.10F},
      Block{1.02F, 0.10F, 0.16F, 5.0F, 16.0F, 0.19F, 0.080F, 0.13F}};
  int block_index = 0;
  for (const auto& block : k_blocks) {
    bool const on_podium = block_index++ >= 4;
    desc.add_rotated_box(
        QVector3D(block.x, block.y + (on_podium ? podium_y : 0.0F), block.z),
        QVector3D(block.sx, block.sy, block.sz),
        QVector3D(block.roll, block.yaw, 0.0F),
        (block.yaw > 40.0F) ? c.limestone : c.marble_shade,
        k_ruin);
  }

  float const stub_top = podium_y + cella_h;
  constexpr std::array<float, 5> k_stub_z{-0.52F, -0.24F, 0.06F, 0.32F, 0.54F};
  for (std::size_t i = 0; i < k_stub_z.size(); ++i) {
    float const rise = 0.06F + 0.055F * static_cast<float>((i * 3) % 4);
    desc.add_box(QVector3D(1.20F, stub_top + rise * 0.5F, k_stub_z[i]),
                 QVector3D(0.055F, rise * 0.5F, 0.11F),
                 c.marble_shade,
                 k_ruin);
  }
  for (float const side : {-1.0F, 1.0F}) {
    for (int i = 0; i < 4; ++i) {
      float const px = -0.02F + 0.36F * static_cast<float>(i);
      float const rise = 0.05F + 0.05F * static_cast<float>((i + 1) % 3);
      desc.add_box(QVector3D(px, stub_top + rise * 0.5F, side * 0.58F),
                   QVector3D(0.13F, rise * 0.5F, 0.052F),
                   c.marble_shade,
                   k_ruin);
    }
  }

  QVector3D const ash = c.soot * 0.45F + c.limestone * 0.55F;
  QVector3D const ash_dark = c.soot * 0.72F + c.limestone * 0.28F;

  desc.add_box(QVector3D(0.52F, stub_top + 0.010F, 0.0F),
               QVector3D(0.62F, 0.010F, 0.52F),
               ash,
               k_ruin);
  struct Debris {
    float x;
    float z;
    float yaw;
    float sx;
    float sy;
    float sz;
  };
  constexpr std::array<Debris, 5> k_debris{
      Debris{0.24F, -0.22F, 27.0F, 0.15F, 0.055F, 0.12F},
      Debris{0.72F, 0.28F, 63.0F, 0.13F, 0.070F, 0.11F},
      Debris{1.00F, -0.30F, 14.0F, 0.11F, 0.048F, 0.14F},
      Debris{0.36F, 0.34F, 88.0F, 0.14F, 0.062F, 0.10F},
      Debris{0.86F, 0.02F, 45.0F, 0.10F, 0.052F, 0.12F}};
  for (const auto& piece : k_debris) {
    desc.add_rotated_box(QVector3D(piece.x, stub_top + piece.sy + 0.010F, piece.z),
                         QVector3D(piece.sx, piece.sy, piece.sz),
                         QVector3D(0.0F, piece.yaw, 0.0F),
                         (piece.yaw > 50.0F) ? ash_dark : c.marble_shade,
                         k_ruin);
  }

  for (float const sx : {-1.10F, -0.20F, 0.86F}) {
    desc.add_box(QVector3D(sx, podium_y + 0.010F, 0.30F),
                 QVector3D(0.26F, 0.008F, 0.22F),
                 ash,
                 k_ruin,
                 BuildingLODMask::Full);
  }
}

auto build_temple_archetype(BuildingState state) -> RenderArchetype {
  RomanTemplePalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.74F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.42F;
  }

  BuildingArchetypeDesc desc("roman_temple");
  desc.set_full_lod_max_distance(84.0F);

  desc.add_box(
      QVector3D(0.0F, 0.055F, 0.0F), QVector3D(1.38F, 0.055F, 1.10F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.195F, 0.0F), QVector3D(1.33F, 0.085F, 1.05F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, 0.345F, 0.0F), QVector3D(1.29F, 0.065F, 1.01F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, 0.435F, 0.0F), QVector3D(1.25F, 0.038F, 0.97F), c.marble_shade);
  desc.add_box(
      QVector3D(0.0F, 0.478F, 0.0F), QVector3D(1.27F, 0.010F, 0.99F), c.marble);

  float const podium_y = 0.488F;

  for (int course = 0; course < 4; ++course) {
    float const course_y = 0.110F + 0.048F * static_cast<float>(course);
    desc.add_box(QVector3D(0.0F, course_y, 1.035F),
                 QVector3D(1.29F, 0.006F, 0.006F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (float const joint_x : {-0.90F, -0.30F, 0.30F, 0.90F}) {
    desc.add_box(QVector3D(joint_x, 0.170F, 0.0F),
                 QVector3D(0.007F, 0.052F, 1.035F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (int step = 0; step < 6; ++step) {
    float const step_index = static_cast<float>(step);
    float const top = 0.081F * (step_index + 1.0F);
    desc.add_box(QVector3D(-1.700F + 0.078F * step_index, top * 0.5F, 0.0F),
                 QVector3D(0.041F, top * 0.5F, 0.76F),
                 (step % 2 == 0) ? c.marble_shade : c.limestone,
                 k_building_state_mask_intact);
  }
  for (float const cheek : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-1.50F, 0.24F, cheek * 0.815F),
                 QVector3D(0.26F, 0.24F, 0.055F),
                 c.limestone,
                 k_building_state_mask_intact);
  }

  float const column_height = 1.52F * height_multiplier;
  bool const ruined = state == BuildingState::Destroyed;

  int column_index = 0;
  auto snapped_height = [&](int index) {
    if (!ruined) {
      return column_height;
    }
    constexpr std::array<float, 7> k_breaks{
        1.24F, 0.46F, 0.88F, 0.31F, 1.05F, 0.62F, 0.78F};
    return column_height * k_breaks[static_cast<std::size_t>(index) % k_breaks.size()];
  };

  for (float const cz : {-0.76F, -0.26F, 0.26F, 0.76F}) {
    add_fluted_column(desc, c, -1.10F, cz, podium_y, snapped_height(column_index++));
    add_fluted_column(desc, c, -0.56F, cz, podium_y, snapped_height(column_index++));
  }
  for (float const cx : {-0.02F, 0.52F, 1.06F}) {
    for (float const cz : {-0.76F, 0.76F}) {
      add_fluted_column(desc, c, cx, cz, podium_y, snapped_height(column_index++));
    }
  }

  float const cella_h = 1.12F * height_multiplier;
  desc.add_box(QVector3D(0.52F, podium_y + cella_h * 0.5F, 0.0F),
               QVector3D(0.70F, cella_h * 0.5F, 0.60F),
               c.marble_shade);
  desc.add_box(QVector3D(0.52F, podium_y + cella_h + 0.030F, 0.0F),
               QVector3D(0.73F, 0.030F, 0.63F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.52F, podium_y + 0.030F, 0.0F),
               QVector3D(0.73F, 0.030F, 0.63F),
               c.marble,
               k_building_state_mask_intact);

  for (int course = 1; course < 6; ++course) {
    float const course_y = podium_y + cella_h * static_cast<float>(course) / 6.0F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.52F, course_y, side * 0.605F),
                   QVector3D(0.68F, 0.007F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(1.225F, course_y, 0.0F),
                 QVector3D(0.006F, 0.007F, 0.58F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (float const px : {-0.06F, 0.32F, 0.70F, 1.08F}) {
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(px, podium_y + cella_h * 0.5F, side * 0.615F),
                   QVector3D(0.042F, cella_h * 0.47F, 0.018F),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(px, podium_y + cella_h * 0.965F, side * 0.622F),
                   QVector3D(0.058F, cella_h * 0.030F, 0.024F),
                   c.marble,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }
  for (float const pz : {-0.40F, 0.0F, 0.40F}) {
    desc.add_box(QVector3D(1.235F, podium_y + cella_h * 0.5F, pz),
                 QVector3D(0.018F, cella_h * 0.47F, 0.042F),
                 c.marble,
                 k_building_state_mask_intact);
  }

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.52F, podium_y + cella_h * 0.895F, side * 0.612F),
                 QVector3D(0.665F, cella_h * 0.040F, 0.012F),
                 c.cloth_red,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(1.232F, podium_y + cella_h * 0.895F, 0.0F),
               QVector3D(0.012F, cella_h * 0.040F, 0.565F),
               c.cloth_red,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(-0.185F, podium_y + cella_h * 0.34F, 0.0F),
               QVector3D(0.032F, cella_h * 0.34F, 0.30F),
               c.soot,
               k_building_state_mask_intact);
  for (float const leaf : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.200F, podium_y + cella_h * 0.32F, leaf * 0.135F),
                 QVector3D(0.020F, cella_h * 0.32F, 0.118F),
                 c.bronze,
                 k_building_state_mask_intact);
    for (int panel = 0; panel < 3; ++panel) {
      float const panel_y =
          podium_y + cella_h * (0.10F + 0.21F * static_cast<float>(panel));
      desc.add_box(QVector3D(-0.222F, panel_y, leaf * 0.135F),
                   QVector3D(0.008F, 0.052F, 0.086F),
                   c.gold,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }
  desc.add_box(QVector3D(-0.185F, podium_y + cella_h * 0.70F, 0.0F),
               QVector3D(0.048F, 0.036F, 0.38F),
               c.marble,
               k_building_state_mask_intact);

  float const entablature_y = podium_y + column_height + 0.070F;
  desc.add_box(QVector3D(0.02F, entablature_y, 0.0F),
               QVector3D(1.26F, 0.062F, 0.88F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.02F, entablature_y + 0.082F, 0.0F),
               QVector3D(1.24F, 0.024F, 0.86F),
               c.cloth_red,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.02F, entablature_y + 0.122F, 0.0F),
               QVector3D(1.30F, 0.030F, 0.92F),
               c.marble,
               k_building_state_mask_intact);

  for (int triglyph = 0; triglyph < 11; ++triglyph) {
    float const z = -0.80F + static_cast<float>(triglyph) * 0.16F;
    desc.add_box(QVector3D(-1.245F, entablature_y + 0.082F, z),
                 QVector3D(0.012F, 0.024F, 0.032F),
                 c.gold,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (int triglyph = 0; triglyph < 13; ++triglyph) {
    float const x = -1.18F + static_cast<float>(triglyph) * 0.20F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(x, entablature_y + 0.082F, side * 0.845F),
                   QVector3D(0.034F, 0.024F, 0.012F),
                   c.gold,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }

  float const pediment_y = entablature_y + 0.152F;

  float const pediment_rise = 0.42F;
  float const pediment_half_z = 0.90F;

  auto add_tympanum = [&](float face_x, float face_dir, const QVector3D& field) {
    auto add_triangle = [&](float x,
                            float half_thick,
                            float base_inset,
                            float z_inset,
                            const QVector3D& color,
                            BuildingLODMask lod) {
      constexpr int k_bands = 24;
      float const usable_rise = pediment_rise - base_inset * 2.0F;
      for (int band = 0; band < k_bands; ++band) {
        float const base =
            base_inset + usable_rise * static_cast<float>(band) / k_bands;
        float const top =
            base_inset + usable_rise * static_cast<float>(band + 1) / k_bands;
        float const half_z = pediment_half_z * (1.0F - base / pediment_rise) - z_inset;
        if (half_z <= 0.02F) {
          continue;
        }
        desc.add_box(QVector3D(x, pediment_y + (base + top) * 0.5F, 0.0F),
                     QVector3D(half_thick, (top - base) * 0.5F + 0.002F, half_z),
                     color,
                     k_building_state_mask_intact,
                     lod);
      }
    };

    add_triangle(face_x, 0.034F, 0.0F, 0.0F, c.marble, BuildingLODMask::All);

    add_triangle(face_x + face_dir * 0.028F,
                 0.008F,
                 0.105F,
                 0.325F,
                 field,
                 BuildingLODMask::Full);

    float const rake_theta =
        std::atan2(pediment_rise, pediment_half_z) * 180.0F / 3.14159265F;
    float const rake_half_len =
        std::sqrt(pediment_half_z * pediment_half_z + pediment_rise * pediment_rise) *
        0.5F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_rotated_box(QVector3D(face_x + face_dir * 0.030F,
                                     pediment_y + pediment_rise * 0.5F + 0.012F,
                                     side * pediment_half_z * 0.5F),
                           QVector3D(0.030F, 0.042F, rake_half_len + 0.030F),
                           QVector3D(side * rake_theta, 0.0F, 0.0F),
                           c.marble,
                           k_building_state_mask_intact);
    }
    desc.add_box(QVector3D(face_x + face_dir * 0.024F, pediment_y - 0.008F, 0.0F),
                 QVector3D(0.026F, 0.026F, pediment_half_z + 0.020F),
                 c.marble,
                 k_building_state_mask_intact);
  };

  add_tympanum(-1.20F, -1.0F, c.cloth_red);
  add_tympanum(1.24F, 1.0F, c.blue_accent);

  add_gable_roof_x(
      [&](const QVector3D& center,
          const QVector3D& scale,
          const QVector3D& euler,
          const QVector3D& color) {
        desc.add_rotated_box(center, scale, euler, color, k_building_state_mask_intact);
      },
      0.02F,
      0.0F,
      pediment_y,
      1.30F,
      pediment_half_z,
      pediment_rise,
      0.038F,
      c.bronze);

  {
    float const theta = std::atan2(pediment_rise, pediment_half_z);
    float const theta_deg = theta * 180.0F / 3.14159265F;
    float const slope_half_len =
        std::sqrt(pediment_half_z * pediment_half_z + pediment_rise * pediment_rise) *
        0.5F;
    float const surface_lift = (0.038F + 0.012F) / std::cos(theta);
    QVector3D const cover_tile = c.bronze * 0.62F + c.verdigris * 0.38F;
    for (int rib = 0; rib < 17; ++rib) {
      float const x = -1.22F + static_cast<float>(rib) * 0.155F;
      for (float const side : {-1.0F, 1.0F}) {
        desc.add_rotated_box(QVector3D(x,
                                       pediment_y + pediment_rise * 0.5F + surface_lift,
                                       side * pediment_half_z * 0.5F),
                             QVector3D(0.015F, 0.011F, slope_half_len),
                             QVector3D(side * theta_deg, 0.0F, 0.0F),
                             cover_tile,
                             k_building_state_mask_intact,
                             BuildingLODMask::Full);
      }
    }
  }

  desc.add_box(QVector3D(0.02F, pediment_y + pediment_rise + 0.030F, 0.0F),
               QVector3D(1.30F, 0.030F, 0.058F),
               c.gold,
               k_building_state_mask_intact);

  for (float const side : {-1.0F, 1.0F}) {
    for (int antefix = 0; antefix < 9; ++antefix) {
      float const x = -1.20F + static_cast<float>(antefix) * 0.30F;
      desc.add_box(QVector3D(x, pediment_y + 0.052F, side * 0.905F),
                   QVector3D(0.036F, 0.042F, 0.014F),
                   c.verdigris,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(x, pediment_y + 0.084F, side * 0.905F),
                   QVector3D(0.020F, 0.020F, 0.012F),
                   c.gold,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
    }
  }

  for (float const corner : {-1.0F, 1.0F}) {
    for (float const face : {-1.20F, 1.24F}) {
      desc.add_box(QVector3D(face, pediment_y + 0.022F, corner * 0.86F),
                   QVector3D(0.062F, 0.050F, 0.062F),
                   c.marble,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
      desc.add_cone(QVector3D(face, pediment_y + 0.062F, corner * 0.86F),
                    QVector3D(face, pediment_y + 0.205F, corner * 0.86F),
                    0.056F,
                    c.gold,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
    }
  }

  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, 0.92F));
  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, -0.92F));

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(
        QVector3D(-1.33F, podium_y, side * 0.60F),
        QVector3D(-1.33F, podium_y + 0.72F * height_multiplier, side * 0.60F),
        0.024F,
        c.bronze,
        k_building_state_mask_intact,
        BuildingLODMask::Full);
    desc.add_palette_box(
        QVector3D(-1.33F, podium_y + 0.52F * height_multiplier, side * 0.60F),
        QVector3D(0.012F, 0.17F, 0.12F),
        k_temple_team_slot,
        BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-1.33F, podium_y + 0.75F * height_multiplier, side * 0.60F),
                 QVector3D(0.032F, 0.032F, 0.032F),
                 c.gold,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  desc.add_palette_box(QVector3D(0.52F, podium_y + cella_h * 0.74F, 0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_palette_box(QVector3D(0.52F, podium_y + cella_h * 0.74F, -0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  add_roman_aquila_relief(desc,
                          QVector3D(-1.252F, pediment_y + pediment_rise * 0.40F, 0.0F),
                          BuildingFacadePlane::ZY,
                          0.46F,
                          c.gold,
                          c.cloth_red);
  add_roman_roof_standard(desc,
                          QVector3D(0.02F, pediment_y + pediment_rise + 0.046F, 0.0F),
                          0.68F,
                          c.gold,
                          c.cloth_red);

  if (ruined) {
    add_roman_temple_ruin(desc, c, podium_y, cella_h);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.18F, 0.0F, 0.92F),
                                 .stone = c.limestone,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.limestone_dark * 0.5F,
                                 .ground_y = 0.29F,
                                 .scale = 1.15F,
                                 .seed = 269});

  return build_building_archetype(desc, state);
}

auto temple_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_temple_archetype);
  return k_set.for_state(state);
}

} // namespace

void register_temple_renderer(EntityRendererRegistry& registry) {
  register_temple_renderer_variant(
      registry,
      TempleRendererConfig{.nation_slug = "roman",
                           .archetype = &temple_archetype,
                           .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.5F},
                           .selection = BuildingSelectionStyle{1.9F, 1.9F}});
}

} // namespace Render::GL::Roman
