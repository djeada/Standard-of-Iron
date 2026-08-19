#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "part_graph.h"

namespace Render::GL {
class Mesh;
}

namespace Render::Creature {

[[nodiscard]] auto primitive_needs_tail(PrimitiveShape shape) noexcept -> bool;

[[nodiscard]] auto
bone_world_offset(const QMatrix4x4& bone,
                  const QVector3D& local_offset) noexcept -> QVector3D;

inline constexpr int k_minimal_radial_segments = 8;
inline constexpr int k_minimal_latitude_segments = 5;

[[nodiscard]] auto
primitive_unit_mesh(const PrimitiveInstance& prim,
                    CreatureLOD lod = CreatureLOD::Full) noexcept -> Render::GL::Mesh*;

[[nodiscard]] auto primitive_unit_model(const PrimitiveInstance& prim,
                                        const QMatrix4x4& anchor_bone,
                                        const QMatrix4x4& tail_bone,
                                        QMatrix4x4& out_model) noexcept -> bool;

} // namespace Render::Creature
