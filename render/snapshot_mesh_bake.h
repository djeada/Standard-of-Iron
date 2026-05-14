#pragma once

#include <QMatrix4x4>

#include <span>
#include <vector>

#include "rigged_mesh.h"

namespace Render::GL {

[[nodiscard]] auto bake_snapshot_vertices(std::span<const RiggedVertex> source_vertices,
                                          std::span<const QMatrix4x4> frame_palette)
    -> std::vector<RiggedVertex>;

}
