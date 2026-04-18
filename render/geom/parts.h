#pragma once

#include "../math/bone_frame.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::Geom {

// Typed part-builders that replace `cylinder_between` for any part whose
// radial cross-section is NOT rotationally symmetric. The frame is supplied
// explicitly so the caller owns which axis is "width" vs "depth"; there is
// no silent perpendicular-picking.
//
// Unit-mesh convention (matches the existing primitives):
//   * The mesh is centred at the origin, axis-aligned.
//   * Cylinders extend along local Y from -0.5 to +0.5, with radius 1 in X and
//     Z (so scaling X and Z independently gives elliptical cross-section).
//   * Boxes are unit-size centred, each local axis running from -0.5 to +0.5.
//   * Spheres are unit-radius centred at origin.
// Callers supply the primitive to draw; these helpers only produce the model
// matrix that orients + sizes it.

// Build a model matrix that places a unit cylinder so that its local +Y maps
// to the segment `a → b`, its local +X maps to `frame.right`, and its local
// +Z maps to `frame.forward`. The frame's `origin` is ignored — the segment
// endpoints provide the positioning. Only `right`, `up` (derived from a→b),
// and `forward` are consumed.
//
// `radius_right` scales the local +X (the cylinder's "shoulder" axis in the
// unit's own right-hand direction). `radius_forward` scales local +Z (the
// cylinder's depth, unit's forward direction). For a circular cylinder, pass
// the same value for both.
//
// If the segment is degenerate (|b-a| < epsilon) the cylinder is placed at
// the midpoint, oriented by `frame.right` and `frame.forward`, with its
// local Y preserved from `frame` (callers can still scale it).
[[nodiscard]] auto oriented_cylinder(const QVector3D &a, const QVector3D &b,
                                     const QVector3D &right_reference,
                                     float radius_right,
                                     float radius_forward) -> QMatrix4x4;

// Same as above but concatenates with a parent (typically the unit's world
// matrix). Equivalent to `parent * oriented_cylinder(a,b,…)` but uses the
// fast affine multiply.
[[nodiscard]] auto oriented_cylinder(const QMatrix4x4 &parent,
                                     const QVector3D &a, const QVector3D &b,
                                     const QVector3D &right_reference,
                                     float radius_right,
                                     float radius_forward) -> QMatrix4x4;

// Oriented unit box centred at `frame.origin`. Half-extents are given in the
// frame's own axes: `half_extents.x()` along right, `.y()` along up,
// `.z()` along forward.
[[nodiscard]] auto oriented_box(const Render::Math::BoneFrame &frame,
                                const QVector3D &half_extents) -> QMatrix4x4;

[[nodiscard]] auto oriented_box(const QMatrix4x4 &parent,
                                const Render::Math::BoneFrame &frame,
                                const QVector3D &half_extents) -> QMatrix4x4;

// Oriented ellipsoid (unit sphere under per-axis scale). Radii are in the
// frame's axes: `radii.x()` along right, `.y()` along up, `.z()` along
// forward.
[[nodiscard]] auto oriented_sphere(const Render::Math::BoneFrame &frame,
                                   const QVector3D &radii) -> QMatrix4x4;

[[nodiscard]] auto oriented_sphere(const QMatrix4x4 &parent,
                                   const Render::Math::BoneFrame &frame,
                                   const QVector3D &radii) -> QMatrix4x4;

} // namespace Render::Geom
