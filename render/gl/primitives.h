#pragma once

#include "mesh.h"
#include <memory>

namespace Render::GL {

inline constexpr int kDefaultRadialSegments = 32;
inline constexpr int kDefaultLatitudeSegments = 16;
inline constexpr int kDefaultCapsuleHeightSegments = 1;
inline constexpr int kDefaultTorsoHeightSegments = 8;

auto get_unit_cylinder(int radial_segments = kDefaultRadialSegments) -> Mesh *;
auto get_unit_cube() -> Mesh *;

auto get_unit_sphere(int lat_segments = kDefaultLatitudeSegments,
                     int lon_segments = kDefaultRadialSegments) -> Mesh *;

auto get_unit_cone(int radial_segments = kDefaultRadialSegments) -> Mesh *;

auto get_unit_capsule(int radial_segments = kDefaultRadialSegments,
                      int height_segments = kDefaultCapsuleHeightSegments)
    -> Mesh *;

auto get_unit_torso(int radial_segments = kDefaultRadialSegments,
                    int height_segments = kDefaultTorsoHeightSegments) -> Mesh *;

} // namespace Render::GL
