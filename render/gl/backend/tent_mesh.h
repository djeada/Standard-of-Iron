#pragma once

#include "render/gl/backend/dead_tree_mesh.h"

namespace Render::GL::BackendPipelines {

[[nodiscard]] auto build_tent_mesh() -> PropMeshData;

} // namespace Render::GL::BackendPipelines
