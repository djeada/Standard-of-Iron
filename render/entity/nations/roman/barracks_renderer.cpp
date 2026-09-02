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

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_team_slot = 0;

constexpr auto k_mask_intact = k_building_state_mask_intact;
constexpr auto k_mask_normal = BuildingStateMask::Normal;
constexpr auto k_mask_damaged = BuildingStateMask::Damaged;

constexpr float k_pi = 3.14159265F;

struct RomanPalette {
  QVector3D plaster{0.87F, 0.82F, 0.70F};
  QVector3D plaster_shade{0.78F, 0.72F, 0.60F};
  QVector3D limestone{0.84F, 0.80F, 0.71F};
  QVector3D limestone_shade{0.74F, 0.70F, 0.61F};
  QVector3D limestone_dark{0.60F, 0.56F, 0.49F};
  QVector3D cedar{0.50F, 0.36F, 0.23F};
  QVector3D cedar_dark{0.33F, 0.22F, 0.13F};
  QVector3D oak{0.46F, 0.39F, 0.30F};
  QVector3D oak_pale{0.62F, 0.55F, 0.44F};
  QVector3D terracotta{0.72F, 0.33F, 0.19F};
  QVector3D terracotta_dark{0.48F, 0.16F, 0.09F};
  QVector3D dado{0.56F, 0.17F, 0.12F};
  QVector3D iron{0.30F, 0.30F, 0.32F};
  QVector3D bronze{0.62F, 0.46F, 0.24F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D shadow{0.12F, 0.09F, 0.07F};
  QVector3D water{0.30F, 0.42F, 0.50F};
  QVector3D turf{0.40F, 0.42F, 0.22F};
  QVector3D earth{0.46F, 0.36F, 0.24F};
  QVector3D earth_dark{0.27F, 0.21F, 0.14F};
  QVector3D leather{0.42F, 0.30F, 0.19F};
  QVector3D leather_dark{0.30F, 0.20F, 0.12F};
  QVector3D terracotta_light{0.80F, 0.42F, 0.26F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> RomanPalette {
  RomanPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

constexpr float k_platform_top = 0.16F;

constexpr float k_range_x0 = -0.62F;
constexpr float k_range_x1 = 1.18F;
constexpr float k_range_z0 = -1.58F;
constexpr float k_range_z1 = 0.30F;
constexpr float k_range_wall_top = 1.50F;
constexpr float k_range_roof_rise = 0.56F;
constexpr float k_range_roof_overhang = 0.12F;

constexpr float k_veranda_post_z = 1.06F;
constexpr float k_veranda_beam_y = 1.26F;
constexpr float k_veranda_eave_z = 1.18F;

constexpr float k_quarters_x0 = -1.58F;
constexpr float k_quarters_x1 = -0.72F;
constexpr float k_quarters_z0 = -1.58F;
constexpr float k_quarters_z1 = 0.95F;
constexpr float k_quarters_wall_top = 2.12F;
constexpr float k_quarters_roof_rise = 0.50F;
constexpr float k_quarters_overhang = 0.10F;

constexpr float k_tower_x = 1.50F;
constexpr float k_tower_z = -1.28F;
constexpr float k_tower_half = 0.20F;
constexpr float k_tower_deck_y = 2.34F;
constexpr float k_tower_eave_y = 2.86F;

constexpr float k_stub_wall_top = 0.66F;

inline auto mid(float a, float b) -> float {
  return (a + b) * 0.5F;
}
inline auto half(float a, float b) -> float {
  return (b - a) * 0.5F;
}

void add_platform(BuildingArchetypeDesc& desc, const RomanPalette& c) {
  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.74F, 0.04F, 1.54F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.66F, 0.02F, 1.46F), c.limestone_shade);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.60F, 0.02F, 1.42F), c.limestone);

  desc.add_box(
      QVector3D(0.0F, 0.04F, 1.68F), QVector3D(1.02F, 0.04F, 0.20F), c.limestone_dark);
  desc.add_box(QVector3D(0.0F, 0.10F, 1.54F),
               QVector3D(0.88F, 0.04F, 0.14F),
               c.limestone_shade,
               k_mask_intact);

  desc.add_box(QVector3D(0.55F, k_platform_top + 0.004F, 0.92F),
               QVector3D(1.10F, 0.004F, 0.54F),
               c.limestone_shade,
               k_mask_intact);
  for (const float gx : {-0.20F, 0.30F, 0.80F, 1.30F}) {
    desc.add_box(QVector3D(gx, k_platform_top + 0.006F, 0.92F),
                 QVector3D(0.006F, 0.004F, 0.54F),
                 c.limestone_dark,
                 k_mask_normal);
  }
}

void add_walls_and_dado(BuildingArchetypeDesc& desc,
                        const RomanPalette& c,
                        float x0,
                        float x1,
                        float z0,
                        float z1,
                        float wall_top) {
  const float cx = mid(x0, x1);
  const float cz = mid(z0, z1);
  const float hx = half(x0, x1);
  const float hz = half(z0, z1);
  desc.add_box(QVector3D(cx, mid(k_platform_top, wall_top), cz),
               QVector3D(hx, half(k_platform_top, wall_top), hz),
               c.plaster);
  desc.add_box(QVector3D(cx, k_platform_top + 0.05F, cz),
               QVector3D(hx + 0.016F, 0.05F, hz + 0.016F),
               c.limestone_dark);
  desc.add_box(QVector3D(cx, k_platform_top + 0.34F, cz),
               QVector3D(hx + 0.010F, 0.24F, hz + 0.010F),
               c.dado);
  for (const float qx : {x0, x1}) {
    for (const float qz : {z0, z1}) {
      desc.add_box(QVector3D(qx, mid(k_platform_top, wall_top), qz),
                   QVector3D(0.065F, half(k_platform_top, wall_top), 0.065F),
                   c.limestone_shade);
    }
  }
}

void add_window_x_face(BuildingArchetypeDesc& desc,
                       const RomanPalette& c,
                       float x,
                       float y,
                       float z,
                       float outward) {
  desc.add_box(QVector3D(x + outward * 0.012F, y, z),
               QVector3D(0.015F, 0.12F, 0.10F),
               c.shadow,
               k_mask_intact);
  desc.add_box(QVector3D(x + outward * 0.022F, y - 0.14F, z),
               QVector3D(0.022F, 0.02F, 0.13F),
               c.limestone_shade,
               k_mask_intact);
  desc.add_box(QVector3D(x + outward * 0.018F, y + 0.14F, z),
               QVector3D(0.018F, 0.02F, 0.12F),
               c.cedar_dark,
               k_mask_intact);
}

void add_window_z_face(BuildingArchetypeDesc& desc,
                       const RomanPalette& c,
                       float x,
                       float y,
                       float z,
                       float outward) {
  desc.add_box(QVector3D(x, y, z + outward * 0.012F),
               QVector3D(0.10F, 0.12F, 0.015F),
               c.shadow,
               k_mask_intact);
  desc.add_box(QVector3D(x, y - 0.14F, z + outward * 0.022F),
               QVector3D(0.13F, 0.02F, 0.022F),
               c.limestone_shade,
               k_mask_intact);
  desc.add_box(QVector3D(x, y + 0.14F, z + outward * 0.018F),
               QVector3D(0.12F, 0.02F, 0.018F),
               c.cedar_dark,
               k_mask_intact);
}

void add_door_z_face(BuildingArchetypeDesc& desc,
                     const RomanPalette& c,
                     float x,
                     float z,
                     float outward,
                     float half_w,
                     float height) {
  const float cy = k_platform_top + height * 0.5F;
  desc.add_box(QVector3D(x, cy, z + outward * 0.012F),
               QVector3D(half_w, height * 0.5F, 0.015F),
               c.shadow,
               k_mask_intact);
  for (const float fx : {-half_w - 0.03F, half_w + 0.03F}) {
    desc.add_box(QVector3D(x + fx, cy + 0.02F, z + outward * 0.024F),
                 QVector3D(0.03F, height * 0.5F + 0.02F, 0.026F),
                 c.cedar,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(x, k_platform_top + height + 0.05F, z + outward * 0.026F),
               QVector3D(half_w + 0.07F, 0.035F, 0.028F),
               c.cedar,
               k_mask_intact);
}

void add_pediment_z_face(BuildingArchetypeDesc& desc,
                         const QVector3D& color,
                         float cx,
                         float z,
                         float eave_y,
                         float half_w,
                         float rise) {
  constexpr int k_steps = 5;
  for (int i = 0; i < k_steps; ++i) {
    const float f0 = static_cast<float>(i) / static_cast<float>(k_steps);
    const float f1 = static_cast<float>(i + 1) / static_cast<float>(k_steps);
    const float y0 = eave_y + rise * f0;
    const float y1 = eave_y + rise * f1;
    desc.add_box(QVector3D(cx, mid(y0, y1), z),
                 QVector3D(half_w * (1.0F - f0), half(y0, y1), 0.05F),
                 color,
                 k_mask_intact);
  }
}

void add_pediment_x_face(BuildingArchetypeDesc& desc,
                         const QVector3D& color,
                         float x,
                         float cz,
                         float eave_y,
                         float half_d,
                         float rise) {
  constexpr int k_steps = 5;
  for (int i = 0; i < k_steps; ++i) {
    const float f0 = static_cast<float>(i) / static_cast<float>(k_steps);
    const float f1 = static_cast<float>(i + 1) / static_cast<float>(k_steps);
    const float y0 = eave_y + rise * f0;
    const float y1 = eave_y + rise * f1;
    desc.add_box(QVector3D(x, mid(y0, y1), cz),
                 QVector3D(0.05F, half(y0, y1), half_d * (1.0F - f0)),
                 color,
                 k_mask_intact);
  }
}

void add_scutum_z_face(BuildingArchetypeDesc& desc,
                       const RomanPalette& c,
                       const QVector3D& center,
                       float tilt_deg,
                       float scale,
                       BuildingStateMask states) {
  const QVector3D rot(tilt_deg, 0.0F, 0.0F);
  desc.add_rotated_box(
      center, QVector3D(0.13F, 0.21F, 0.016F) * scale, rot, c.cedar_dark, states);
  desc.add_palette_box(center + QVector3D(0.0F, 0.0F, 0.012F * scale),
                       QVector3D(0.115F, 0.195F, 0.010F) * scale,
                       k_team_slot,
                       states);
  desc.add_box(center + QVector3D(0.0F, 0.0F, 0.024F * scale),
               QVector3D(0.014F, 0.19F, 0.008F) * scale,
               c.bronze,
               states);
  desc.add_box(center + QVector3D(0.0F, 0.0F, 0.030F * scale),
               QVector3D(0.036F, 0.036F, 0.010F) * scale,
               c.gold,
               states);
}

void add_range(BuildingArchetypeDesc& desc,
               const RomanPalette& c,
               BuildingState state) {
  const bool destroyed = state == BuildingState::Destroyed;
  const float wall_top = destroyed ? k_stub_wall_top : k_range_wall_top;
  add_walls_and_dado(desc, c, k_range_x0, k_range_x1, k_range_z0, k_range_z1, wall_top);
  if (destroyed) {
    return;
  }

  const float cx = mid(k_range_x0, k_range_x1);
  const float cz = mid(k_range_z0, k_range_z1);
  desc.add_box(QVector3D(cx, k_range_wall_top - 0.030F, cz),
               QVector3D(half(k_range_x0, k_range_x1) + 0.04F,
                         0.038F,
                         half(k_range_z0, k_range_z1) + 0.04F),
               c.cedar_dark,
               k_mask_intact);

  for (const float wx : {-0.30F, 0.20F, 0.70F}) {
    add_window_z_face(desc, c, wx, 1.10F, k_range_z0, -1.0F);
  }
  add_window_x_face(desc, c, k_range_x1, 1.10F, -1.10F, 1.0F);
  add_window_x_face(desc, c, k_range_x1, 1.10F, -0.30F, 1.0F);

  for (const float dx : {-0.38F, 0.08F, 0.54F, 1.00F}) {
    add_door_z_face(desc, c, dx, k_range_z1, 1.0F, 0.10F, 0.92F);
  }
  for (const float sx : {-0.15F, 0.31F, 0.77F}) {
    add_scutum_z_face(
        desc, c, QVector3D(sx, 0.98F, k_range_z1 + 0.03F), 0.0F, 0.72F, k_mask_normal);
  }

  add_pediment_x_face(desc,
                      c.plaster_shade,
                      k_range_x1,
                      cz,
                      k_range_wall_top,
                      half(k_range_z0, k_range_z1),
                      k_range_roof_rise - 0.05F);
}

void add_veranda(BuildingArchetypeDesc& desc,
                 const RomanPalette& c,
                 BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }
  constexpr std::array<float, 5> k_post_x{-0.52F, -0.10F, 0.32F, 0.74F, 1.16F};
  for (std::size_t i = 0; i < k_post_x.size(); ++i) {
    const float px = k_post_x[i];
    const BuildingStateMask states = (i == 1 || i == 3) ? k_mask_normal : k_mask_intact;
    desc.add_box(QVector3D(px, k_platform_top + 0.04F, k_veranda_post_z),
                 QVector3D(0.085F, 0.04F, 0.085F),
                 c.limestone_shade,
                 states);
    desc.add_cylinder(QVector3D(px, k_platform_top + 0.08F, k_veranda_post_z),
                      QVector3D(px, k_veranda_beam_y, k_veranda_post_z),
                      0.052F,
                      c.cedar,
                      states);
    desc.add_box(QVector3D(px, k_veranda_beam_y - 0.03F, k_veranda_post_z),
                 QVector3D(0.08F, 0.03F, 0.08F),
                 c.cedar_dark,
                 states);
  }

  desc.add_box(QVector3D(mid(k_range_x0, k_range_x1 + 0.10F),
                         k_veranda_beam_y + 0.035F,
                         k_veranda_post_z),
               QVector3D(half(k_range_x0, k_range_x1 + 0.10F), 0.04F, 0.055F),
               c.cedar_dark,
               k_mask_intact);

  desc.add_box(QVector3D(mid(k_range_x0, k_range_x1),
                         k_platform_top + 0.02F,
                         mid(k_range_z1, k_veranda_post_z)),
               QVector3D(half(k_range_x0, k_range_x1),
                         0.02F,
                         half(k_range_z1, k_veranda_post_z)),
               c.plaster_shade,
               k_mask_intact);

  const float top_y = k_range_wall_top - 0.04F;
  const float top_z = k_range_z1;
  const float low_y = k_veranda_beam_y - 0.02F;
  const float low_z = k_veranda_eave_z;
  const float run = low_z - top_z;
  const float drop = top_y - low_y;
  const float theta_deg = std::atan2(drop, run) * 180.0F / k_pi;
  const float slope_len = std::sqrt(run * run + drop * drop);
  const QVector3D slope_center(
      mid(k_range_x0, k_range_x1 + 0.10F), mid(top_y, low_y), mid(top_z, low_z));
  const QVector3D rot(theta_deg, 0.0F, 0.0F);

  for (const float rx : {-0.55F, -0.28F, 0.0F, 0.28F, 0.56F, 0.84F, 1.12F}) {
    desc.add_rotated_box(QVector3D(rx, slope_center.y() - 0.03F, slope_center.z()),
                         QVector3D(0.025F, 0.025F, slope_len * 0.5F),
                         rot,
                         c.cedar_dark,
                         k_mask_intact);
  }
  desc.add_rotated_box(slope_center,
                       QVector3D(half(k_range_x0, k_range_x1 + 0.10F) + 0.06F,
                                 0.03F,
                                 slope_len * 0.5F + 0.04F),
                       rot,
                       c.terracotta,
                       k_mask_intact);
  const QVector3D normal(0.0F, std::cos(theta_deg * k_pi / 180.0F), 0.0F);
  for (const float f : {0.18F, 0.50F, 0.82F}) {
    const QVector3D at(
        slope_center.x(), top_y - drop * f + normal.y() * 0.04F, top_z + run * f);
    desc.add_rotated_box(
        at,
        QVector3D(half(k_range_x0, k_range_x1 + 0.10F) + 0.04F, 0.012F, 0.03F),
        rot,
        c.terracotta_dark,
        k_mask_normal);
  }
  desc.add_box(QVector3D(slope_center.x(), low_y + 0.02F, low_z + 0.02F),
               QVector3D(half(k_range_x0, k_range_x1 + 0.10F) + 0.06F, 0.025F, 0.03F),
               c.terracotta_dark,
               k_mask_intact);
}

void add_range_roof(BuildingArchetypeDesc& desc,
                    const RomanPalette& c,
                    BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }
  const float x0 = k_quarters_x1 + 0.02F;
  const float x1 = k_range_x1 + k_range_roof_overhang;
  const float cx = mid(x0, x1);
  const float hx = half(x0, x1);
  const float cz = mid(k_range_z0, k_range_z1);
  const float half_depth = half(k_range_z0, k_range_z1) + k_range_roof_overhang;
  const float eave_y = k_range_wall_top;
  const float theta = std::atan2(k_range_roof_rise, half_depth);
  const float theta_deg = theta * 180.0F / k_pi;
  const float slope_len =
      std::sqrt(half_depth * half_depth + k_range_roof_rise * k_range_roof_rise);
  const QVector3D slab(hx, 0.045F, slope_len * 0.5F + 0.02F);
  const float cy = eave_y + k_range_roof_rise * 0.5F;

  desc.add_rotated_box(QVector3D(cx, cy, cz + half_depth * 0.5F),
                       slab,
                       QVector3D(theta_deg, 0.0F, 0.0F),
                       c.terracotta,
                       k_mask_normal);
  desc.add_rotated_box(QVector3D(cx - hx * 0.5F, cy, cz + half_depth * 0.5F),
                       QVector3D(hx * 0.5F, 0.045F, slope_len * 0.5F + 0.02F),
                       QVector3D(theta_deg, 0.0F, 0.0F),
                       c.terracotta,
                       k_mask_damaged);
  desc.add_rotated_box(QVector3D(cx, cy, cz - half_depth * 0.5F),
                       slab,
                       QVector3D(-theta_deg, 0.0F, 0.0F),
                       c.terracotta * 0.96F,
                       k_mask_intact);

  for (const float rx : {-0.50F, -0.10F, 0.30F, 0.70F, 1.10F}) {
    desc.add_rotated_box(QVector3D(rx, cy + 0.02F, cz + half_depth * 0.5F),
                         QVector3D(0.03F, 0.03F, slope_len * 0.5F),
                         QVector3D(theta_deg, 0.0F, 0.0F),
                         c.cedar_dark,
                         k_mask_damaged);
  }

  const QVector3D normal(0.0F, std::cos(theta), std::sin(theta));
  for (const float f : {0.16F, 0.36F, 0.56F, 0.76F, 0.94F}) {
    const float y = eave_y + k_range_roof_rise * f;
    const float dz = half_depth * (1.0F - f);
    const QVector3D lift = normal * 0.05F;
    desc.add_rotated_box(QVector3D(cx, y + lift.y(), cz + dz + lift.z()),
                         QVector3D(hx - 0.01F, 0.012F, 0.032F),
                         QVector3D(theta_deg, 0.0F, 0.0F),
                         c.terracotta_dark,
                         k_mask_normal);
    desc.add_rotated_box(QVector3D(cx, y + lift.y(), cz - dz - lift.z()),
                         QVector3D(hx - 0.01F, 0.012F, 0.032F),
                         QVector3D(-theta_deg, 0.0F, 0.0F),
                         c.terracotta_dark,
                         k_mask_intact);
  }
  desc.add_box(QVector3D(cx, eave_y + k_range_roof_rise + 0.025F, cz),
               QVector3D(hx + 0.02F, 0.035F, 0.07F),
               c.terracotta_dark,
               k_mask_intact);
  for (const float ax : {-0.30F, 0.10F, 0.50F, 0.90F, 1.24F}) {
    desc.add_box(QVector3D(ax, eave_y + 0.035F, cz + half_depth + 0.01F),
                 QVector3D(0.035F, 0.04F, 0.015F),
                 c.terracotta_dark,
                 k_mask_normal);
  }
}

void add_quarters(BuildingArchetypeDesc& desc,
                  const RomanPalette& c,
                  BuildingState state) {
  const bool destroyed = state == BuildingState::Destroyed;
  const float wall_top = destroyed ? k_stub_wall_top + 0.10F : k_quarters_wall_top;
  add_walls_and_dado(
      desc, c, k_quarters_x0, k_quarters_x1, k_quarters_z0, k_quarters_z1, wall_top);
  if (destroyed) {
    return;
  }

  const float cx = mid(k_quarters_x0, k_quarters_x1);
  const float cz = mid(k_quarters_z0, k_quarters_z1);
  const float hx = half(k_quarters_x0, k_quarters_x1);
  const float hz = half(k_quarters_z0, k_quarters_z1);

  desc.add_box(QVector3D(cx, 1.30F, cz),
               QVector3D(hx + 0.035F, 0.028F, hz + 0.035F),
               c.limestone_shade,
               k_mask_intact);
  desc.add_box(QVector3D(cx, k_quarters_wall_top - 0.035F, cz),
               QVector3D(hx + 0.045F, 0.043F, hz + 0.045F),
               c.limestone_shade,
               k_mask_intact);

  add_door_z_face(desc, c, cx, k_quarters_z1, 1.0F, 0.15F, 1.00F);
  for (const float px : {cx - 0.30F, cx + 0.30F}) {
    desc.add_box(QVector3D(px, k_platform_top + 0.60F, k_quarters_z1 + 0.05F),
                 QVector3D(0.055F, 0.60F, 0.05F),
                 c.limestone,
                 k_mask_intact);
    desc.add_box(QVector3D(px, k_platform_top + 1.22F, k_quarters_z1 + 0.05F),
                 QVector3D(0.08F, 0.03F, 0.075F),
                 c.limestone_shade,
                 k_mask_intact);
    desc.add_box(QVector3D(px, k_platform_top + 0.03F, k_quarters_z1 + 0.05F),
                 QVector3D(0.08F, 0.03F, 0.075F),
                 c.limestone_shade,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(cx, k_platform_top + 1.27F, k_quarters_z1 + 0.05F),
               QVector3D(0.42F, 0.025F, 0.075F),
               c.limestone_shade,
               k_mask_intact);

  for (const float wx : {cx - 0.22F, cx + 0.22F}) {
    add_window_z_face(desc, c, wx, 1.74F, k_quarters_z1, 1.0F);
    add_window_z_face(desc, c, wx, 1.74F, k_quarters_z0, -1.0F);
  }
  for (const float wz : {-1.10F, -0.45F, 0.20F}) {
    add_window_x_face(desc, c, k_quarters_x0, 1.74F, wz, -1.0F);
    add_window_x_face(desc, c, k_quarters_x0, 0.98F, wz, -1.0F);
  }

  add_pediment_z_face(desc,
                      c.plaster_shade,
                      cx,
                      k_quarters_z1,
                      k_quarters_wall_top,
                      hx,
                      k_quarters_roof_rise - 0.05F);
  add_pediment_z_face(desc,
                      c.plaster_shade,
                      cx,
                      k_quarters_z0,
                      k_quarters_wall_top,
                      hx,
                      k_quarters_roof_rise - 0.05F);
  add_roman_aquila_relief(
      desc,
      QVector3D(cx, k_quarters_wall_top + 0.20F, k_quarters_z1 + 0.06F),
      BuildingFacadePlane::XY,
      0.40F,
      c.bronze,
      c.terracotta_dark,
      k_mask_intact);

  const float eave_y = k_quarters_wall_top;
  const float half_depth = hx + k_quarters_overhang;
  const float theta = std::atan2(k_quarters_roof_rise, half_depth);
  const float theta_deg = theta * 180.0F / k_pi;
  const float slope_len =
      std::sqrt(half_depth * half_depth + k_quarters_roof_rise * k_quarters_roof_rise);
  const float cy = eave_y + k_quarters_roof_rise * 0.5F;
  const QVector3D slab(slope_len * 0.5F + 0.02F, 0.045F, hz + k_quarters_overhang);
  desc.add_rotated_box(QVector3D(cx + half_depth * 0.5F, cy, cz),
                       slab,
                       QVector3D(0.0F, 0.0F, -theta_deg),
                       c.terracotta,
                       k_mask_intact);
  desc.add_rotated_box(QVector3D(cx - half_depth * 0.5F, cy, cz),
                       slab,
                       QVector3D(0.0F, 0.0F, theta_deg),
                       c.terracotta * 0.96F,
                       k_mask_normal);
  desc.add_rotated_box(QVector3D(cx - half_depth * 0.5F, cy, cz - hz * 0.5F),
                       QVector3D(slope_len * 0.5F + 0.02F, 0.045F, hz * 0.5F),
                       QVector3D(0.0F, 0.0F, theta_deg),
                       c.terracotta * 0.96F,
                       k_mask_damaged);
  for (const float rz : {-1.20F, -0.60F, 0.0F, 0.60F}) {
    desc.add_rotated_box(QVector3D(cx - half_depth * 0.5F, cy + 0.02F, rz),
                         QVector3D(slope_len * 0.5F, 0.03F, 0.03F),
                         QVector3D(0.0F, 0.0F, theta_deg),
                         c.cedar_dark,
                         k_mask_damaged);
  }
  const QVector3D normal(std::sin(theta), std::cos(theta), 0.0F);
  for (const float f : {0.18F, 0.42F, 0.66F, 0.90F}) {
    const float y = eave_y + k_quarters_roof_rise * f;
    const float dx = half_depth * (1.0F - f);
    const QVector3D lift = normal * 0.05F;
    desc.add_rotated_box(QVector3D(cx + dx + lift.x(), y + lift.y(), cz),
                         QVector3D(0.032F, 0.012F, hz + k_quarters_overhang - 0.01F),
                         QVector3D(0.0F, 0.0F, -theta_deg),
                         c.terracotta_dark,
                         k_mask_intact);
    desc.add_rotated_box(QVector3D(cx - dx - lift.x(), y + lift.y(), cz),
                         QVector3D(0.032F, 0.012F, hz + k_quarters_overhang - 0.01F),
                         QVector3D(0.0F, 0.0F, theta_deg),
                         c.terracotta_dark,
                         k_mask_normal);
  }
  desc.add_box(QVector3D(cx, eave_y + k_quarters_roof_rise + 0.025F, cz),
               QVector3D(0.07F, 0.035F, hz + k_quarters_overhang + 0.02F),
               c.terracotta_dark,
               k_mask_intact);

  add_roman_roof_standard(
      desc,
      QVector3D(cx, eave_y + k_quarters_roof_rise + 0.06F, k_quarters_z1 - 0.30F),
      0.82F,
      c.gold,
      c.terracotta_dark,
      k_mask_normal);
}

void add_watchtower(BuildingArchetypeDesc& desc,
                    const RomanPalette& c,
                    BuildingState state) {
  if (state == BuildingState::Destroyed) {
    for (const float sx : {-k_tower_half, k_tower_half}) {
      for (const float sz : {-k_tower_half, k_tower_half}) {
        desc.add_cylinder(QVector3D(k_tower_x + sx, k_platform_top, k_tower_z + sz),
                          QVector3D(k_tower_x + sx,
                                    k_platform_top + 0.45F + (sx > 0.0F ? 0.25F : 0.0F),
                                    k_tower_z + sz),
                          0.045F,
                          c.oak,
                          BuildingStateMask::Destroyed);
      }
    }
    return;
  }

  const float post_top = k_tower_eave_y + 0.02F;
  for (const float sx : {-k_tower_half, k_tower_half}) {
    for (const float sz : {-k_tower_half, k_tower_half}) {
      desc.add_box(QVector3D(k_tower_x + sx, k_platform_top + 0.03F, k_tower_z + sz),
                   QVector3D(0.075F, 0.03F, 0.075F),
                   c.limestone_shade,
                   k_mask_intact);
      desc.add_cylinder(QVector3D(k_tower_x + sx, k_platform_top, k_tower_z + sz),
                        QVector3D(k_tower_x + sx, post_top, k_tower_z + sz),
                        0.045F,
                        c.oak,
                        k_mask_intact);
    }
  }

  const float brace_len = std::sqrt(0.40F * 0.40F + 0.80F * 0.80F);
  const float brace_deg = std::atan2(0.80F, 0.40F) * 180.0F / k_pi;
  for (const float y_mid : {k_platform_top + 0.60F, k_platform_top + 1.44F}) {
    for (const float fz : {-k_tower_half, k_tower_half}) {
      for (const float sign : {1.0F, -1.0F}) {
        desc.add_rotated_box(QVector3D(k_tower_x, y_mid, k_tower_z + fz),
                             QVector3D(brace_len * 0.5F, 0.02F, 0.02F),
                             QVector3D(0.0F, 0.0F, sign * brace_deg),
                             c.oak,
                             k_mask_intact);
      }
    }
    for (const float fx : {-k_tower_half, k_tower_half}) {
      for (const float sign : {1.0F, -1.0F}) {
        desc.add_rotated_box(QVector3D(k_tower_x + fx, y_mid, k_tower_z),
                             QVector3D(0.02F, 0.02F, brace_len * 0.5F),
                             QVector3D(sign * brace_deg, 0.0F, 0.0F),
                             c.oak,
                             k_mask_intact);
      }
    }
  }
  for (const float ty : {k_platform_top + 1.02F, k_platform_top + 1.86F}) {
    desc.add_box(QVector3D(k_tower_x, ty, k_tower_z),
                 QVector3D(k_tower_half + 0.03F, 0.025F, 0.025F),
                 c.cedar_dark,
                 k_mask_intact);
    desc.add_box(QVector3D(k_tower_x, ty, k_tower_z),
                 QVector3D(0.025F, 0.025F, k_tower_half + 0.03F),
                 c.cedar_dark,
                 k_mask_intact);
  }

  const float rail_x = k_tower_x - k_tower_half - 0.045F;
  for (const float lz : {k_tower_z - 0.09F, k_tower_z + 0.09F}) {
    desc.add_box(QVector3D(rail_x, mid(k_platform_top, k_tower_deck_y), lz),
                 QVector3D(0.015F, half(k_platform_top, k_tower_deck_y), 0.015F),
                 c.cedar,
                 k_mask_normal);
  }
  for (float ry = k_platform_top + 0.24F; ry < k_tower_deck_y - 0.05F; ry += 0.26F) {
    desc.add_box(QVector3D(rail_x, ry, k_tower_z),
                 QVector3D(0.012F, 0.012F, 0.09F),
                 c.cedar,
                 k_mask_normal);
  }

  desc.add_box(QVector3D(k_tower_x, k_tower_deck_y, k_tower_z),
               QVector3D(k_tower_half + 0.13F, 0.03F, k_tower_half + 0.13F),
               c.cedar,
               k_mask_intact);
  desc.add_box(QVector3D(k_tower_x, k_tower_deck_y - 0.05F, k_tower_z),
               QVector3D(k_tower_half + 0.08F, 0.025F, k_tower_half + 0.08F),
               c.cedar_dark,
               k_mask_intact);
  const float par_half = k_tower_half + 0.13F;
  const float par_y = k_tower_deck_y + 0.18F;
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(k_tower_x, par_y, k_tower_z + side * par_half),
                 QVector3D(par_half, 0.15F, 0.012F),
                 c.oak_pale,
                 k_mask_intact);
    desc.add_box(QVector3D(k_tower_x + side * par_half, par_y, k_tower_z),
                 QVector3D(0.012F, 0.15F, par_half),
                 c.oak_pale,
                 k_mask_intact);
    desc.add_box(QVector3D(k_tower_x, par_y + 0.17F, k_tower_z + side * par_half),
                 QVector3D(par_half + 0.01F, 0.02F, 0.022F),
                 c.cedar_dark,
                 k_mask_intact);
    desc.add_box(QVector3D(k_tower_x + side * par_half, par_y + 0.17F, k_tower_z),
                 QVector3D(0.022F, 0.02F, par_half + 0.01F),
                 c.cedar_dark,
                 k_mask_intact);
  }

  desc.add_palette_box(
      QVector3D(k_tower_x, par_y + 0.02F, k_tower_z + par_half + 0.03F),
      QVector3D(0.17F, 0.13F, 0.010F),
      k_team_slot,
      k_mask_normal);
  desc.add_box(QVector3D(k_tower_x, par_y - 0.12F, k_tower_z + par_half + 0.03F),
               QVector3D(0.17F, 0.018F, 0.012F),
               c.gold,
               k_mask_normal);
  desc.add_box(QVector3D(k_tower_x, par_y + 0.16F, k_tower_z + par_half + 0.03F),
               QVector3D(0.20F, 0.015F, 0.015F),
               c.cedar_dark,
               k_mask_normal);

  const float roof_half = k_tower_half + 0.18F;
  const float roof_rise = 0.26F;
  add_gable_roof_x(
      [&](const QVector3D& center,
          const QVector3D& scale,
          const QVector3D& euler,
          const QVector3D& color) {
        desc.add_rotated_box(center, scale, euler, color, k_mask_normal);
      },
      k_tower_x,
      k_tower_z,
      k_tower_eave_y,
      roof_half,
      roof_half,
      roof_rise,
      0.035F,
      c.terracotta,
      0.02F);
  desc.add_box(QVector3D(k_tower_x, k_tower_eave_y + roof_rise + 0.02F, k_tower_z),
               QVector3D(roof_half + 0.04F, 0.03F, 0.05F),
               c.terracotta_dark,
               k_mask_normal);
  desc.add_box(QVector3D(k_tower_x, k_tower_eave_y - 0.01F, k_tower_z),
               QVector3D(k_tower_half + 0.04F, 0.025F, k_tower_half + 0.04F),
               c.cedar_dark,
               k_mask_intact);
}

void add_rampart(BuildingArchetypeDesc& desc,
                 const RomanPalette& c,
                 BuildingState state) {
  const bool destroyed = state == BuildingState::Destroyed;

  desc.add_box(QVector3D(0.0F, 0.09F, -1.66F), QVector3D(1.72F, 0.07F, 0.15F), c.earth);
  desc.add_box(QVector3D(0.0F, 0.17F, -1.66F), QVector3D(1.68F, 0.012F, 0.11F), c.turf);
  desc.add_box(
      QVector3D(-1.66F, 0.09F, -0.16F), QVector3D(0.15F, 0.07F, 1.52F), c.earth);
  desc.add_box(
      QVector3D(-1.66F, 0.17F, -0.16F), QVector3D(0.11F, 0.012F, 1.48F), c.turf);
  desc.add_box(
      QVector3D(1.74F, 0.09F, -0.05F), QVector3D(0.13F, 0.07F, 1.02F), c.earth);
  desc.add_box(
      QVector3D(1.74F, 0.17F, -0.05F), QVector3D(0.09F, 0.012F, 0.98F), c.turf);

  desc.add_box(
      QVector3D(0.0F, 0.006F, -1.86F), QVector3D(1.78F, 0.006F, 0.06F), c.earth_dark);
  desc.add_box(
      QVector3D(-1.86F, 0.006F, -0.16F), QVector3D(0.06F, 0.006F, 1.58F), c.earth_dark);

  if (destroyed) {
    return;
  }
  int index = 0;
  const auto stake = [&](float x, float z) {
    const BuildingStateMask states = (index % 3 == 1) ? k_mask_normal : k_mask_intact;
    const float jitter = std::sin(static_cast<float>(index) * 7.31F) * 0.04F;
    const float top = 0.92F + jitter;
    desc.add_cylinder(QVector3D(x, 0.12F, z),
                      QVector3D(x, top, z),
                      0.034F,
                      weathered(c.oak, index, 0.10F),
                      states);
    desc.add_cone(QVector3D(x, top, z),
                  QVector3D(x, top + 0.16F, z),
                  0.036F,
                  weathered(c.oak_pale, index, 0.10F),
                  states);
    ++index;
  };
  for (float x = -1.60F; x <= 1.62F; x += 0.175F) {
    stake(x, -1.66F);
  }
  for (float z = -1.50F; z <= 1.30F; z += 0.175F) {
    stake(-1.66F, z);
  }
  for (float z = -0.95F; z <= 0.85F; z += 0.18F) {
    stake(1.74F, z);
  }
  desc.add_box(QVector3D(0.0F, 0.66F, -1.66F),
               QVector3D(1.64F, 0.02F, 0.02F),
               c.cedar_dark,
               k_mask_intact);
  desc.add_box(QVector3D(-1.66F, 0.66F, -0.10F),
               QVector3D(0.02F, 0.02F, 1.42F),
               c.cedar_dark,
               k_mask_intact);
  desc.add_box(QVector3D(1.74F, 0.66F, -0.05F),
               QVector3D(0.02F, 0.02F, 0.94F),
               c.cedar_dark,
               k_mask_intact);
}

void add_drill_yard(BuildingArchetypeDesc& desc,
                    const RomanPalette& c,
                    BuildingState state) {
  const bool destroyed = state == BuildingState::Destroyed;

  const float palus_x = 1.32F;
  const float palus_z = 0.92F;
  desc.add_cylinder(
      QVector3D(palus_x, k_platform_top, palus_z),
      QVector3D(palus_x, k_platform_top + (destroyed ? 0.55F : 1.34F), palus_z),
      0.065F,
      c.oak);
  desc.add_cylinder(QVector3D(palus_x, k_platform_top + 0.40F, palus_z),
                    QVector3D(palus_x, k_platform_top + 0.46F, palus_z),
                    0.072F,
                    c.iron,
                    k_mask_intact);
  desc.add_cylinder(QVector3D(palus_x, k_platform_top + 1.00F, palus_z),
                    QVector3D(palus_x, k_platform_top + 1.06F, palus_z),
                    0.072F,
                    c.iron,
                    k_mask_intact);
  desc.add_box(QVector3D(palus_x, k_platform_top + 0.04F, palus_z),
               QVector3D(0.16F, 0.04F, 0.16F),
               c.limestone_dark);

  const float rack_x = 0.58F;
  const float rack_z = 1.34F;
  for (const float dx : {-0.34F, 0.34F}) {
    desc.add_box(QVector3D(rack_x + dx, k_platform_top + 0.36F, rack_z),
                 QVector3D(0.03F, 0.36F, 0.03F),
                 c.cedar_dark,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(rack_x, k_platform_top + 0.70F, rack_z),
               QVector3D(0.38F, 0.02F, 0.03F),
               c.cedar_dark,
               k_mask_intact);
  for (const float dx : {-0.25F, -0.15F, -0.05F, 0.05F, 0.15F, 0.25F}) {
    desc.add_cylinder(QVector3D(rack_x + dx, k_platform_top, rack_z + 0.12F),
                      QVector3D(rack_x + dx, k_platform_top + 1.40F, rack_z - 0.02F),
                      0.012F,
                      c.cedar,
                      k_mask_normal);
    desc.add_cylinder(QVector3D(rack_x + dx, k_platform_top + 1.40F, rack_z - 0.02F),
                      QVector3D(rack_x + dx, k_platform_top + 1.72F, rack_z - 0.05F),
                      0.007F,
                      c.iron,
                      k_mask_normal);
    desc.add_cone(QVector3D(rack_x + dx, k_platform_top + 1.72F, rack_z - 0.05F),
                  QVector3D(rack_x + dx, k_platform_top + 1.84F, rack_z - 0.06F),
                  0.016F,
                  c.iron,
                  k_mask_normal);
  }

  const float shield_x = -0.20F;
  const float shield_z = 1.36F;
  for (const float dx : {-0.30F, 0.30F}) {
    desc.add_box(QVector3D(shield_x + dx, k_platform_top + 0.30F, shield_z),
                 QVector3D(0.03F, 0.30F, 0.03F),
                 c.cedar_dark,
                 k_mask_intact);
  }
  desc.add_box(QVector3D(shield_x, k_platform_top + 0.58F, shield_z),
               QVector3D(0.34F, 0.02F, 0.03F),
               c.cedar_dark,
               k_mask_intact);
  for (const float dx : {-0.19F, 0.0F, 0.19F}) {
    add_scutum_z_face(
        desc,
        c,
        QVector3D(shield_x + dx, k_platform_top + 0.30F, shield_z + 0.06F),
        -12.0F,
        0.78F,
        k_mask_normal);
  }

  const float trough_x = 1.54F;
  const float trough_z = 0.34F;
  desc.add_box(QVector3D(trough_x, k_platform_top + 0.12F, trough_z),
               QVector3D(0.13F, 0.12F, 0.28F),
               c.limestone_dark,
               k_mask_intact);
  desc.add_box(QVector3D(trough_x, k_platform_top + 0.22F, trough_z),
               QVector3D(0.10F, 0.012F, 0.25F),
               c.water,
               k_mask_intact);

  const float tent_x = -0.32F;
  const float tent_z = 0.66F;
  const float tent_rise = 0.40F;
  const float tent_half_z = 0.24F;
  const float tent_half_x = 0.28F;
  const float tent_deg = std::atan2(tent_rise, tent_half_z) * 180.0F / k_pi;
  const float tent_len = std::sqrt(tent_half_z * tent_half_z + tent_rise * tent_rise);
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(QVector3D(tent_x,
                                   k_platform_top + tent_rise * 0.5F,
                                   tent_z + side * tent_half_z * 0.5F),
                         QVector3D(tent_half_x, 0.012F, tent_len * 0.5F),
                         QVector3D(side * tent_deg, 0.0F, 0.0F),
                         side > 0.0F ? c.leather : c.leather_dark,
                         k_mask_normal);
  }
  desc.add_cylinder(
      QVector3D(
          tent_x - tent_half_x - 0.02F, k_platform_top + tent_rise + 0.01F, tent_z),
      QVector3D(
          tent_x + tent_half_x + 0.02F, k_platform_top + tent_rise + 0.01F, tent_z),
      0.016F,
      c.cedar_dark,
      k_mask_normal);
  for (const float ex : {tent_x - tent_half_x, tent_x + tent_half_x}) {
    desc.add_cylinder(QVector3D(ex, k_platform_top, tent_z),
                      QVector3D(ex, k_platform_top + tent_rise, tent_z),
                      0.014F,
                      c.cedar_dark,
                      k_mask_normal);
  }
  desc.add_box(QVector3D(tent_x + tent_half_x + 0.005F, k_platform_top + 0.14F, tent_z),
               QVector3D(0.004F, 0.14F, 0.10F),
               c.shadow,
               k_mask_normal);

  const float altar_x = mid(k_quarters_x0, k_quarters_x1);
  const float altar_z = 1.26F;
  desc.add_box(QVector3D(altar_x, k_platform_top + 0.02F, altar_z),
               QVector3D(0.13F, 0.02F, 0.13F),
               c.limestone_shade,
               k_mask_intact);
  desc.add_box(QVector3D(altar_x, k_platform_top + 0.15F, altar_z),
               QVector3D(0.09F, 0.11F, 0.09F),
               c.limestone,
               k_mask_intact);
  desc.add_box(QVector3D(altar_x, k_platform_top + 0.28F, altar_z),
               QVector3D(0.12F, 0.02F, 0.12F),
               c.limestone_shade,
               k_mask_intact);
  desc.add_box(QVector3D(altar_x, k_platform_top + 0.19F, altar_z + 0.095F),
               QVector3D(0.05F, 0.04F, 0.006F),
               c.dado,
               k_mask_intact);
  desc.add_cylinder(QVector3D(altar_x, k_platform_top + 0.30F, altar_z),
                    QVector3D(altar_x, k_platform_top + 0.33F, altar_z),
                    0.06F,
                    c.terracotta_dark,
                    k_mask_normal);
  desc.add_cone(QVector3D(altar_x, k_platform_top + 0.33F, altar_z),
                QVector3D(altar_x, k_platform_top + 0.50F, altar_z),
                0.045F,
                QVector3D(0.95F, 0.58F, 0.14F),
                k_mask_normal);
}

void add_imbrex_lines(BuildingArchetypeDesc& desc,
                      const RomanPalette& c,
                      BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }
  {
    const float cz = mid(k_range_z0, k_range_z1);
    const float half_depth = half(k_range_z0, k_range_z1) + k_range_roof_overhang;
    const float theta = std::atan2(k_range_roof_rise, half_depth);
    const float theta_deg = theta * 180.0F / k_pi;
    const float slope_len =
        std::sqrt(half_depth * half_depth + k_range_roof_rise * k_range_roof_rise);
    const float cy = k_range_wall_top + k_range_roof_rise * 0.5F;
    const QVector3D lift = QVector3D(0.0F, std::cos(theta), std::sin(theta)) * 0.058F;
    for (float x = -0.58F; x <= 1.26F; x += 0.23F) {
      desc.add_rotated_box(
          QVector3D(x, cy + lift.y(), cz + half_depth * 0.5F + lift.z()),
          QVector3D(0.022F, 0.012F, slope_len * 0.5F - 0.03F),
          QVector3D(theta_deg, 0.0F, 0.0F),
          c.terracotta_light,
          k_mask_normal);
      desc.add_rotated_box(
          QVector3D(x, cy + lift.y(), cz - half_depth * 0.5F - lift.z()),
          QVector3D(0.022F, 0.012F, slope_len * 0.5F - 0.03F),
          QVector3D(-theta_deg, 0.0F, 0.0F),
          c.terracotta_light,
          k_mask_intact);
    }
  }
  {
    const float cx = mid(k_quarters_x0, k_quarters_x1);
    const float hx = half(k_quarters_x0, k_quarters_x1);
    const float half_depth = hx + k_quarters_overhang;
    const float theta = std::atan2(k_quarters_roof_rise, half_depth);
    const float theta_deg = theta * 180.0F / k_pi;
    const float slope_len = std::sqrt(half_depth * half_depth +
                                      k_quarters_roof_rise * k_quarters_roof_rise);
    const float cy = k_quarters_wall_top + k_quarters_roof_rise * 0.5F;
    const QVector3D lift = QVector3D(std::sin(theta), std::cos(theta), 0.0F) * 0.058F;
    for (float z = k_quarters_z0 - 0.02F; z <= k_quarters_z1 + 0.04F; z += 0.25F) {
      desc.add_rotated_box(
          QVector3D(cx + half_depth * 0.5F + lift.x(), cy + lift.y(), z),
          QVector3D(slope_len * 0.5F - 0.03F, 0.012F, 0.022F),
          QVector3D(0.0F, 0.0F, -theta_deg),
          c.terracotta_light,
          k_mask_intact);
      desc.add_rotated_box(
          QVector3D(cx - half_depth * 0.5F - lift.x(), cy + lift.y(), z),
          QVector3D(slope_len * 0.5F - 0.03F, 0.012F, 0.022F),
          QVector3D(0.0F, 0.0F, theta_deg),
          c.terracotta_light,
          k_mask_normal);
    }
  }
}

auto build_barracks_archetype(BuildingState state) -> RenderArchetype {
  RomanPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("roman_barracks");

  add_platform(desc, c);
  add_range(desc, c, state);
  add_veranda(desc, c, state);
  add_range_roof(desc, c, state);
  add_quarters(desc, c, state);
  add_imbrex_lines(desc, c, state);
  add_watchtower(desc, c, state);
  add_rampart(desc, c, state);
  add_drill_yard(desc, c, state);

  if (state == BuildingState::Damaged) {
    add_rubble_field(desc,
                     RubbleField{.center = QVector3D(0.6F, k_platform_top, 0.75F),
                                 .extent = QVector3D(0.9F, 0.0F, 0.30F),
                                 .stone = c.terracotta_dark,
                                 .stone_dark = c.limestone_dark,
                                 .chunk_scale = 0.8F,
                                 .count = 6,
                                 .seed = 449,
                                 .states = BuildingStateMask::Damaged});
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.46F, 0.0F, 1.28F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.cedar_dark * 0.6F,
                                 .ground_y = 0.16F,
                                 .scale = 1.25F,
                                 .seed = 443});

  return build_building_archetype(desc, state);
}

auto barracks_archetype(BuildingState state,
                        Mesh*,
                        Texture*) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_barracks_archetype);
  return k_set.for_state(state);
}

void draw_vexillum(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(-1.50F, 0.0F, 1.42F),
       .pole_height = 3.0F,
       .pole_radius = 0.045F,
       .banner_width = 0.9F,
       .banner_height = 0.6F,
       .pole_color = c.cedar,
       .beam_color = c.cedar,
       .connector_color = c.limestone,
       .ornament_offset = QVector3D(0.25F, 3.15F, 0.03F),
       .ornament_size = QVector3D(0.35F, 0.03F, 0.015F),
       .ornament_color = c.gold,
       .ring_count = 4,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.5F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = c.gold},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const RomanPalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.cedar,
                                                .timber_light = c.limestone,
                                                .wood_dark = c.cedar_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_stockpile_yard(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const RomanPalette& c) {
  draw_barracks_stockpile(
      p,
      out,
      unit,
      white,
      StockpileYardStyle{.gravel = QVector3D(0.62F, 0.55F, 0.44F),
                         .earth_light = QVector3D(0.72F, 0.65F, 0.53F),
                         .stone_light = c.limestone,
                         .stone_mid = c.limestone_shade,
                         .stone_dark = c.limestone_dark,
                         .timber = c.cedar,
                         .timber_dark = c.cedar_dark,
                         .ore = QVector3D(0.35F, 0.34F, 0.36F),
                         .ore_rust = QVector3D(0.46F, 0.28F, 0.16F)});
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  RomanPalette const c = make_palette(team);
  draw_stockpile_yard(p, out, unit, white, c);
  draw_vexillum(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "roman",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .selection = BuildingSelectionStyle{2.6F, 2.2F}});
}

} // namespace Render::GL::Roman
