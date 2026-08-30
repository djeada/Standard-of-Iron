#include "temple_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/entity/temple_renderer_common.h"
#include "render/render_archetype.h"

namespace Render::GL::Carthage {
namespace {

struct CarthageTemplePalette {
  QVector3D sandstone{0.82F, 0.70F, 0.52F};
  QVector3D sandstone_light{0.92F, 0.84F, 0.68F};
  QVector3D sandstone_dark{0.55F, 0.44F, 0.31F};
  QVector3D rubble{0.79F, 0.67F, 0.50F};
  QVector3D screed{0.77F, 0.71F, 0.59F};
  QVector3D mortar{0.63F, 0.55F, 0.42F};
  QVector3D basalt{0.22F, 0.21F, 0.22F};
  QVector3D basalt_light{0.34F, 0.33F, 0.34F};
  QVector3D cedar{0.34F, 0.21F, 0.10F};
  QVector3D cedar_light{0.52F, 0.35F, 0.17F};
  QVector3D bronze{0.48F, 0.27F, 0.075F};
  QVector3D gold{0.70F, 0.47F, 0.13F};
  QVector3D indigo{0.21F, 0.25F, 0.45F};
  QVector3D oxblood{0.47F, 0.10F, 0.08F};
  QVector3D saffron{0.76F, 0.36F, 0.035F};
  QVector3D ember{0.76F, 0.19F, 0.025F};
  QVector3D soot{0.15F, 0.13F, 0.12F};
  QVector3D verdigris{0.26F, 0.44F, 0.39F};
};

constexpr std::uint8_t k_temple_team_slot = 1;

constexpr float k_shrine_x = 0.47F;
constexpr float k_shrine_half_x = 0.77F;
constexpr float k_shrine_half_z = 0.92F;
constexpr float k_wall_batter = 0.052F;

auto wall_half_x_at(float t) -> float {
  return k_shrine_half_x - (k_wall_batter * t);
}

auto wall_half_z_at(float t) -> float {
  return k_shrine_half_z - (k_wall_batter * t);
}

void add_stepped_merlon(BuildingArchetypeDesc& desc,
                        const CarthageTemplePalette& c,
                        const QVector3D& base,
                        float half_along,
                        float half_across,
                        bool along_x) {
  QVector3D const lower = along_x ? QVector3D(half_along, 0.030F, half_across)
                                  : QVector3D(half_across, 0.030F, half_along);
  QVector3D const mid = along_x ? QVector3D(half_along * 0.70F, 0.028F, half_across)
                                : QVector3D(half_across, 0.028F, half_along * 0.70F);
  QVector3D const top = along_x ? QVector3D(half_along * 0.40F, 0.026F, half_across)
                                : QVector3D(half_across, 0.026F, half_along * 0.40F);

  desc.add_box(base + QVector3D(0.0F, 0.030F, 0.0F),
               lower,
               c.sandstone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.088F, 0.0F),
               mid,
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.142F, 0.0F),
               top,
               c.sandstone,
               k_building_state_mask_intact);
}

void add_votive_pillar(BuildingArchetypeDesc& desc,
                       const CarthageTemplePalette& c,
                       float x,
                       float z,
                       float base_y,
                       float height) {
  desc.add_box(
      QVector3D(x, base_y + 0.026F, z), QVector3D(0.128F, 0.026F, 0.128F), c.basalt);
  desc.add_box(QVector3D(x, base_y + 0.068F, z),
               QVector3D(0.106F, 0.020F, 0.106F),
               c.basalt_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(x, base_y + 0.100F, z),
               QVector3D(0.088F, 0.014F, 0.088F),
               c.sandstone_dark,
               k_building_state_mask_intact);

  constexpr int k_drums = 4;
  float const shaft_bottom = base_y + 0.112F;
  float const shaft_top = base_y + height - 0.120F;
  float const shaft_span = std::max(shaft_top - shaft_bottom, 0.04F);
  for (int drum = 0; drum < k_drums; ++drum) {
    float const t0 = static_cast<float>(drum) / k_drums;
    float const t1 = static_cast<float>(drum + 1) / k_drums;
    desc.add_cylinder(QVector3D(x, shaft_bottom + (shaft_span * t0), z),
                      QVector3D(x, shaft_bottom + (shaft_span * t1) + 0.004F, z),
                      0.082F - (0.014F * (t0 + t1) * 0.5F),
                      c.sandstone);
  }

  for (int band = 1; band < 4; ++band) {
    float const band_y = shaft_bottom + (shaft_span * static_cast<float>(band) / 4.0F);
    desc.add_cylinder(QVector3D(x, band_y, z),
                      QVector3D(x, band_y + 0.020F, z),
                      0.080F,
                      c.bronze,
                      k_building_state_mask_intact);
  }

  desc.add_cylinder(QVector3D(x, shaft_top, z),
                    QVector3D(x, shaft_top + 0.030F, z),
                    0.086F,
                    c.sandstone_light,
                    k_building_state_mask_intact);
  desc.add_box(QVector3D(x, shaft_top + 0.052F, z),
               QVector3D(0.100F, 0.022F, 0.100F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(x, shaft_top + 0.082F, z),
               QVector3D(0.082F, 0.014F, 0.082F),
               c.basalt_light,
               k_building_state_mask_intact);
  desc.add_cone(QVector3D(x, base_y + height, z),
                QVector3D(x, base_y + height + 0.150F, z),
                0.074F,
                c.gold,
                BuildingStateMask::Normal);
}

void add_incense_brazier(BuildingArchetypeDesc& desc,
                         const CarthageTemplePalette& c,
                         const QVector3D& base) {
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.055F, 0.0F, 0.045F),
                      base + QVector3D(0.0F, 0.120F, 0.0F),
                      0.014F,
                      c.bronze,
                      k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(side * 0.055F, 0.0F, -0.045F),
                      base + QVector3D(0.0F, 0.120F, 0.0F),
                      0.014F,
                      c.bronze,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(base + QVector3D(0.0F, 0.118F, 0.0F),
                    base + QVector3D(0.0F, 0.185F, 0.0F),
                    0.098F,
                    c.bronze,
                    k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(0.0F, 0.180F, 0.0F),
                    base + QVector3D(0.0F, 0.196F, 0.0F),
                    0.078F,
                    c.soot,
                    k_building_state_mask_intact);
  desc.add_cone(base + QVector3D(0.0F, 0.194F, 0.0F),
                base + QVector3D(0.0F, 0.300F, 0.0F),
                0.058F,
                c.ember,
                BuildingStateMask::Normal);
}

void add_opus_africanum(BuildingArchetypeDesc& desc,
                        const CarthageTemplePalette& c,
                        float podium_y,
                        float wall_h) {
  constexpr std::array<float, 5> k_pier_x{-0.24F, 0.13F, 0.50F, 0.87F, 1.19F};
  constexpr std::array<float, 5> k_pier_z{-0.86F, -0.43F, 0.0F, 0.43F, 0.86F};

  auto pier_face = [&](float t) {
    return wall_half_z_at(t) + 0.014F;
  };

  for (float const px : k_pier_x) {
    for (float const side : {-1.0F, 1.0F}) {
      for (int block = 0; block < 5; ++block) {
        float const t0 = static_cast<float>(block) / 5.0F;
        float const t1 = static_cast<float>(block + 1) / 5.0F;
        float const mid = (t0 + t1) * 0.5F;
        desc.add_box(QVector3D(px, podium_y + (wall_h * mid), side * pier_face(mid)),
                     QVector3D(0.062F, wall_h * 0.092F, 0.016F),
                     (block % 2 == 0) ? c.sandstone_light : c.sandstone,
                     k_building_state_mask_intact);
      }
    }
  }

  for (float const pz : k_pier_z) {
    for (int block = 0; block < 5; ++block) {
      float const t0 = static_cast<float>(block) / 5.0F;
      float const t1 = static_cast<float>(block + 1) / 5.0F;
      float const mid = (t0 + t1) * 0.5F;
      desc.add_box(QVector3D(k_shrine_x + wall_half_x_at(mid) + 0.014F,
                             podium_y + (wall_h * mid),
                             pz),
                   QVector3D(0.016F, wall_h * 0.092F, 0.062F),
                   (block % 2 == 0) ? c.sandstone_light : c.sandstone,
                   k_building_state_mask_intact);
    }
  }

  for (int course = 1; course < 8; ++course) {
    float const t = static_cast<float>(course) / 8.0F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(k_shrine_x,
                             podium_y + (wall_h * t),
                             side * (wall_half_z_at(t) + 0.006F)),
                   QVector3D(wall_half_x_at(t) * 0.97F, 0.007F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
    desc.add_box(QVector3D(k_shrine_x + wall_half_x_at(t) + 0.006F,
                           podium_y + (wall_h * t),
                           0.0F),
                 QVector3D(0.006F, 0.007F, wall_half_z_at(t) * 0.97F),
                 c.mortar,
                 k_building_state_mask_intact);
  }
}

void add_cavetto_cornice(BuildingArchetypeDesc& desc,
                         const CarthageTemplePalette& c,
                         float cornice_y) {
  desc.add_cylinder(
      QVector3D(k_shrine_x - k_shrine_half_x - 0.020F, cornice_y + 0.026F, 0.0F),
      QVector3D(k_shrine_x + k_shrine_half_x + 0.020F, cornice_y + 0.026F, 0.0F),
      0.030F,
      c.sandstone_dark,
      k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(QVector3D(k_shrine_x - k_shrine_half_x - 0.020F,
                                cornice_y + 0.026F,
                                side * (k_shrine_half_z + 0.006F)),
                      QVector3D(k_shrine_x + k_shrine_half_x + 0.020F,
                                cornice_y + 0.026F,
                                side * (k_shrine_half_z + 0.006F)),
                      0.030F,
                      c.sandstone_dark,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(QVector3D(k_shrine_x + k_shrine_half_x + 0.006F,
                              cornice_y + 0.026F,
                              -k_shrine_half_z - 0.020F),
                    QVector3D(k_shrine_x + k_shrine_half_x + 0.006F,
                              cornice_y + 0.026F,
                              k_shrine_half_z + 0.020F),
                    0.030F,
                    c.sandstone_dark,
                    k_building_state_mask_intact);

  constexpr int k_flares = 4;
  for (int flare = 0; flare < k_flares; ++flare) {
    float const t = static_cast<float>(flare) / (k_flares - 1);
    float const grow = 0.030F + (0.098F * t * t);
    desc.add_box(QVector3D(k_shrine_x,
                           cornice_y + 0.072F + (0.040F * static_cast<float>(flare)),
                           0.0F),
                 QVector3D(k_shrine_half_x + grow, 0.021F, k_shrine_half_z + grow),
                 (flare % 2 == 0) ? c.sandstone : c.sandstone_light,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(k_shrine_x, cornice_y + 0.242F, 0.0F),
               QVector3D(k_shrine_half_x + 0.150F, 0.020F, k_shrine_half_z + 0.150F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_shrine_x, cornice_y + 0.276F, 0.0F),
               QVector3D(k_shrine_half_x + 0.120F, 0.016F, k_shrine_half_z + 0.120F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_shrine_x, cornice_y + 0.298F, 0.0F),
               QVector3D(k_shrine_half_x + 0.100F, 0.008F, k_shrine_half_z + 0.100F),
               c.screed,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(
        QVector3D(k_shrine_x, cornice_y + 0.302F, side * (k_shrine_half_z + 0.086F)),
        QVector3D(k_shrine_half_x + 0.100F, 0.006F, 0.018F),
        c.verdigris,
        k_building_state_mask_intact);
  }
  desc.add_box(
      QVector3D(k_shrine_x + k_shrine_half_x + 0.086F, cornice_y + 0.302F, 0.0F),
      QVector3D(0.018F, 0.006F, k_shrine_half_z + 0.100F),
      c.verdigris,
      k_building_state_mask_intact);
  desc.add_box(
      QVector3D(k_shrine_x - k_shrine_half_x - 0.086F, cornice_y + 0.302F, 0.0F),
      QVector3D(0.018F, 0.006F, k_shrine_half_z + 0.100F),
      c.verdigris,
      k_building_state_mask_intact);
}

void add_roof_parapet(BuildingArchetypeDesc& desc,
                      const CarthageTemplePalette& c,
                      float roof_y) {
  float const half_x = k_shrine_half_x + 0.086F;
  float const half_z = k_shrine_half_z + 0.086F;

  constexpr int k_along_x = 7;
  for (int merlon = 0; merlon < k_along_x; ++merlon) {
    float const t = static_cast<float>(merlon) / (k_along_x - 1);
    float const px = k_shrine_x - half_x + 0.09F + ((half_x - 0.09F) * 2.0F * t);
    for (float const side : {-1.0F, 1.0F}) {
      add_stepped_merlon(
          desc, c, QVector3D(px, roof_y, side * half_z), 0.086F, 0.040F, true);
    }
  }

  constexpr int k_along_z = 7;
  for (int merlon = 1; merlon < k_along_z - 1; ++merlon) {
    float const t = static_cast<float>(merlon) / (k_along_z - 1);
    float const pz = -half_z + (half_z * 2.0F * t);
    add_stepped_merlon(
        desc, c, QVector3D(k_shrine_x + half_x, roof_y, pz), 0.086F, 0.040F, false);
  }
}

void add_pylon_doorway(BuildingArchetypeDesc& desc,
                       const CarthageTemplePalette& c,
                       float podium_y,
                       float wall_h) {
  float const jamb_h = wall_h * 0.35F;

  desc.add_box(QVector3D(-0.282F, podium_y + jamb_h, 0.0F),
               QVector3D(0.030F, jamb_h + 0.024F, 0.372F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.300F, podium_y + jamb_h, 0.0F),
               QVector3D(0.018F, jamb_h + 0.006F, 0.340F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.290F, podium_y + (jamb_h * 2.0F) + 0.048F, 0.0F),
               QVector3D(0.040F, 0.026F, 0.412F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(-0.312F, podium_y + (jamb_h * 2.0F) + 0.086F, -0.392F),
                    QVector3D(-0.312F, podium_y + (jamb_h * 2.0F) + 0.086F, 0.392F),
                    0.026F,
                    c.sandstone_dark,
                    k_building_state_mask_intact);

  desc.add_box(QVector3D(-0.286F, podium_y + (jamb_h * 0.96F), 0.0F),
               QVector3D(0.026F, jamb_h * 0.96F, 0.300F),
               c.soot,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.302F, podium_y + (jamb_h * 0.92F), 0.0F),
               QVector3D(0.024F, jamb_h * 0.92F, 0.272F),
               c.cedar,
               k_building_state_mask_intact);
  for (int stud = 0; stud < 4; ++stud) {
    float const stud_y =
        podium_y + (jamb_h * (0.28F + (0.44F * static_cast<float>(stud))));
    desc.add_box(QVector3D(-0.330F, stud_y, 0.0F),
                 QVector3D(0.008F, 0.016F, 0.252F),
                 c.bronze,
                 k_building_state_mask_intact);
  }
  for (float const leaf : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.332F, podium_y + (jamb_h * 0.92F), leaf * 0.024F),
                 QVector3D(0.008F, jamb_h * 0.88F, 0.012F),
                 c.bronze,
                 k_building_state_mask_intact);
  }

  for (int step = 0; step < 2; ++step) {
    float const t = static_cast<float>(step);
    desc.add_box(QVector3D(-0.340F - (0.056F * t),
                           podium_y + 0.012F + (0.026F * (1.0F - t)),
                           0.0F),
                 QVector3D(0.056F, 0.014F, 0.340F + (0.024F * t)),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }
}

void add_carthage_temple_ruin(BuildingArchetypeDesc& desc,
                              const CarthageTemplePalette& c,
                              float podium_y,
                              float wall_h) {
  constexpr auto k_ruin = BuildingStateMask::Destroyed;
  float const wall_top = podium_y + wall_h;
  QVector3D const ash = (c.soot * 0.45F) + (c.sandstone * 0.55F);
  QVector3D const ash_dark = (c.soot * 0.70F) + (c.sandstone * 0.30F);

  constexpr std::array<float, 6> k_rise{0.14F, 0.05F, 0.20F, 0.08F, 0.16F, 0.03F};
  for (int i = 0; i < 9; ++i) {
    float const px = -0.26F + (0.19F * static_cast<float>(i));
    for (float const side : {-1.0F, 1.0F}) {
      float const rise =
          k_rise[static_cast<std::size_t>(i + (side > 0 ? 2 : 0)) % k_rise.size()];
      if (rise < 0.04F) {
        continue;
      }
      desc.add_box(QVector3D(px, wall_top + (rise * 0.5F), side * 0.86F),
                   QVector3D(0.088F, rise * 0.5F, 0.062F),
                   c.sandstone,
                   k_ruin);
    }
  }
  for (int i = 0; i < 7; ++i) {
    float const pz = -0.78F + (0.26F * static_cast<float>(i));
    float const rise = k_rise[static_cast<std::size_t>(i + 3) % k_rise.size()];
    if (rise < 0.04F) {
      continue;
    }
    desc.add_box(QVector3D(1.18F, wall_top + (rise * 0.5F), pz),
                 QVector3D(0.062F, rise * 0.5F, 0.088F),
                 c.sandstone,
                 k_ruin);
  }

  desc.add_box(QVector3D(0.47F, wall_top + 0.010F, 0.0F),
               QVector3D(0.70F, 0.010F, 0.84F),
               ash,
               k_ruin);

  struct Block {
    float x;
    float y;
    float z;
    float yaw;
    float roll;
    float sx;
    float sy;
    float sz;
    bool on_wall;
  };
  constexpr std::array<Block, 9> k_blocks{
      Block{0.30F, 0.0F, -0.28F, 31.0F, 8.0F, 0.17F, 0.070F, 0.13F, true},
      Block{0.78F, 0.0F, 0.34F, 69.0F, -7.0F, 0.14F, 0.085F, 0.12F, true},
      Block{1.02F, 0.0F, -0.44F, 12.0F, 13.0F, 0.12F, 0.055F, 0.15F, true},
      Block{0.44F, 0.0F, 0.52F, 84.0F, -5.0F, 0.15F, 0.065F, 0.11F, true},
      Block{-1.34F, 0.10F, 0.44F, 24.0F, 10.0F, 0.17F, 0.080F, 0.12F, false},
      Block{-0.48F, 0.09F, -1.22F, 57.0F, -9.0F, 0.14F, 0.068F, 0.14F, false},
      Block{0.58F, 0.10F, 1.28F, 16.0F, 15.0F, 0.19F, 0.072F, 0.11F, false},
      Block{1.46F, 0.09F, -0.62F, 78.0F, -12.0F, 0.13F, 0.060F, 0.13F, false},
      Block{-1.08F, 0.09F, -0.96F, 42.0F, 6.0F, 0.15F, 0.064F, 0.12F, false}};
  for (const auto& block : k_blocks) {
    desc.add_rotated_box(
        QVector3D(block.x,
                  block.y + (block.on_wall ? wall_top + block.sy + 0.010F : block.sy),
                  block.z),
        QVector3D(block.sx, block.sy, block.sz),
        QVector3D(block.roll, block.yaw, 0.0F),
        (block.yaw > 50.0F) ? ash_dark : c.sandstone_dark,
        k_ruin);
  }

  for (float const side : {-1.0F, 1.0F}) {
    float const yaw = side > 0.0F ? 0.35F : -0.62F;
    QVector3D const mid(-1.30F, 0.085F, side * 0.66F);
    QVector3D const axis(std::cos(yaw) * 0.42F, 0.0F, std::sin(yaw) * 0.42F);
    desc.add_cylinder(mid - axis, mid + axis, 0.079F, c.sandstone, k_ruin);
    desc.add_box(mid + axis + QVector3D(0.0F, 0.006F, 0.0F),
                 QVector3D(0.086F, 0.026F, 0.086F),
                 c.basalt,
                 k_ruin);
  }

  for (float const sx : {-0.90F, 0.30F, 1.10F}) {
    desc.add_box(QVector3D(sx, podium_y + 0.010F, -0.42F),
                 QVector3D(0.24F, 0.008F, 0.20F),
                 ash,
                 k_ruin);
  }
}

auto build_temple_archetype(BuildingState state) -> RenderArchetype {
  CarthageTemplePalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.74F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.42F;
  }

  BuildingArchetypeDesc desc("carthage_temple");

  desc.add_box(
      QVector3D(0.0F, 0.055F, 0.0F), QVector3D(1.38F, 0.055F, 1.16F), c.basalt);
  desc.add_box(
      QVector3D(0.0F, 0.195F, 0.0F), QVector3D(1.33F, 0.085F, 1.11F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.345F, 0.0F), QVector3D(1.29F, 0.065F, 1.07F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.428F, 0.0F), QVector3D(1.25F, 0.030F, 1.03F), c.sandstone);
  desc.add_box(QVector3D(0.0F, 0.464F, 0.0F),
               QVector3D(1.30F, 0.014F, 1.08F),
               c.sandstone_light);
  desc.add_box(QVector3D(0.0F, 0.482F, 0.0F),
               QVector3D(1.27F, 0.008F, 1.05F),
               c.sandstone_light);

  float const podium_y = 0.490F;

  for (float const joint_x : {-0.84F, -0.24F, 0.36F, 0.96F}) {
    desc.add_box(QVector3D(joint_x, 0.165F, 0.0F),
                 QVector3D(0.008F, 0.052F, 1.095F),
                 c.mortar,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(joint_x, 0.290F, 0.0F),
                 QVector3D(0.008F, 0.046F, 1.075F),
                 c.mortar,
                 k_building_state_mask_intact);
  }
  for (int course = 0; course < 3; ++course) {
    float const course_y = 0.130F + (0.086F * static_cast<float>(course));
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.0F, course_y, side * 1.095F),
                   QVector3D(1.30F, 0.006F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
    for (float const face : {-1.315F, 1.315F}) {
      desc.add_box(QVector3D(face, course_y, 0.0F),
                   QVector3D(0.006F, 0.006F, 1.08F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
  }

  for (int step = 0; step < 6; ++step) {
    float const step_index = static_cast<float>(step);
    float const top = 0.082F * (step_index + 1.0F);
    desc.add_box(QVector3D(-1.700F + (0.078F * step_index), top * 0.5F, 0.0F),
                 QVector3D(0.041F, top * 0.5F, 0.66F),
                 (step % 2 == 0) ? c.sandstone_dark : c.sandstone,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.700F + (0.078F * step_index), top - 0.006F, 0.0F),
                 QVector3D(0.043F, 0.006F, 0.665F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }
  for (float const cheek : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-1.50F, 0.24F, cheek * 0.715F),
                 QVector3D(0.25F, 0.24F, 0.055F),
                 c.basalt,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.50F, 0.492F, cheek * 0.715F),
                 QVector3D(0.26F, 0.012F, 0.062F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }

  float const wall_h = 1.12F * height_multiplier;

  constexpr int k_wall_courses = 6;
  for (int course = 0; course < k_wall_courses; ++course) {
    float const t0 = static_cast<float>(course) / k_wall_courses;
    float const t1 = static_cast<float>(course + 1) / k_wall_courses;
    float const mid = (t0 + t1) * 0.5F;
    desc.add_box(
        QVector3D(k_shrine_x, podium_y + (wall_h * mid), 0.0F),
        QVector3D(wall_half_x_at(mid), wall_h * (t1 - t0) * 0.5F, wall_half_z_at(mid)),
        (course % 2 == 0) ? c.sandstone : c.rubble);
  }

  add_opus_africanum(desc, c, podium_y, wall_h);

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.5F), side * 0.80F),
                 QVector3D(0.30F, wall_h * 0.5F, 0.12F),
                 c.sandstone);
    desc.add_box(QVector3D(-0.86F, podium_y + (wall_h * 0.5F), side * 0.80F),
                 QVector3D(0.026F, wall_h * 0.5F, 0.14F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h + 0.030F, side * 0.80F),
                 QVector3D(0.33F, 0.030F, 0.15F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.74F), 0.0F),
               QVector3D(0.30F, wall_h * 0.055F, 0.70F),
               c.cedar,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.83F), 0.0F),
               QVector3D(0.32F, wall_h * 0.040F, 0.72F),
               c.sandstone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.90F), 0.0F),
               QVector3D(0.34F, wall_h * 0.030F, 0.74F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.895F, podium_y + (wall_h * 0.86F), 0.0F),
               QVector3D(0.014F, wall_h * 0.070F, 0.74F),
               c.basalt,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.86F), side * 0.752F),
                 QVector3D(0.34F, wall_h * 0.070F, 0.014F),
                 c.basalt,
                 k_building_state_mask_intact);
  }
  for (int rafter = 0; rafter < 5; ++rafter) {
    float const z = -0.56F + (0.28F * static_cast<float>(rafter));
    desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.70F), z),
                 QVector3D(0.30F, wall_h * 0.022F, 0.030F),
                 c.cedar_light,
                 k_building_state_mask_intact);
  }
  for (int merlon = 0; merlon < 5; ++merlon) {
    float const pz = -0.68F + (0.34F * static_cast<float>(merlon));
    add_stepped_merlon(desc,
                       c,
                       QVector3D(-0.58F, podium_y + wall_h + 0.060F, pz),
                       0.070F,
                       0.140F,
                       false);
  }

  for (float const cz : {-0.34F, 0.34F}) {
    desc.add_box(QVector3D(-0.66F, podium_y + 0.030F, cz),
                 QVector3D(0.10F, 0.030F, 0.10F),
                 c.basalt);
    desc.add_cylinder(QVector3D(-0.66F, podium_y + 0.055F, cz),
                      QVector3D(-0.66F, podium_y + (wall_h * 0.60F), cz),
                      0.070F,
                      c.basalt_light);
    desc.add_cylinder(QVector3D(-0.66F, podium_y + (wall_h * 0.60F), cz),
                      QVector3D(-0.66F, podium_y + (wall_h * 0.64F), cz),
                      0.086F,
                      c.bronze,
                      k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.66F, podium_y + (wall_h * 0.67F), cz),
                 QVector3D(0.098F, wall_h * 0.032F, 0.098F),
                 c.basalt,
                 k_building_state_mask_intact);
  }

  auto wrap_band = [&](float band_y,
                       float half_h,
                       float proud,
                       const QVector3D& color,
                       BuildingStateMask states) {
    float const t = (band_y - podium_y) / std::max(wall_h, 0.001F);
    float const half_x = wall_half_x_at(t);
    float const half_z = wall_half_z_at(t);
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(k_shrine_x, band_y, side * (half_z + (proud * 0.5F))),
                   QVector3D(half_x + proud, half_h, (proud * 0.5F) + 0.004F),
                   color,
                   states);
    }
    desc.add_box(QVector3D(k_shrine_x + half_x + (proud * 0.5F), band_y, 0.0F),
                 QVector3D((proud * 0.5F) + 0.004F, half_h, half_z + proud),
                 color,
                 states);
  };

  float const register_y = podium_y + (wall_h * 0.845F);
  float const register_half_h = wall_h * 0.082F;
  wrap_band(register_y, register_half_h, 0.026F, c.indigo, BuildingStateMask::Normal);
  wrap_band(register_y - (register_half_h * 0.52F),
            wall_h * 0.020F,
            0.034F,
            c.oxblood,
            BuildingStateMask::Normal);
  for (float const edge : {-1.0F, 1.0F}) {
    wrap_band(register_y + (edge * (register_half_h + (wall_h * 0.024F))),
              wall_h * 0.024F,
              0.044F,
              c.sandstone_light,
              k_building_state_mask_intact);
  }
  wrap_band(podium_y + (wall_h * 0.085F),
            wall_h * 0.085F,
            0.030F,
            c.sandstone_dark,
            k_building_state_mask_intact);

  add_pylon_doorway(desc, c, podium_y, wall_h);

  float const cornice_y = podium_y + wall_h;
  add_cavetto_cornice(desc, c, cornice_y);
  add_roof_parapet(desc, c, cornice_y + 0.306F);

  for (float const joint_z : {-0.62F, 0.0F, 0.62F}) {
    desc.add_box(QVector3D(k_shrine_x, cornice_y + 0.308F, joint_z),
                 QVector3D(k_shrine_half_x + 0.080F, 0.004F, 0.007F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
  }
  for (float const joint_x : {-0.10F, 0.47F, 1.04F}) {
    desc.add_box(QVector3D(joint_x, cornice_y + 0.308F, 0.0F),
                 QVector3D(0.007F, 0.004F, k_shrine_half_z + 0.080F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(0.62F, cornice_y + 0.326F, 0.0F),
               QVector3D(0.500F, 0.020F, 0.540F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.62F, cornice_y + 0.436F, 0.0F),
               QVector3D(0.440F, 0.090F, 0.480F),
               c.sandstone,
               k_building_state_mask_intact);
  for (int band = 1; band < 3; ++band) {
    float const band_y = cornice_y + 0.346F + (0.060F * static_cast<float>(band));
    desc.add_box(QVector3D(0.62F, band_y, 0.485F),
                 QVector3D(0.435F, 0.007F, 0.006F),
                 c.mortar,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(1.062F, band_y, 0.0F),
                 QVector3D(0.006F, 0.007F, 0.475F),
                 c.mortar,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(0.178F, cornice_y + 0.420F, 0.0F),
               QVector3D(0.014F, 0.062F, 0.140F),
               c.soot,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.172F, cornice_y + 0.420F, 0.0F),
               QVector3D(0.010F, 0.056F, 0.118F),
               c.cedar,
               k_building_state_mask_intact);

  for (int flare = 0; flare < 3; ++flare) {
    float const t = static_cast<float>(flare) / 2.0F;
    float const grow = 0.014F + (0.062F * t * t);
    desc.add_box(QVector3D(0.62F,
                           cornice_y + 0.542F + (0.030F * static_cast<float>(flare)),
                           0.0F),
                 QVector3D(0.440F + grow, 0.016F, 0.480F + grow),
                 (flare % 2 == 0) ? c.sandstone_light : c.sandstone,
                 k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(0.62F, cornice_y + 0.622F, 0.0F),
               QVector3D(0.520F, 0.014F, 0.560F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.62F, cornice_y + 0.646F, 0.0F),
               QVector3D(0.480F, 0.012F, 0.520F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.62F, cornice_y + 0.662F, 0.0F),
               QVector3D(0.190F, 0.008F, 0.210F),
               c.indigo,
               BuildingStateMask::Normal);

  float const pillar_height = 1.86F * height_multiplier;
  add_votive_pillar(desc, c, -1.16F, 0.62F, podium_y, pillar_height);
  add_votive_pillar(desc, c, -1.16F, -0.62F, podium_y, pillar_height);

  desc.add_box(QVector3D(-1.02F, podium_y + 0.024F, 0.0F),
               QVector3D(0.30F, 0.024F, 0.56F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.02F, podium_y + 0.034F, 0.0F),
               QVector3D(0.19F, 0.010F, 0.32F),
               c.indigo,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-1.02F, podium_y + 0.040F, 0.0F),
               QVector3D(0.09F, 0.010F, 0.15F),
               c.saffron,
               BuildingStateMask::Normal);

  add_incense_brazier(desc, c, QVector3D(-1.42F, 0.30F, 0.94F));
  add_incense_brazier(desc, c, QVector3D(-1.42F, 0.30F, -0.94F));

  desc.add_palette_box(QVector3D(-0.334F, podium_y + (wall_h * 0.74F), 0.0F),
                       QVector3D(0.014F, 0.042F, 0.30F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(k_shrine_x + wall_half_x_at(0.735F) + 0.028F,
                         podium_y + (wall_h * 0.735F),
                         0.0F),
               QVector3D(0.016F, 0.020F, 0.30F),
               c.bronze,
               k_building_state_mask_intact);
  desc.add_palette_box(QVector3D(k_shrine_x + wall_half_x_at(0.575F) + 0.030F,
                                 podium_y + (wall_h * 0.575F),
                                 0.0F),
                       QVector3D(0.012F, wall_h * 0.145F, 0.255F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_box(QVector3D(k_shrine_x + wall_half_x_at(0.428F) + 0.032F,
                         podium_y + (wall_h * 0.428F),
                         0.0F),
               QVector3D(0.012F, 0.014F, 0.255F),
               c.gold,
               BuildingStateMask::Normal);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.62F), side * 0.930F),
                 QVector3D(0.16F, 0.020F, 0.010F),
                 c.bronze,
                 k_building_state_mask_intact);
    desc.add_palette_box(QVector3D(-0.58F, podium_y + (wall_h * 0.44F), side * 0.936F),
                         QVector3D(0.13F, 0.17F, 0.010F),
                         k_temple_team_slot,
                         BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-0.58F, podium_y + (wall_h * 0.26F), side * 0.938F),
                 QVector3D(0.13F, 0.014F, 0.010F),
                 c.gold,
                 BuildingStateMask::Normal);
  }

  add_punic_tanit_relief(desc,
                         QVector3D(-0.356F, podium_y + (wall_h * 0.44F), 0.0F),
                         BuildingFacadePlane::ZY,
                         0.40F,
                         c.gold,
                         c.basalt);

  add_punic_horned_crown(desc,
                         QVector3D(0.62F, cornice_y + 0.664F, 0.0F),
                         0.66F,
                         c.basalt,
                         c.gold,
                         c.ember);

  if (state == BuildingState::Destroyed) {
    add_carthage_temple_ruin(desc, c, podium_y, wall_h);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.18F, 0.0F, 0.98F),
                                 .stone = c.sandstone,
                                 .stone_dark = c.basalt,
                                 .timber = c.basalt * 0.7F,
                                 .ground_y = 0.28F,
                                 .scale = 1.15F,
                                 .seed = 311});

  desc.scale_uniformly(k_temple_mesh_scale);

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
      TempleRendererConfig{.nation_slug = "carthage",
                           .archetype = &temple_archetype,
                           .selection = BuildingSelectionStyle{3.8F, 3.8F}});
}

} // namespace Render::GL::Carthage
