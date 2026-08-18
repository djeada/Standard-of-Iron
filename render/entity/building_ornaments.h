#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "building_archetype_desc.h"

namespace Render::GL {

enum class BuildingFacadePlane {
  XY,
  ZY,
};

inline auto
weathered(const QVector3D& color, int seed, float amount = 0.06F) -> QVector3D {
  const float n = std::sin(static_cast<float>(seed) * 12.9898F) * 43758.5453F;
  const float frac = n - std::floor(n);
  const float delta = (frac - 0.5F) * 2.0F * amount;
  const float scale = 1.0F + delta;
  return QVector3D(std::clamp(color.x() * scale, 0.0F, 1.0F),
                   std::clamp(color.y() * scale, 0.0F, 1.0F),
                   std::clamp(color.z() * scale, 0.0F, 1.0F));
}

template <typename AddBox>
void add_merlon_strip_x(AddBox&& add_box,
                        float y,
                        float z,
                        float start_x,
                        float spacing,
                        int count,
                        const QVector3D& half_size,
                        const QVector3D& color) {
  for (int i = 0; i < count; ++i) {
    add_box(
        QVector3D(start_x + spacing * static_cast<float>(i), y, z), half_size, color);
  }
}

template <typename AddBox>
void add_merlon_strip_z(AddBox&& add_box,
                        float x,
                        float y,
                        float start_z,
                        float spacing,
                        int count,
                        const QVector3D& half_size,
                        const QVector3D& color) {
  for (int i = 0; i < count; ++i) {
    add_box(
        QVector3D(x, y, start_z + spacing * static_cast<float>(i)), half_size, color);
  }
}

template <typename AddBox>
void add_square_parapet(AddBox&& add_box,
                        float y,
                        float half_extent,
                        int per_side,
                        const QVector3D& merlon_half,
                        const QVector3D& color,
                        int seed = 0) {

  const int steps = std::max(per_side, 2);
  const float span = half_extent * 2.0F;
  const float spacing = span / static_cast<float>(steps - 1);
  int index = seed;
  for (int i = 0; i < steps; ++i) {
    const float along = -half_extent + spacing * static_cast<float>(i);
    add_box(QVector3D(along, y, -half_extent), merlon_half, weathered(color, index++));
    add_box(QVector3D(along, y, half_extent), merlon_half, weathered(color, index++));
  }

  for (int i = 1; i < steps - 1; ++i) {
    const float along = -half_extent + spacing * static_cast<float>(i);
    add_box(QVector3D(-half_extent, y, along), merlon_half, weathered(color, index++));
    add_box(QVector3D(half_extent, y, along), merlon_half, weathered(color, index++));
  }
}

template <typename AddBox>
void add_embrasures(AddBox&& add_box,
                    float y,
                    float face_offset,
                    const QVector3D& slit_half,
                    const QVector3D& color) {

  add_box(QVector3D(0.0F, y, face_offset),
          QVector3D(slit_half.x(), slit_half.y(), slit_half.z()),
          color);
  add_box(QVector3D(0.0F, y, -face_offset),
          QVector3D(slit_half.x(), slit_half.y(), slit_half.z()),
          color);
  add_box(QVector3D(face_offset, y, 0.0F),
          QVector3D(slit_half.z(), slit_half.y(), slit_half.x()),
          color);
  add_box(QVector3D(-face_offset, y, 0.0F),
          QVector3D(slit_half.z(), slit_half.y(), slit_half.x()),
          color);
}

template <typename AddBox>
void add_tile_rows_z(AddBox&& add_box,
                     float y,
                     float start_z,
                     float end_z,
                     float spacing,
                     const QVector3D& half_size,
                     const QVector3D& color) {
  if (spacing == 0.0F) {
    return;
  }

  const float step = std::fabs(spacing);
  const float direction = (end_z >= start_z) ? 1.0F : -1.0F;
  for (float z = start_z;
       (direction > 0.0F) ? (z <= end_z + 0.001F) : (z >= end_z - 0.001F);
       z += direction * step) {
    add_box(QVector3D(0.0F, y, z), half_size, color);
  }
}

template <typename AddRotatedBox>
void add_gable_roof_x(AddRotatedBox&& add_rot,
                      float center_x,
                      float center_z,
                      float eave_y,
                      float half_span_x,
                      float half_depth_z,
                      float rise,
                      float half_thick,
                      const QVector3D& color,
                      float overhang = 0.0F) {
  const float theta = std::atan2(rise, half_depth_z);
  const float theta_deg = theta * 180.0F / 3.14159265F;
  const float slope = std::sqrt(half_depth_z * half_depth_z + rise * rise);
  const float half_len = slope * 0.5F + overhang;
  const QVector3D scale(half_span_x + overhang, half_thick, half_len);
  const float cy = eave_y + rise * 0.5F;
  add_rot(QVector3D(center_x, cy, center_z + half_depth_z * 0.5F),
          scale,
          QVector3D(theta_deg, 0.0F, 0.0F),
          color);
  add_rot(QVector3D(center_x, cy, center_z - half_depth_z * 0.5F),
          scale,
          QVector3D(-theta_deg, 0.0F, 0.0F),
          color);
}

template <typename AddRotatedBox>
void add_gable_roof_z(AddRotatedBox&& add_rot,
                      float center_x,
                      float center_z,
                      float eave_y,
                      float half_span_z,
                      float half_depth_x,
                      float rise,
                      float half_thick,
                      const QVector3D& color,
                      float overhang = 0.0F) {
  const float theta = std::atan2(rise, half_depth_x);
  const float theta_deg = theta * 180.0F / 3.14159265F;
  const float slope = std::sqrt(half_depth_x * half_depth_x + rise * rise);
  const float half_len = slope * 0.5F + overhang;
  const QVector3D scale(half_len, half_thick, half_span_z + overhang);
  const float cy = eave_y + rise * 0.5F;
  add_rot(QVector3D(center_x + half_depth_x * 0.5F, cy, center_z),
          scale,
          QVector3D(0.0F, 0.0F, -theta_deg),
          color);
  add_rot(QVector3D(center_x - half_depth_x * 0.5F, cy, center_z),
          scale,
          QVector3D(0.0F, 0.0F, theta_deg),
          color);
}

namespace Detail {

inline auto facade_point(const QVector3D& center,
                         BuildingFacadePlane plane,
                         float horizontal,
                         float vertical,
                         float normal_offset = 0.0F) -> QVector3D {
  if (plane == BuildingFacadePlane::XY) {
    return center + QVector3D(horizontal, vertical, normal_offset);
  }
  return center + QVector3D(normal_offset, vertical, horizontal);
}

inline auto facade_scale(BuildingFacadePlane plane,
                         float horizontal,
                         float vertical,
                         float depth) -> QVector3D {
  if (plane == BuildingFacadePlane::XY) {
    return {horizontal, vertical, depth};
  }
  return {depth, vertical, horizontal};
}

inline auto facade_rotation(BuildingFacadePlane plane, float degrees) -> QVector3D {
  if (plane == BuildingFacadePlane::XY) {
    return {0.0F, 0.0F, degrees};
  }
  return {degrees, 0.0F, 0.0F};
}

struct EagleFeather {
  float horizontal;
  float vertical;
  float half_length;
  float half_thickness;
  float sweep_degrees;
};

inline constexpr std::array<EagleFeather, 4> k_eagle_wing{{
    {0.115F, 0.055F, 0.150F, 0.062F, 8.0F},
    {0.255F, 0.105F, 0.130F, 0.048F, 24.0F},
    {0.370F, 0.170F, 0.105F, 0.036F, 42.0F},
    {0.455F, 0.245F, 0.075F, 0.026F, 58.0F},
}};

inline constexpr std::array<EagleFeather, 3> k_eagle_tail{{
    {0.000F, -0.205F, 0.100F, 0.030F, 0.0F},
    {0.058F, -0.190F, 0.090F, 0.026F, 16.0F},
    {0.115F, -0.160F, 0.075F, 0.022F, 30.0F},
}};

inline void add_eagle_silhouette(BuildingArchetypeDesc& desc,
                                 const QVector3D& center,
                                 BuildingFacadePlane plane,
                                 float scale,
                                 float depth,
                                 const QVector3D& bronze,
                                 const QVector3D& shade,
                                 BuildingStateMask states) {
  const auto point = [&](float horizontal, float vertical, float normal) {
    return facade_point(center, plane, horizontal * scale, vertical * scale, normal);
  };
  const auto size = [&](float horizontal, float vertical, float thickness) {
    return facade_scale(plane, horizontal * scale, vertical * scale, thickness);
  };

  const float relief = depth * 0.55F;

  for (float const side : {-1.0F, 1.0F}) {
    for (auto const& feather : k_eagle_wing) {
      desc.add_rotated_box(point(side * feather.horizontal, feather.vertical, relief),
                           size(feather.half_length, feather.half_thickness, relief),
                           facade_rotation(plane, side * feather.sweep_degrees),
                           shade,
                           states);
      desc.add_rotated_box(
          point(side * feather.horizontal, feather.vertical + 0.022F, depth),
          size(feather.half_length * 0.88F, feather.half_thickness * 0.72F, depth),
          facade_rotation(plane, side * feather.sweep_degrees),
          bronze,
          states);
    }

    desc.add_rotated_box(point(side * 0.085F, 0.115F, depth),
                         size(0.085F, 0.050F, depth),
                         facade_rotation(plane, side * 34.0F),
                         bronze,
                         states);
  }

  for (auto const& feather : k_eagle_tail) {
    for (float const side : {-1.0F, 1.0F}) {
      if (feather.horizontal == 0.0F && side < 0.0F) {
        continue;
      }
      desc.add_rotated_box(point(side * feather.horizontal, feather.vertical, depth),
                           size(feather.half_thickness, feather.half_length, depth),
                           facade_rotation(plane, side * feather.sweep_degrees),
                           bronze,
                           states);
    }
  }

  desc.add_box(
      point(0.0F, -0.015F, depth), size(0.072F, 0.155F, depth * 1.25F), bronze, states);
  desc.add_box(point(0.0F, 0.075F, depth * 1.15F),
               size(0.055F, 0.070F, depth * 1.35F),
               bronze,
               states);

  desc.add_box(point(0.018F, 0.180F, depth * 1.2F),
               size(0.046F, 0.055F, depth * 1.4F),
               bronze,
               states);
  desc.add_cone(point(0.048F, 0.184F, depth * 1.2F),
                point(0.104F, 0.156F, depth * 1.2F),
                0.020F * scale,
                bronze,
                states);
}

inline void add_laurel_wreath(BuildingArchetypeDesc& desc,
                              const QVector3D& center,
                              BuildingFacadePlane plane,
                              float scale,
                              float depth,
                              float radius,
                              const QVector3D& color,
                              BuildingStateMask states) {
  constexpr int k_leaves = 14;
  for (int index = 0; index < k_leaves; ++index) {
    float const t = static_cast<float>(index) / static_cast<float>(k_leaves);
    float const angle = (t * 6.2831853F) - 1.5707963F;

    if (std::sin(angle) > 0.72F) {
      continue;
    }
    float const horizontal = std::cos(angle) * radius;
    float const vertical = std::sin(angle) * radius;
    desc.add_rotated_box(
        facade_point(center, plane, horizontal * scale, vertical * scale, depth * 0.6F),
        facade_scale(plane, 0.052F * scale, 0.024F * scale, depth * 0.6F),
        facade_rotation(plane, (angle * 180.0F / 3.14159265F) + 90.0F),
        color,
        states);
  }
}

inline void add_tanit_sign(BuildingArchetypeDesc& desc,
                           const QVector3D& center,
                           BuildingFacadePlane plane,
                           float scale,
                           float depth,
                           const QVector3D& symbol,
                           const QVector3D& shade,
                           BuildingStateMask states) {
  const auto point = [&](float horizontal, float vertical, float normal) {
    return facade_point(center, plane, horizontal * scale, vertical * scale, normal);
  };
  const auto size = [&](float horizontal, float vertical, float thickness) {
    return facade_scale(plane, horizontal * scale, vertical * scale, thickness);
  };

  desc.add_cylinder(point(0.0F, 0.235F, depth * 0.2F),
                    point(0.0F, 0.235F, depth * 1.9F),
                    0.098F * scale,
                    symbol,
                    states);
  desc.add_cylinder(point(0.0F, 0.235F, 0.0F),
                    point(0.0F, 0.235F, depth * 0.35F),
                    0.114F * scale,
                    shade,
                    states);

  desc.add_box(point(0.0F, 0.085F, depth), size(0.235F, 0.032F, depth), symbol, states);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(point(side * 0.272F, 0.124F, depth),
                         size(0.075F, 0.030F, depth),
                         facade_rotation(plane, side * -38.0F),
                         symbol,
                         states);
  }

  constexpr std::array<std::pair<float, float>, 4> k_body{
      {{0.060F, 0.020F}, {0.104F, -0.058F}, {0.148F, -0.136F}, {0.192F, -0.214F}}};
  for (auto const& course : k_body) {
    desc.add_box(point(0.0F, course.second, depth),
                 size(course.first, 0.042F, depth),
                 symbol,
                 states);
  }
  desc.add_box(point(0.0F, -0.262F, depth), size(0.215F, 0.030F, depth), shade, states);
}

} // namespace Detail

inline void
add_roman_aquila_relief(BuildingArchetypeDesc& desc,
                        const QVector3D& center,
                        BuildingFacadePlane plane,
                        float scale,
                        const QVector3D& bronze,
                        const QVector3D& shadow,
                        BuildingStateMask states = k_building_state_mask_intact) {
  const float depth = 0.028F * scale;

  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.0F),
               Detail::facade_scale(plane, 0.58F * scale, 0.54F * scale, depth * 0.5F),
               weathered(shadow, 3, 0.10F),
               states);

  Detail::add_laurel_wreath(desc, center, plane, scale, depth, 0.36F, bronze, states);
  Detail::add_eagle_silhouette(
      desc, center, plane, scale, depth, bronze, shadow, states);
}

inline void
add_punic_tanit_relief(BuildingArchetypeDesc& desc,
                       const QVector3D& center,
                       BuildingFacadePlane plane,
                       float scale,
                       const QVector3D& symbol,
                       const QVector3D& backing,
                       BuildingStateMask states = k_building_state_mask_intact) {
  const float depth = 0.028F * scale;

  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.0F),
               Detail::facade_scale(plane, 0.58F * scale, 0.74F * scale, depth * 0.7F),
               backing,
               states);
  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.0F, depth * 0.25F),
               Detail::facade_scale(plane, 0.48F * scale, 0.64F * scale, depth * 0.35F),
               weathered(backing, 7, 0.12F),
               states);

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(
        Detail::facade_point(center, plane, side * 0.52F * scale, 0.0F, depth * 0.3F),
        Detail::facade_scale(plane, 0.032F * scale, 0.72F * scale, depth * 0.45F),
        symbol,
        states);
  }

  Detail::add_tanit_sign(desc, center, plane, scale, depth, symbol, backing, states);
}

inline void
add_roman_roof_standard(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        float scale,
                        const QVector3D& bronze,
                        const QVector3D& crimson,
                        BuildingStateMask states = k_building_state_mask_intact) {

  desc.add_box(base + QVector3D(0.0F, 0.030F * scale, 0.0F),
               QVector3D(0.150F, 0.030F, 0.120F) * scale,
               crimson,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.078F * scale, 0.0F),
               QVector3D(0.100F, 0.022F, 0.082F) * scale,
               bronze,
               states);
  desc.add_cylinder(base + QVector3D(0.0F, 0.085F * scale, 0.0F),
                    base + QVector3D(0.0F, 0.560F * scale, 0.0F),
                    0.026F * scale,
                    bronze,
                    states);

  desc.add_box(base + QVector3D(0.0F, 0.330F * scale, 0.0F),
               QVector3D(0.150F, 0.088F, 0.030F) * scale,
               crimson,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.330F * scale, 0.033F * scale),
               QVector3D(0.098F, 0.044F, 0.011F) * scale,
               bronze,
               states);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(base + QVector3D(side * 0.135F * scale, 0.238F * scale, 0.0F),
                 QVector3D(0.016F, 0.030F, 0.016F) * scale,
                 bronze,
                 states);
  }

  desc.add_cylinder(base + QVector3D(0.0F, 0.560F * scale, -0.02F * scale),
                    base + QVector3D(0.0F, 0.560F * scale, 0.02F * scale),
                    0.058F * scale,
                    bronze,
                    states);

  Detail::add_eagle_silhouette(desc,
                               base + QVector3D(0.0F, 0.700F * scale, 0.0F),
                               BuildingFacadePlane::XY,
                               scale * 0.68F,
                               0.030F * scale,
                               bronze,
                               crimson,
                               states);
}

inline void
add_punic_horned_crown(BuildingArchetypeDesc& desc,
                       const QVector3D& base,
                       float scale,
                       const QVector3D& obsidian,
                       const QVector3D& bronze,
                       const QVector3D& ember,
                       BuildingStateMask states = k_building_state_mask_intact) {

  desc.add_box(base + QVector3D(0.0F, 0.030F * scale, 0.0F),
               QVector3D(0.170F, 0.028F, 0.140F) * scale,
               obsidian,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.078F * scale, 0.0F),
               QVector3D(0.118F, 0.026F, 0.098F) * scale,
               bronze,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.128F * scale, 0.0F),
               QVector3D(0.076F, 0.032F, 0.070F) * scale,
               obsidian,
               states);

  desc.add_cylinder(base + QVector3D(0.0F, 0.150F * scale, 0.0F),
                    base + QVector3D(0.0F, 0.620F * scale, 0.0F),
                    0.040F * scale,
                    obsidian,
                    states);
  desc.add_box(base + QVector3D(0.0F, 0.370F * scale, 0.0F),
               QVector3D(0.058F, 0.090F, 0.058F) * scale,
               ember,
               states);

  desc.add_box(base + QVector3D(0.0F, 0.690F * scale, 0.0F),
               QVector3D(0.105F, 0.100F, 0.105F) * scale,
               bronze,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.690F * scale, 0.0F),
               QVector3D(0.128F, 0.038F, 0.128F) * scale,
               ember,
               states);

  constexpr int k_crescent_steps = 9;
  constexpr float k_crescent_span = 0.74F;
  for (int index = 0; index < k_crescent_steps; ++index) {
    float const t =
        static_cast<float>(index) / static_cast<float>(k_crescent_steps - 1);
    float const angle = 3.14159265F * (k_crescent_span * (t - 0.5F));
    float const radius = 0.255F * scale;
    desc.add_rotated_box(
        base + QVector3D(std::sin(angle) * radius,
                         (0.900F * scale) + (std::cos(angle) * radius * 0.55F),
                         0.0F),
        QVector3D(0.036F, 0.052F, 0.036F) * scale,
        QVector3D(0.0F, 0.0F, -(angle * 180.0F / 3.14159265F)),
        bronze,
        states);
  }
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cone(base + QVector3D(side * 0.222F * scale, 0.928F * scale, 0.0F),
                  base + QVector3D(side * 0.262F * scale, 1.115F * scale, 0.0F),
                  0.036F * scale,
                  bronze,
                  states);
  }
}

} // namespace Render::GL
