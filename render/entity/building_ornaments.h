#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>

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

} // namespace Detail

inline void
add_roman_aquila_relief(BuildingArchetypeDesc& desc,
                        const QVector3D& center,
                        BuildingFacadePlane plane,
                        float scale,
                        const QVector3D& bronze,
                        const QVector3D& shadow,
                        BuildingStateMask states = k_building_state_mask_intact) {
  const float depth = 0.025F * scale;
  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.0F),
               Detail::facade_scale(plane, 0.62F * scale, 0.42F * scale, depth),
               shadow,
               states,
               BuildingLODMask::Full);

  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.01F, depth),
               Detail::facade_scale(plane, 0.08F * scale, 0.25F * scale, depth),
               bronze,
               states,
               BuildingLODMask::Full);
  desc.add_box(
      Detail::facade_point(center, plane, 0.035F * scale, 0.17F * scale, depth),
      Detail::facade_scale(plane, 0.10F * scale, 0.09F * scale, depth),
      bronze,
      states,
      BuildingLODMask::Full);

  for (float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(
        Detail::facade_point(center, plane, side * 0.16F * scale, 0.06F * scale, depth),
        Detail::facade_scale(plane, 0.27F * scale, 0.065F * scale, depth),
        Detail::facade_rotation(plane, side * 18.0F),
        bronze,
        states,
        BuildingLODMask::Full);
    desc.add_rotated_box(
        Detail::facade_point(center, plane, side * 0.24F * scale, 0.13F * scale, depth),
        Detail::facade_scale(plane, 0.20F * scale, 0.055F * scale, depth),
        Detail::facade_rotation(plane, side * 38.0F),
        bronze,
        states,
        BuildingLODMask::Full);
    desc.add_rotated_box(
        Detail::facade_point(
            center, plane, side * 0.045F * scale, -0.15F * scale, depth),
        Detail::facade_scale(plane, 0.055F * scale, 0.16F * scale, depth),
        Detail::facade_rotation(plane, side * 20.0F),
        bronze,
        states,
        BuildingLODMask::Full);
  }
}

inline void
add_punic_tanit_relief(BuildingArchetypeDesc& desc,
                       const QVector3D& center,
                       BuildingFacadePlane plane,
                       float scale,
                       const QVector3D& symbol,
                       const QVector3D& backing,
                       BuildingStateMask states = k_building_state_mask_intact) {
  const float depth = 0.025F * scale;
  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.0F),
               Detail::facade_scale(plane, 0.48F * scale, 0.52F * scale, depth),
               backing,
               states,
               BuildingLODMask::Full);
  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.08F * scale, depth),
               Detail::facade_scale(plane, 0.38F * scale, 0.045F * scale, depth),
               symbol,
               states,
               BuildingLODMask::Full);
  desc.add_box(Detail::facade_point(center, plane, 0.0F, 0.20F * scale, depth),
               Detail::facade_scale(plane, 0.11F * scale, 0.11F * scale, depth),
               symbol,
               states,
               BuildingLODMask::Full);
  for (float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(
        Detail::facade_point(
            center, plane, side * 0.075F * scale, -0.08F * scale, depth),
        Detail::facade_scale(plane, 0.055F * scale, 0.27F * scale, depth),
        Detail::facade_rotation(plane, side * 31.0F),
        symbol,
        states,
        BuildingLODMask::Full);
  }
  desc.add_box(Detail::facade_point(center, plane, 0.0F, -0.21F * scale, depth),
               Detail::facade_scale(plane, 0.30F * scale, 0.045F * scale, depth),
               symbol,
               states,
               BuildingLODMask::Full);
}

inline void
add_roman_roof_standard(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        float scale,
                        const QVector3D& bronze,
                        const QVector3D& crimson,
                        BuildingStateMask states = k_building_state_mask_intact) {
  desc.add_box(base + QVector3D(0.0F, 0.035F * scale, 0.0F),
               QVector3D(0.16F, 0.035F, 0.13F) * scale,
               crimson,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.09F * scale, 0.0F),
               QVector3D(0.11F, 0.025F, 0.09F) * scale,
               bronze,
               states);
  desc.add_cylinder(base + QVector3D(0.0F, 0.10F * scale, 0.0F),
                    base + QVector3D(0.0F, 0.58F * scale, 0.0F),
                    0.028F * scale,
                    bronze,
                    states);

  desc.add_box(base + QVector3D(0.0F, 0.37F * scale, 0.0F),
               QVector3D(0.15F, 0.09F, 0.035F) * scale,
               crimson,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.37F * scale, 0.038F * scale),
               QVector3D(0.10F, 0.045F, 0.012F) * scale,
               bronze,
               states,
               BuildingLODMask::Full);

  for (float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(base + QVector3D(side * 0.15F * scale, 0.60F * scale, 0.0F),
                         QVector3D(0.18F, 0.045F, 0.055F) * scale,
                         QVector3D(0.0F, 0.0F, side * 18.0F),
                         bronze,
                         states);
    desc.add_rotated_box(base + QVector3D(side * 0.25F * scale, 0.66F * scale, 0.0F),
                         QVector3D(0.11F, 0.035F, 0.045F) * scale,
                         QVector3D(0.0F, 0.0F, side * 36.0F),
                         bronze,
                         states,
                         BuildingLODMask::Full);
  }
  desc.add_box(base + QVector3D(0.0F, 0.62F * scale, 0.0F),
               QVector3D(0.065F, 0.13F, 0.065F) * scale,
               bronze,
               states);
  desc.add_box(base + QVector3D(0.035F * scale, 0.73F * scale, 0.0F),
               QVector3D(0.055F, 0.045F, 0.055F) * scale,
               bronze,
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
  desc.add_box(base + QVector3D(0.0F, 0.035F * scale, 0.0F),
               QVector3D(0.28F, 0.035F, 0.22F) * scale,
               obsidian,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.10F * scale, 0.0F),
               QVector3D(0.20F, 0.035F, 0.16F) * scale,
               bronze,
               states);
  desc.add_box(base + QVector3D(0.0F, 0.17F * scale, 0.0F),
               QVector3D(0.13F, 0.045F, 0.12F) * scale,
               obsidian,
               states);

  desc.add_cylinder(base + QVector3D(0.0F, 0.17F * scale, 0.0F),
                    base + QVector3D(0.0F, 0.62F * scale, 0.0F),
                    0.055F * scale,
                    obsidian,
                    states);
  desc.add_box(base + QVector3D(0.0F, 0.45F * scale, 0.0F),
               QVector3D(0.075F, 0.13F, 0.075F) * scale,
               ember,
               states);
  desc.add_rotated_box(base + QVector3D(0.0F, 0.69F * scale, 0.0F),
                       QVector3D(0.075F, 0.16F, 0.075F) * scale,
                       QVector3D(0.0F, 0.0F, 45.0F),
                       bronze,
                       states);

  for (float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(base + QVector3D(side * 0.16F * scale, 0.39F * scale, 0.0F),
                         QVector3D(0.18F, 0.045F, 0.065F) * scale,
                         QVector3D(0.0F, 0.0F, side * 43.0F),
                         bronze,
                         states);
    desc.add_rotated_box(base + QVector3D(side * 0.29F * scale, 0.53F * scale, 0.0F),
                         QVector3D(0.14F, 0.040F, 0.055F) * scale,
                         QVector3D(0.0F, 0.0F, side * 67.0F),
                         obsidian,
                         states);
    desc.add_box(base + QVector3D(side * 0.34F * scale, 0.65F * scale, 0.0F),
                 QVector3D(0.045F, 0.10F, 0.045F) * scale,
                 bronze,
                 states,
                 BuildingLODMask::Full);
  }
}

} // namespace Render::GL
