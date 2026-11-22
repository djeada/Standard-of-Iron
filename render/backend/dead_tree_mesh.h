#pragma once

#include <QVector3D>

#include <cstdint>
#include <utility>
#include <vector>

namespace Render::GL::BackendPipelines {

using PropMeshVertex = std::pair<QVector3D, QVector3D>;

struct PropMeshData {
  std::vector<PropMeshVertex> vertices;
  std::vector<uint16_t> indices;
};

[[nodiscard]] auto build_dead_tree_mesh() -> PropMeshData;

} // namespace Render::GL::BackendPipelines
