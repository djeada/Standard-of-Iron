#pragma once

#include "mesh.h"
#include <memory>

namespace Render::GL {

inline constexpr int kDefaultRadialSegments = 32;
inline constexpr int kDefaultLatitudeSegments = 16;
inline constexpr int kDefaultCapsuleHeightSegments = 1;
inline constexpr int kDefaultTorsoHeightSegments = 8;

auto get_unit_cylinder(int radialSegments = kDefaultRadialSegments) -> Mesh *;
auto get_unit_cube() -> Mesh *;

auto get_unit_sphere(int latSegments = kDefaultLatitudeSegments,
                     int lonSegments = kDefaultRadialSegments) -> Mesh *;

auto get_unit_cone(int radialSegments = kDefaultRadialSegments) -> Mesh *;

auto get_unit_capsule(int radialSegments = kDefaultRadialSegments,
                      int heightSegments = kDefaultCapsuleHeightSegments)
    -> Mesh *;

auto get_unit_torso(int radialSegments = kDefaultRadialSegments,
                    int heightSegments = kDefaultTorsoHeightSegments) -> Mesh *;

} // namespace Render::GL
