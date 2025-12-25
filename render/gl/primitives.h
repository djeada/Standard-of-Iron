#pragma once

#include "mesh.h"
#include <memory>

namespace Render::GL {

inline constexpr int k_default_radial_segments = 32;
inline constexpr int k_default_latitude_segments = 16;
inline constexpr int k_default_capsule_height_segments = 1;
inline constexpr int k_default_torso_height_segments = 8;

auto get_unit_cylinder(int radial_segments = k_default_radial_segments)
    -> Mesh *;
auto get_unit_cube() -> Mesh *;

auto get_unit_sphere(int lat_segments = k_default_latitude_segments,
                     int lon_segments = k_default_radial_segments) -> Mesh *;

auto get_unit_cone(int radial_segments = k_default_radial_segments) -> Mesh *;

auto get_unit_capsule(int radial_segments = k_default_radial_segments,
                      int height_segments = k_default_capsule_height_segments)
    -> Mesh *;

auto get_unit_torso(int radial_segments = k_default_radial_segments,
                    int height_segments = k_default_torso_height_segments)
    -> Mesh *;

} 
