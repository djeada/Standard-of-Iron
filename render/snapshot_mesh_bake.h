#pragma once

#include "rigged_mesh.h"

#include <QMatrix4x4>

#include <span>
#include <vector>

namespace Render::GL {

[[nodiscard]] auto bake_snapshot_vertices(
    std::span<const RiggedVertex> source_vertices,
    std::span<const QMatrix4x4> frame_palette) -> std::vector<RiggedVertex>;

}
