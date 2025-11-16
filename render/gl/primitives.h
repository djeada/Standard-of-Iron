#pragma once

#include "mesh.h"
#include <memory>

namespace Render::GL {

inline constexpr int kDefaultRadialSegments = 32;
inline constexpr int kDefaultLatitudeSegments = 16;
inline constexpr int kDefaultCapsuleHeightSegments = 1;
inline constexpr int kDefaultTorsoHeightSegments = 8;

auto getUnitCylinder(int radialSegments = kDefaultRadialSegments) -> Mesh *;
auto getUnitCube() -> Mesh *;

auto getUnitSphere(int latSegments = kDefaultLatitudeSegments,
                   int lonSegments = kDefaultRadialSegments) -> Mesh *;

auto getUnitCone(int radialSegments = kDefaultRadialSegments) -> Mesh *;

auto getUnitCapsule(int radialSegments = kDefaultRadialSegments,
                    int heightSegments = kDefaultCapsuleHeightSegments)
    -> Mesh *;

auto getUnitTorso(int radialSegments = kDefaultRadialSegments,
                  int heightSegments = kDefaultTorsoHeightSegments) -> Mesh *;

} // namespace Render::GL
