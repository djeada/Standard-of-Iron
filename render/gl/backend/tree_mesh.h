#pragma once

#include "ring_loft_builder.h"

namespace Render::GL::BackendPipelines {

struct TreeMesh {
  std::vector<RingVertex> vertices;
  std::vector<std::uint16_t> indices;
};

auto build_olive_tree_mesh() -> TreeMesh;

} // namespace Render::GL::BackendPipelines
