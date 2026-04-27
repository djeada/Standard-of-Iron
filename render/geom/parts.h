#pragma once

#include "../math/bone_frame.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::Geom {

[[nodiscard]] auto oriented_cylinder(const QVector3D &a, const QVector3D &b,
                                     const QVector3D &right_reference,
                                     float radius_right,
                                     float radius_forward) -> QMatrix4x4;

[[nodiscard]] auto oriented_cylinder(const QMatrix4x4 &parent,
                                     const QVector3D &a, const QVector3D &b,
                                     const QVector3D &right_reference,
                                     float radius_right,
                                     float radius_forward) -> QMatrix4x4;

[[nodiscard]] auto oriented_box(const Render::Math::BoneFrame &frame,
                                const QVector3D &half_extents) -> QMatrix4x4;

[[nodiscard]] auto oriented_box(const QMatrix4x4 &parent,
                                const Render::Math::BoneFrame &frame,
                                const QVector3D &half_extents) -> QMatrix4x4;

[[nodiscard]] auto oriented_sphere(const Render::Math::BoneFrame &frame,
                                   const QVector3D &radii) -> QMatrix4x4;

[[nodiscard]] auto oriented_sphere(const QMatrix4x4 &parent,
                                   const Render::Math::BoneFrame &frame,
                                   const QVector3D &radii) -> QMatrix4x4;

} // namespace Render::Geom
