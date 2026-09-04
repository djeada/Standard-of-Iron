#include "barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "math/math_utils.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/barracks_renderer_common.h"
#include "render/entity/barracks_stockpile.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_team_slot = 0;

constexpr auto k_mask_intact = k_building_state_mask_intact;
constexpr auto k_mask_normal = BuildingStateMask::Normal;

constexpr float k_pi = 3.14159265F;

struct CarthagePalette {
  QVector3D sandstone{0.82F, 0.70F, 0.52F};
  QVector3D sandstone_light{0.92F, 0.84F, 0.68F};
  QVector3D sandstone_dark{0.55F, 0.44F, 0.31F};
  QVector3D stone_dark{0.38F, 0.32F, 0.24F};
  QVector3D plaster{0.90F, 0.83F, 0.68F};
  QVector3D plaster_shade{0.80F, 0.73F, 0.58F};
  QVector3D brick_dark{0.40F, 0.18F, 0.11F};
  QVector3D indigo{0.21F, 0.25F, 0.45F};
  QVector3D oxblood{0.47F, 0.10F, 0.08F};
  QVector3D saffron{0.76F, 0.36F, 0.035F};
  QVector3D wood{0.46F, 0.30F, 0.16F};
  QVector3D wood_dark{0.23F, 0.16F, 0.09F};
  QVector3D iron{0.24F, 0.23F, 0.21F};
  QVector3D bronze{0.72F, 0.43F, 0.12F};
  QVector3D ember{0.68F, 0.22F, 0.045F};
  QVector3D flame{0.95F, 0.55F, 0.12F};
  QVector3D shadow{0.10F, 0.08F, 0.06F};
  QVector3D sack{0.72F, 0.62F, 0.44F};
  QVector3D frond{0.30F, 0.46F, 0.20F};
  QVector3D frond_dark{0.20F, 0.34F, 0.14F};
  QVector3D palm_trunk{0.47F, 0.37F, 0.24F};
  QVector3D date{0.62F, 0.34F, 0.10F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

constexpr float k_platform_top = 0.22F;

constexpr float k_keep_hx = 1.28F;
constexpr float k_keep_z0 = -1.42F;
constexpr float k_keep_z1 = 0.58F;
constexpr float k_keep_plinth_top = 0.62F;
constexpr float k_keep_ashlar_top = 1.45F;
constexpr float k_keep_top = 1.95F;

constexpr float k_tower_r = 0.34F;
constexpr float k_tower_top = 2.30F;

constexpr float k_donjon_hx = 0.60F;
constexpr float k_donjon_z0 = -1.32F;
constexpr float k_donjon_z1 = -0.30F;
constexpr float k_donjon_top = 2.78F;

constexpr float k_gate_hx = 0.58F;
constexpr float k_gate_z0 = k_keep_z1;
constexpr float k_gate_z1 = 1.14F;
constexpr float k_gate_arch_y = 1.42F;
constexpr float k_gate_top = 2.24F;

struct Heights {
  float keep;
  float tower;
  float donjon;
  float gate;
  bool destroyed;
  bool damaged;
};

auto heights_for(BuildingState state) -> Heights {
  switch (state) {
  case BuildingState::Damaged:
    return {1.74F, 1.96F, 2.46F, 2.00F, false, true};
  case BuildingState::Destroyed:
    return {0.72F, 0.88F, 0.0F, 0.92F, true, false};
  case BuildingState::Normal:
  default:
    return {k_keep_top, k_tower_top, k_donjon_top, k_gate_top, false, false};
  }
}

inline auto mid(float a, float b) -> float {
  return (a + b) * 0.5F;
}
inline auto half(float a, float b) -> float {
  return (b - a) * 0.5F;
}

void add_platform(BuildingArchetypeDesc& desc, const CarthagePalette& c) {
  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.92F, 0.05F, 1.62F), c.stone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.84F, 0.04F, 1.54F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.18F, 0.0F), QVector3D(1.78F, 0.04F, 1.48F), c.sandstone);

  desc.add_box(
      QVector3D(0.0F, 0.06F, 1.68F), QVector3D(0.86F, 0.04F, 0.20F), c.stone_dark);
  desc.add_box(QVector3D(0.0F, 0.13F, 1.52F),
               QVector3D(0.72F, 0.04F, 0.16F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, 0.20F, 1.40F),
               QVector3D(0.60F, 0.04F, 0.12F),
               c.sandstone,
               k_mask_intact);

  desc.add_box(QVector3D(0.0F, k_platform_top + 0.004F, 1.10F),
               QVector3D(1.46F, 0.004F, 0.40F),
               c.sandstone_dark,
               k_mask_intact);
}

void add_merlon(BuildingArchetypeDesc& desc,
                const CarthagePalette& c,
                const QVector3D& center,
                const QVector3D& half_size,
                float yaw_deg,
                BuildingStateMask states,
                int) {
  const QVector3D rot(0.0F, yaw_deg, 0.0F);
  const QVector3D body(half_size.x(), half_size.y() * 1.55F, half_size.z());
  desc.add_rotated_box(
      QVector3D(center.x(), center.y() + body.y() - half_size.y(), center.z()),
      body,
      rot,
      c.sandstone_light,
      states);
  desc.add_rotated_box(
      QVector3D(center.x(),
                center.y() + 2.0F * body.y() - half_size.y() + 0.010F,
                center.z()),
      QVector3D(half_size.x() + 0.008F, 0.010F, half_size.z() + 0.008F),
      rot,
      c.sandstone_dark,
      states);
}

void add_slit_z_face(BuildingArchetypeDesc& desc,
                     const CarthagePalette& c,
                     float x,
                     float y,
                     float z,
                     float outward) {
  desc.add_box(QVector3D(x, y, z + outward * 0.012F),
               QVector3D(0.03F, 0.15F, 0.015F),
               c.shadow,
               k_mask_intact);
  desc.add_box(QVector3D(x, y - 0.17F, z + outward * 0.02F),
               QVector3D(0.07F, 0.018F, 0.022F),
               c.sandstone_dark,
               k_mask_intact);
}

void add_slit_x_face(BuildingArchetypeDesc& desc,
                     const CarthagePalette& c,
                     float x,
                     float y,
                     float z,
                     float outward) {
  desc.add_box(QVector3D(x + outward * 0.012F, y, z),
               QVector3D(0.015F, 0.15F, 0.03F),
               c.shadow,
               k_mask_intact);
  desc.add_box(QVector3D(x + outward * 0.02F, y - 0.17F, z),
               QVector3D(0.022F, 0.018F, 0.07F),
               c.sandstone_dark,
               k_mask_intact);
}

void add_keep(BuildingArchetypeDesc& desc, const CarthagePalette& c, const Heights& h) {
  const float cz = mid(k_keep_z0, k_keep_z1);
  const float hz = half(k_keep_z0, k_keep_z1);

  desc.add_box(QVector3D(0.0F, mid(k_platform_top, 0.42F), cz),
               QVector3D(k_keep_hx + 0.11F, half(k_platform_top, 0.42F), hz + 0.11F),
               c.stone_dark);
  desc.add_box(
      QVector3D(0.0F, mid(0.42F, k_keep_plinth_top), cz),
      QVector3D(k_keep_hx + 0.055F, half(0.42F, k_keep_plinth_top), hz + 0.055F),
      c.sandstone_dark);

  const float ashlar_top = std::min(h.keep, k_keep_ashlar_top);
  desc.add_box(QVector3D(0.0F, mid(k_keep_plinth_top, ashlar_top), cz),
               QVector3D(k_keep_hx, half(k_keep_plinth_top, ashlar_top), hz),
               c.sandstone);
  if (h.destroyed) {
    for (const float px : {-1.00F, -0.40F, 0.40F, 1.00F}) {
      desc.add_box(QVector3D(px, mid(k_keep_plinth_top, h.keep + 0.10F), k_keep_z1),
                   QVector3D(0.06F, half(k_keep_plinth_top, h.keep + 0.10F), 0.03F),
                   c.stone_dark,
                   BuildingStateMask::Destroyed);
    }
    return;
  }

  desc.add_box(
      QVector3D(0.0F, mid(k_keep_ashlar_top, h.keep), cz),
      QVector3D(k_keep_hx - 0.01F, half(k_keep_ashlar_top, h.keep), hz - 0.01F),
      c.plaster,
      k_mask_intact);
  desc.add_box(QVector3D(0.0F, k_keep_ashlar_top + 0.05F, cz),
               QVector3D(k_keep_hx + 0.012F, 0.03F, hz + 0.012F),
               c.indigo,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, k_keep_ashlar_top + 0.012F, cz),
               QVector3D(k_keep_hx + 0.02F, 0.012F, hz + 0.02F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.keep - 0.115F, cz),
               QVector3D(k_keep_hx + 0.02F, 0.012F, hz + 0.02F),
               c.brick_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.keep - 0.070F, cz),
               QVector3D(k_keep_hx + 0.035F, 0.032F, hz + 0.035F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.keep - 0.024F, cz),
               QVector3D(k_keep_hx + 0.080F, 0.032F, hz + 0.080F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.keep + 0.02F, cz),
               QVector3D(k_keep_hx - 0.06F, 0.02F, hz - 0.06F),
               c.sandstone_dark,
               k_mask_intact);

  for (const float px : {-1.00F, -0.40F, 0.40F, 1.00F}) {
    for (const float side : {-1.0F, 1.0F}) {
      const float z = side > 0.0F ? k_keep_z1 : k_keep_z0;
      desc.add_box(
          QVector3D(px, mid(k_keep_plinth_top, h.keep - 0.07F), z + side * 0.025F),
          QVector3D(0.06F, half(k_keep_plinth_top, h.keep - 0.07F), 0.025F),
          c.sandstone_dark,
          k_mask_intact);
    }
  }
  for (const float pz : {-0.95F, -0.42F, 0.10F}) {
    for (const float side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(side * (k_keep_hx + 0.025F),
                             mid(k_keep_plinth_top, h.keep - 0.07F),
                             pz),
                   QVector3D(0.025F, half(k_keep_plinth_top, h.keep - 0.07F), 0.06F),
                   c.sandstone_dark,
                   k_mask_intact);
    }
  }
  for (const float course_y : {0.90F, 1.18F}) {
    desc.add_box(QVector3D(0.0F, course_y, cz),
                 QVector3D(k_keep_hx + 0.008F, 0.006F, hz + 0.008F),
                 c.sandstone_dark * 0.92F,
                 k_mask_normal);
  }

  add_slit_z_face(desc, c, -0.70F, 1.20F, k_keep_z0, -1.0F);
  add_slit_z_face(desc, c, 0.0F, 1.20F, k_keep_z0, -1.0F);
  add_slit_z_face(desc, c, 0.70F, 1.20F, k_keep_z0, -1.0F);
  add_slit_z_face(desc, c, -0.70F, 1.20F, k_keep_z1, 1.0F);
  add_slit_z_face(desc, c, 0.70F, 1.20F, k_keep_z1, 1.0F);
  for (const float sz : {-0.70F, -0.16F}) {
    add_slit_x_face(desc, c, -k_keep_hx, 1.20F, sz, -1.0F);
    add_slit_x_face(desc, c, k_keep_hx, 1.20F, sz, 1.0F);
  }

  const QVector3D merlon_half(0.09F, 0.07F, 0.07F);
  const float merlon_y = h.keep + 0.07F;
  int seed = 11;
  for (const float mx : {-1.10F, -0.72F, 0.72F, 1.10F}) {
    const int merlon_seed = seed++;
    add_merlon(desc,
               c,
               QVector3D(mx, merlon_y, k_keep_z1 - 0.02F),
               merlon_half,
               0.0F,
               (merlon_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
               merlon_seed);
  }
  for (const float mx : {-1.10F, -0.76F, 0.76F, 1.10F}) {
    const int merlon_seed = seed++;
    add_merlon(desc,
               c,
               QVector3D(mx, merlon_y, k_keep_z0 + 0.02F),
               merlon_half,
               0.0F,
               (merlon_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
               merlon_seed);
  }
  for (const float mz : {-1.05F, -0.70F, -0.35F, 0.0F, 0.30F}) {
    for (const float side : {-1.0F, 1.0F}) {
      const int merlon_seed = seed++;
      add_merlon(desc,
                 c,
                 QVector3D(side * (k_keep_hx - 0.02F), merlon_y, mz),
                 QVector3D(0.07F, 0.07F, 0.09F),
                 0.0F,
                 (merlon_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
                 merlon_seed);
    }
  }

  const float hood_x = -0.95F;
  const float hood_z = 0.10F;
  desc.add_box(QVector3D(hood_x, h.keep + 0.20F, hood_z),
               QVector3D(0.16F, 0.16F, 0.16F),
               c.plaster_shade,
               k_mask_normal);
  desc.add_box(QVector3D(hood_x, h.keep + 0.40F, hood_z),
               QVector3D(0.19F, 0.03F, 0.19F),
               c.sandstone_dark,
               k_mask_normal);
  desc.add_box(QVector3D(hood_x, h.keep + 0.22F, hood_z + 0.17F),
               QVector3D(0.08F, 0.10F, 0.015F),
               c.shadow,
               k_mask_normal);
}

void add_towers(BuildingArchetypeDesc& desc,
                const CarthagePalette& c,
                const Heights& h) {
  const std::array<QVector3D, 4> corners{QVector3D(-k_keep_hx, 0.0F, k_keep_z0),
                                         QVector3D(k_keep_hx, 0.0F, k_keep_z0),
                                         QVector3D(-k_keep_hx, 0.0F, k_keep_z1),
                                         QVector3D(k_keep_hx, 0.0F, k_keep_z1)};
  int seed = 31;
  for (const QVector3D& corner : corners) {
    const float x = corner.x();
    const float z = corner.z();
    const bool front = z > 0.0F;
    const float top = h.tower + (front ? 0.0F : 0.06F);

    desc.add_cylinder(QVector3D(x, k_platform_top, z),
                      QVector3D(x, k_keep_plinth_top, z),
                      k_tower_r + 0.08F,
                      c.stone_dark);
    desc.add_cylinder(QVector3D(x, k_keep_plinth_top, z),
                      QVector3D(x, top, z),
                      k_tower_r,
                      c.sandstone);
    if (h.destroyed) {
      desc.add_cylinder(QVector3D(x, top, z),
                        QVector3D(x, top + 0.22F + (front ? 0.14F : 0.0F), z),
                        k_tower_r - 0.14F,
                        c.sandstone_dark,
                        BuildingStateMask::Destroyed);
      ++seed;
      continue;
    }

    desc.add_cylinder(QVector3D(x, k_keep_ashlar_top + 0.02F, z),
                      QVector3D(x, k_keep_ashlar_top + 0.08F, z),
                      k_tower_r + 0.015F,
                      c.indigo,
                      k_mask_intact);
    desc.add_cylinder(QVector3D(x, top - 0.09F, z),
                      QVector3D(x, top - 0.04F, z),
                      k_tower_r + 0.03F,
                      c.sandstone_dark,
                      k_mask_intact);
    desc.add_cylinder(QVector3D(x, top - 0.04F, z),
                      QVector3D(x, top + 0.05F, z),
                      k_tower_r + 0.075F,
                      c.sandstone_dark,
                      k_mask_intact);
    desc.add_cylinder(QVector3D(x, top + 0.05F, z),
                      QVector3D(x, top + 0.08F, z),
                      k_tower_r - 0.04F,
                      c.sandstone_dark,
                      k_mask_intact);

    for (int i = 0; i < 8; ++i) {
      const float angle = static_cast<float>(i) * (k_pi * 0.25F) + k_pi * 0.125F;
      const float mx = x + std::sin(angle) * (k_tower_r - 0.02F);
      const float mz = z + std::cos(angle) * (k_tower_r - 0.02F);
      add_merlon(desc,
                 c,
                 QVector3D(mx, top + 0.16F, mz),
                 QVector3D(0.075F, 0.075F, 0.045F),
                 angle * 180.0F / k_pi,
                 (i % 3 == 1) ? k_mask_normal : k_mask_intact,
                 seed + i);
    }

    const float slit_x = x + (x > 0.0F ? 1.0F : -1.0F) * (k_tower_r - 0.005F);
    desc.add_box(QVector3D(slit_x, 1.20F, z),
                 QVector3D(0.02F, 0.15F, 0.03F),
                 c.shadow,
                 k_mask_intact);
    const float slit_z = z + (front ? 1.0F : -1.0F) * (k_tower_r - 0.005F);
    desc.add_box(QVector3D(x, 1.20F, slit_z),
                 QVector3D(0.03F, 0.15F, 0.02F),
                 c.shadow,
                 k_mask_intact);

    if (front) {
      desc.add_cylinder(QVector3D(x, top + 0.08F, z),
                        QVector3D(x, top + 0.34F, z),
                        0.03F,
                        c.iron,
                        k_mask_normal);
      desc.add_cylinder(QVector3D(x, top + 0.34F, z),
                        QVector3D(x, top + 0.46F, z),
                        0.11F,
                        c.iron,
                        k_mask_normal);
      desc.add_box(QVector3D(x, top + 0.49F, z),
                   QVector3D(0.07F, 0.04F, 0.07F),
                   c.ember,
                   k_mask_normal);
      desc.add_cone(QVector3D(x, top + 0.50F, z),
                    QVector3D(x, top + 0.70F, z),
                    0.06F,
                    c.flame,
                    k_mask_normal);
    }
    seed += 9;
  }
}

void add_donjon(BuildingArchetypeDesc& desc,
                const CarthagePalette& c,
                const Heights& h) {
  if (h.destroyed) {
    return;
  }
  const float cz = mid(k_donjon_z0, k_donjon_z1);
  const float hz = half(k_donjon_z0, k_donjon_z1);
  const float base_y = k_keep_top - 0.02F;
  const float top = h.donjon;
  const float band_y = base_y + (top - base_y) * 0.55F;

  desc.add_box(QVector3D(0.0F, mid(base_y, band_y), cz),
               QVector3D(k_donjon_hx, half(base_y, band_y), hz),
               c.sandstone,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, mid(band_y, top), cz),
               QVector3D(k_donjon_hx - 0.01F, half(band_y, top), hz - 0.01F),
               c.plaster,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, band_y + 0.03F, cz),
               QVector3D(k_donjon_hx + 0.012F, 0.025F, hz + 0.012F),
               c.indigo,
               k_mask_intact);
  for (const float qx : {-k_donjon_hx, k_donjon_hx}) {
    for (const float qz : {k_donjon_z0, k_donjon_z1}) {
      desc.add_box(QVector3D(qx, mid(base_y, top), qz),
                   QVector3D(0.07F, half(base_y, top), 0.07F),
                   c.sandstone_dark,
                   k_mask_intact);
    }
  }
  desc.add_box(QVector3D(0.0F, top - 0.115F, cz),
               QVector3D(k_donjon_hx + 0.02F, 0.012F, hz + 0.02F),
               c.brick_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, top - 0.070F, cz),
               QVector3D(k_donjon_hx + 0.035F, 0.032F, hz + 0.035F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, top - 0.024F, cz),
               QVector3D(k_donjon_hx + 0.080F, 0.032F, hz + 0.080F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, top + 0.015F, cz),
               QVector3D(k_donjon_hx - 0.05F, 0.015F, hz - 0.05F),
               c.sandstone_dark,
               k_mask_intact);

  add_slit_z_face(desc, c, -0.28F, base_y + 0.40F, k_donjon_z1, 1.0F);
  add_slit_z_face(desc, c, 0.28F, base_y + 0.40F, k_donjon_z1, 1.0F);
  add_slit_x_face(desc, c, -k_donjon_hx, base_y + 0.40F, cz, -1.0F);
  add_slit_x_face(desc, c, k_donjon_hx, base_y + 0.40F, cz, 1.0F);

  const float merlon_y = top + 0.07F;
  int seed = 71;
  for (const float mx : {-0.46F, -0.16F, 0.16F, 0.46F}) {
    const int front_seed = seed++;
    add_merlon(desc,
               c,
               QVector3D(mx, merlon_y, k_donjon_z1 - 0.02F),
               QVector3D(0.075F, 0.07F, 0.06F),
               0.0F,
               (front_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
               front_seed);
    const int back_seed = seed++;
    add_merlon(desc,
               c,
               QVector3D(mx, merlon_y, k_donjon_z0 + 0.02F),
               QVector3D(0.075F, 0.07F, 0.06F),
               0.0F,
               (back_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
               back_seed);
  }
  for (const float mz : {-1.08F, -0.81F, -0.54F}) {
    for (const float side : {-1.0F, 1.0F}) {
      const int merlon_seed = seed++;
      add_merlon(desc,
                 c,
                 QVector3D(side * (k_donjon_hx - 0.02F), merlon_y, mz),
                 QVector3D(0.06F, 0.07F, 0.075F),
                 0.0F,
                 (merlon_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
                 merlon_seed);
    }
  }

  add_punic_tanit_relief(
      desc,
      QVector3D(0.0F, mid(base_y, top) + 0.02F, k_donjon_z1 + 0.012F),
      BuildingFacadePlane::XY,
      0.34F,
      c.bronze,
      c.brick_dark,
      k_mask_intact);

  add_punic_horned_crown(desc,
                         QVector3D(0.0F, top + 0.03F, cz),
                         0.92F,
                         c.iron,
                         c.bronze,
                         c.ember,
                         k_mask_normal);
}

void add_gatehouse(BuildingArchetypeDesc& desc,
                   const CarthagePalette& c,
                   const Heights& h) {
  const float cz = mid(k_gate_z0, k_gate_z1);
  const float hz = half(k_gate_z0, k_gate_z1);
  const float pier_top = h.destroyed ? h.gate : k_gate_top;

  for (const float side : {-1.0F, 1.0F}) {
    const float px = side * mid(0.30F, k_gate_hx);
    desc.add_box(QVector3D(px, mid(k_platform_top, pier_top), cz),
                 QVector3D(half(0.30F, k_gate_hx), half(k_platform_top, pier_top), hz),
                 c.sandstone);
    desc.add_box(QVector3D(px, mid(k_platform_top, 0.50F), cz),
                 QVector3D(half(0.30F, k_gate_hx) + 0.03F,
                           half(k_platform_top, 0.50F),
                           hz + 0.03F),
                 c.stone_dark);
    desc.add_box(QVector3D(side * (k_gate_hx - 0.05F),
                           mid(k_platform_top, pier_top - 0.05F),
                           k_gate_z1 + 0.02F),
                 QVector3D(0.05F, half(k_platform_top, pier_top - 0.05F), 0.025F),
                 c.sandstone_dark);
  }

  desc.add_box(QVector3D(0.0F, mid(k_platform_top, 1.14F), 0.96F),
               QVector3D(0.30F, half(k_platform_top, 1.14F), 0.035F),
               c.wood_dark);
  desc.add_box(QVector3D(0.0F, mid(k_platform_top, 1.14F), 0.99F),
               QVector3D(0.006F, half(k_platform_top, 1.14F), 0.01F),
               c.shadow);
  for (const float sy : {0.40F, 0.72F, 1.04F}) {
    desc.add_box(QVector3D(0.0F, sy, 0.998F),
                 QVector3D(0.29F, 0.022F, 0.008F),
                 c.bronze,
                 k_mask_intact);
    for (const float sx : {-0.22F, -0.08F, 0.08F, 0.22F}) {
      desc.add_box(QVector3D(sx, sy, 1.006F),
                   QVector3D(0.014F, 0.014F, 0.006F),
                   c.iron,
                   k_mask_normal);
    }
  }

  if (h.destroyed) {
    return;
  }

  desc.add_box(QVector3D(0.0F, mid(k_gate_arch_y, h.gate), cz),
               QVector3D(k_gate_hx, half(k_gate_arch_y, h.gate), hz),
               c.sandstone,
               k_mask_intact);
  desc.add_cylinder(QVector3D(0.0F, k_gate_arch_y, k_gate_z0 + 0.02F),
                    QVector3D(0.0F, k_gate_arch_y, k_gate_z1 + 0.005F),
                    0.31F,
                    c.sandstone_dark,
                    k_mask_intact);
  desc.add_cylinder(QVector3D(0.0F, k_gate_arch_y, k_gate_z1 - 0.06F),
                    QVector3D(0.0F, k_gate_arch_y, k_gate_z1 + 0.012F),
                    0.345F,
                    c.stone_dark,
                    k_mask_intact);
  desc.add_box(QVector3D(0.0F, k_gate_arch_y + 0.36F, k_gate_z1 + 0.02F),
               QVector3D(0.06F, 0.07F, 0.02F),
               c.brick_dark,
               k_mask_intact);

  desc.add_box(QVector3D(0.0F, h.gate - 0.115F, cz),
               QVector3D(k_gate_hx + 0.02F, 0.012F, hz + 0.02F),
               c.brick_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.gate - 0.070F, cz),
               QVector3D(k_gate_hx + 0.035F, 0.032F, hz + 0.035F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, h.gate - 0.024F, cz),
               QVector3D(k_gate_hx + 0.080F, 0.032F, hz + 0.080F),
               c.sandstone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(0.0F, k_keep_ashlar_top + 0.05F, cz),
               QVector3D(k_gate_hx + 0.012F, 0.03F, hz + 0.012F),
               c.indigo,
               k_mask_intact);

  add_punic_tanit_relief(desc,
                         QVector3D(0.0F, h.gate - 0.36F, k_gate_z1 + 0.012F),
                         BuildingFacadePlane::XY,
                         0.30F,
                         c.bronze,
                         c.brick_dark,
                         k_mask_intact);

  const float merlon_y = h.gate + 0.07F;
  int seed = 91;
  for (const float mx : {-0.46F, -0.16F, 0.16F, 0.46F}) {
    const int merlon_seed = seed++;
    add_merlon(desc,
               c,
               QVector3D(mx, merlon_y, k_gate_z1 - 0.02F),
               QVector3D(0.075F, 0.07F, 0.06F),
               0.0F,
               (merlon_seed % 2 == 0) ? k_mask_intact : k_mask_normal,
               merlon_seed);
  }
  for (const float side : {-1.0F, 1.0F}) {
    add_merlon(desc,
               c,
               QVector3D(side * (k_gate_hx - 0.02F), merlon_y, cz + 0.08F),
               QVector3D(0.06F, 0.07F, 0.075F),
               0.0F,
               k_mask_intact,
               seed++);
  }

  const float awn_top_y = k_gate_arch_y - 0.02F;
  const float awn_top_z = k_gate_z1 + 0.01F;
  const float awn_low_y = 1.26F;
  const float awn_low_z = 1.50F;
  const float run = awn_low_z - awn_top_z;
  const float drop = awn_top_y - awn_low_y;
  const float tilt_deg = std::atan2(drop, run) * 180.0F / k_pi;
  const float len = std::sqrt(run * run + drop * drop);
  desc.add_rotated_box(
      QVector3D(0.0F, mid(awn_top_y, awn_low_y), mid(awn_top_z, awn_low_z)),
      QVector3D(0.52F, 0.012F, len * 0.5F),
      QVector3D(tilt_deg, 0.0F, 0.0F),
      c.oxblood,
      k_mask_normal);
  for (const float stripe_x : {-0.36F, 0.0F, 0.36F}) {
    desc.add_rotated_box(QVector3D(stripe_x,
                                   mid(awn_top_y, awn_low_y) + 0.012F,
                                   mid(awn_top_z, awn_low_z)),
                         QVector3D(0.05F, 0.006F, len * 0.5F - 0.01F),
                         QVector3D(tilt_deg, 0.0F, 0.0F),
                         c.saffron,
                         k_mask_normal);
  }
  desc.add_palette_box(QVector3D(0.0F, awn_low_y - 0.06F, awn_low_z),
                       QVector3D(0.52F, 0.06F, 0.008F),
                       k_team_slot,
                       k_mask_normal);
  desc.add_box(QVector3D(0.0F, awn_low_y - 0.125F, awn_low_z),
               QVector3D(0.52F, 0.012F, 0.010F),
               c.bronze,
               k_mask_normal);
  for (const float px : {-0.50F, 0.50F}) {
    desc.add_cylinder(QVector3D(px, k_platform_top, awn_low_z - 0.02F),
                      QVector3D(px, awn_low_y + 0.02F, awn_low_z - 0.02F),
                      0.022F,
                      c.bronze,
                      k_mask_normal);
    desc.add_box(QVector3D(px, k_platform_top + 0.03F, awn_low_z - 0.02F),
                 QVector3D(0.06F, 0.03F, 0.06F),
                 c.stone_dark,
                 k_mask_normal);
  }
}

void add_drill_yard(BuildingArchetypeDesc& desc,
                    const CarthagePalette& c,
                    const Heights& h) {
  const float rack_x = -0.95F;
  const float rack_z = 1.12F;
  for (const float dx : {-0.30F, 0.30F}) {
    desc.add_box(QVector3D(rack_x + dx, mid(k_platform_top, 1.06F), rack_z),
                 QVector3D(0.03F, half(k_platform_top, 1.06F), 0.03F),
                 c.wood_dark,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(rack_x, 1.02F, rack_z),
               QVector3D(0.34F, 0.02F, 0.03F),
               c.wood_dark,
               k_mask_intact);
  desc.add_box(QVector3D(rack_x, k_platform_top + 0.06F, rack_z + 0.10F),
               QVector3D(0.34F, 0.02F, 0.08F),
               c.wood_dark,
               k_mask_intact);
  for (const float dx : {-0.24F, -0.14F, -0.04F, 0.06F, 0.16F, 0.26F}) {
    desc.add_cylinder(QVector3D(rack_x + dx, k_platform_top, rack_z + 0.18F),
                      QVector3D(rack_x + dx, k_platform_top + 1.75F, rack_z - 0.04F),
                      0.012F,
                      c.wood,
                      k_mask_normal);
    desc.add_cone(QVector3D(rack_x + dx, k_platform_top + 1.75F, rack_z - 0.04F),
                  QVector3D(rack_x + dx, k_platform_top + 1.98F, rack_z - 0.07F),
                  0.022F,
                  c.bronze,
                  k_mask_normal);
  }

  const float lean = 16.0F * k_pi / 180.0F;
  int i = 0;
  for (const float sx : {0.72F, 0.98F, 1.24F}) {
    const float r = 0.17F;
    const float cy = k_platform_top + r * std::cos(lean) + 0.005F;
    const float cz = 1.02F + static_cast<float>(i) * 0.02F;
    const QVector3D axis(0.0F, std::sin(lean), std::cos(lean));
    const QVector3D center(sx, cy, cz);
    desc.add_cylinder(
        center - axis * 0.018F, center + axis * 0.018F, r, c.wood_dark, k_mask_normal);
    desc.add_palette_cylinder(center + axis * 0.018F,
                              center + axis * 0.030F,
                              r - 0.012F,
                              k_team_slot,
                              k_mask_normal);
    desc.add_cylinder(center + axis * 0.030F,
                      center + axis * 0.040F,
                      r * 0.55F,
                      c.bronze,
                      k_mask_normal);
    desc.add_cylinder(center + axis * 0.040F,
                      center + axis * 0.058F,
                      r * 0.16F,
                      c.bronze,
                      k_mask_normal);
    ++i;
  }

  const float post_x = 0.62F;
  const float post_z = 1.40F;
  desc.add_cylinder(
      QVector3D(post_x, k_platform_top, post_z),
      QVector3D(post_x, k_platform_top + (h.destroyed ? 0.50F : 1.42F), post_z),
      0.05F,
      c.wood);
  desc.add_box(QVector3D(post_x, k_platform_top + 0.03F, post_z),
               QVector3D(0.12F, 0.03F, 0.12F),
               c.stone_dark);
  desc.add_box(QVector3D(post_x + 0.16F, k_platform_top + 1.36F, post_z),
               QVector3D(0.18F, 0.02F, 0.02F),
               c.wood_dark,
               k_mask_intact);
  desc.add_cylinder(QVector3D(post_x + 0.30F, k_platform_top + 1.36F, post_z),
                    QVector3D(post_x + 0.30F, k_platform_top + 1.08F, post_z),
                    0.008F,
                    c.sack * 0.7F,
                    k_mask_normal);
  desc.add_box(QVector3D(post_x + 0.30F, k_platform_top + 0.90F, post_z),
               QVector3D(0.09F, 0.19F, 0.09F),
               c.sack,
               k_mask_normal);
  desc.add_box(QVector3D(post_x + 0.30F, k_platform_top + 1.07F, post_z),
               QVector3D(0.05F, 0.02F, 0.05F),
               c.wood_dark,
               k_mask_normal);

  const float braz_x = 1.30F;
  const float braz_z = 1.36F;
  for (int leg = 0; leg < 3; ++leg) {
    const float a = static_cast<float>(leg) * (2.0F * k_pi / 3.0F);
    desc.add_cylinder(QVector3D(braz_x + std::sin(a) * 0.14F,
                                k_platform_top,
                                braz_z + std::cos(a) * 0.14F),
                      QVector3D(braz_x + std::sin(a) * 0.05F,
                                k_platform_top + 0.58F,
                                braz_z + std::cos(a) * 0.05F),
                      0.014F,
                      c.iron,
                      k_mask_intact);
  }
  desc.add_cylinder(QVector3D(braz_x, k_platform_top + 0.52F, braz_z),
                    QVector3D(braz_x, k_platform_top + 0.64F, braz_z),
                    0.13F,
                    c.iron,
                    k_mask_intact);
  desc.add_box(QVector3D(braz_x, k_platform_top + 0.66F, braz_z),
               QVector3D(0.08F, 0.03F, 0.08F),
               c.ember,
               k_mask_normal);
  desc.add_cone(QVector3D(braz_x, k_platform_top + 0.67F, braz_z),
                QVector3D(braz_x, k_platform_top + 0.90F, braz_z),
                0.07F,
                c.flame,
                k_mask_normal);

  desc.add_box(QVector3D(-1.50F, k_platform_top + 0.11F, 1.10F),
               QVector3D(0.11F, 0.11F, 0.26F),
               c.stone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(-1.50F, k_platform_top + 0.20F, 1.10F),
               QVector3D(0.085F, 0.012F, 0.235F),
               QVector3D(0.28F, 0.40F, 0.46F),
               k_mask_intact);
}

void add_palm(BuildingArchetypeDesc& desc,
              const CarthagePalette& c,
              const QVector3D& base,
              float height,
              float lean_x,
              int seed) {
  constexpr int k_segments = 4;
  QVector3D from = base;
  for (int i = 0; i < k_segments; ++i) {
    const float t0 = static_cast<float>(i) / k_segments;
    const float t1 = static_cast<float>(i + 1) / k_segments;
    const QVector3D to(base.x() + lean_x * t1 * t1,
                       base.y() + height * t1,
                       base.z() + 0.04F * std::sin(static_cast<float>(seed + i)));
    desc.add_cylinder(from,
                      to,
                      0.062F - 0.012F * t0,
                      weathered(c.palm_trunk, seed + i, 0.03F),
                      k_mask_intact);
    from = to;
  }
  const QVector3D crown = from;
  desc.add_cylinder(crown - QVector3D(0.0F, 0.02F, 0.0F),
                    crown + QVector3D(0.0F, 0.06F, 0.0F),
                    0.07F,
                    c.frond_dark,
                    k_mask_intact);
  for (int i = 0; i < 9; ++i) {
    const float yaw = static_cast<float>(i) * (2.0F * k_pi / 9.0F) +
                      0.3F * static_cast<float>(seed % 3);
    const float droop = (i % 2 == 0) ? 0.62F : 0.36F;
    const float len = (i % 2 == 0) ? 0.62F : 0.52F;
    const QVector3D dir(std::sin(yaw) * std::cos(droop),
                        -std::sin(droop),
                        std::cos(yaw) * std::cos(droop));
    const QVector3D start = crown + QVector3D(0.0F, 0.05F, 0.0F) + dir * 0.04F;
    desc.add_rotated_box(start + dir * (len * 0.5F),
                         QVector3D(0.048F, 0.008F, len * 0.5F),
                         QVector3D(droop * 180.0F / k_pi, yaw * 180.0F / k_pi, 0.0F),
                         (i % 2 == 0) ? c.frond : c.frond_dark,
                         k_mask_intact);
  }
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(crown + QVector3D(side * 0.09F, -0.06F, 0.0F),
                 QVector3D(0.035F, 0.05F, 0.035F),
                 c.date,
                 k_mask_normal);
  }
}

void add_palms(BuildingArchetypeDesc& desc, const CarthagePalette& c) {
  add_palm(desc, c, QVector3D(1.74F, 0.0F, 1.44F), 2.05F, -0.10F, 3);
  add_palm(desc, c, QVector3D(-1.80F, 0.0F, -0.42F), 2.30F, 0.08F, 7);
}

void add_roof_canopy(BuildingArchetypeDesc& desc,
                     const CarthagePalette& c,
                     const Heights& h) {
  if (h.destroyed || h.damaged) {
    return;
  }
  const float deck_y = h.keep + 0.03F;
  const float cx = 0.78F;
  const float cz = 0.06F;
  const float hx = 0.30F;
  const float hz = 0.36F;
  const float post_top = deck_y + 0.46F;
  for (const float sx : {-hx, hx}) {
    for (const float sz : {-hz, hz}) {
      desc.add_cylinder(QVector3D(cx + sx, deck_y, cz + sz),
                        QVector3D(cx + sx, post_top, cz + sz),
                        0.018F,
                        c.wood,
                        k_mask_normal);
    }
  }
  desc.add_box(QVector3D(cx, post_top + 0.01F, cz),
               QVector3D(hx + 0.05F, 0.010F, hz + 0.05F),
               c.oxblood,
               k_mask_normal);
  for (const float sz : {-0.22F, 0.0F, 0.22F}) {
    desc.add_box(QVector3D(cx, post_top + 0.022F, cz + sz),
                 QVector3D(hx + 0.04F, 0.004F, 0.05F),
                 c.saffron,
                 k_mask_normal);
  }
  desc.add_palette_box(QVector3D(cx, post_top - 0.04F, cz + hz + 0.05F),
                       QVector3D(hx + 0.05F, 0.04F, 0.006F),
                       k_team_slot,
                       k_mask_normal);
  desc.add_box(QVector3D(cx + 0.02F, deck_y + 0.06F, cz - 0.05F),
               QVector3D(0.16F, 0.06F, 0.12F),
               c.sack,
               k_mask_normal);
}

auto build_barracks_archetype(BuildingState state) -> RenderArchetype {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("carthage_barracks");
  const Heights h = heights_for(state);

  add_platform(desc, c);
  add_keep(desc, c, h);
  add_towers(desc, c, h);
  add_donjon(desc, c, h);
  add_gatehouse(desc, c, h);
  add_drill_yard(desc, c, h);
  add_palms(desc, c);
  add_roof_canopy(desc, c, h);

  if (state == BuildingState::Damaged) {
    add_rubble_field(desc,
                     RubbleField{.center = QVector3D(0.0F, k_platform_top, 1.15F),
                                 .extent = QVector3D(1.1F, 0.0F, 0.28F),
                                 .stone = c.sandstone_dark,
                                 .stone_dark = c.stone_dark,
                                 .chunk_scale = 0.8F,
                                 .count = 6,
                                 .seed = 487,
                                 .states = BuildingStateMask::Damaged});
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.46F, 0.0F, 1.28F),
                                 .stone = c.sandstone_dark,
                                 .stone_dark = c.stone_dark,
                                 .timber = c.wood_dark,
                                 .ground_y = k_platform_top,
                                 .scale = 1.25F,
                                 .seed = 487});

  return build_building_archetype(desc, state);
}

auto barracks_archetype(BuildingState state,
                        Mesh*,
                        Texture*) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_barracks_archetype);
  return k_set.for_state(state);
}

void draw_standards(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const CarthagePalette& c,
                    const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(-1.70F, 0.0F, 1.34F),
       .pole_height = 2.9F,
       .pole_radius = 0.045F,
       .banner_width = 0.8F,
       .banner_height = 0.5F,
       .pole_color = c.wood,
       .beam_color = c.wood,
       .connector_color = c.sandstone_light,
       .ornament_offset = QVector3D(0.0F, 3.1F, 0.0F),
       .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
       .ornament_color = c.bronze,
       .ring_count = 3,
       .ring_y_start = 0.5F,
       .ring_spacing = 0.6F,
       .ring_height = 0.03F,
       .ring_radius_scale = 2.2F,
       .ring_color = c.bronze},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const CarthagePalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.wood,
                                                .timber_light = c.sandstone_light,
                                                .wood_dark = c.wood_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_stockpile_yard(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const CarthagePalette& c) {
  draw_barracks_stockpile(
      p,
      out,
      unit,
      white,
      StockpileYardStyle{.gravel = QVector3D(0.58F, 0.49F, 0.36F),
                         .earth_light = QVector3D(0.69F, 0.60F, 0.45F),
                         .stone_light = c.sandstone_light,
                         .stone_mid = c.sandstone,
                         .stone_dark = c.stone_dark,
                         .timber = c.wood,
                         .timber_dark = c.wood_dark,
                         .ore = c.iron * 1.55F,
                         .ore_rust = c.brick_dark});
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  CarthagePalette const c = make_palette(team);
  draw_stockpile_yard(p, out, unit, white, c);
  draw_standards(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "carthage",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .selection = BuildingSelectionStyle{2.4F, 2.0F}});
}

} // namespace Render::GL::Carthage
