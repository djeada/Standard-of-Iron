#pragma once

#include "render/gl/backend/dead_tree_mesh.h"

namespace Render::GL::BackendPipelines {

struct RockMass {
  QVector3D base;
  float radius_x{0.0F};
  float radius_z{0.0F};
  float height{0.0F};
  float lean_x{0.0F};
  float lean_z{0.0F};
  int seed{0};
};

struct RockShard {
  QVector3D root;
  QVector3D tip;
  float radius{0.0F};
  int sides{6};
  int seed{0};
};

void append_rock_mass(PropMeshData& out, const RockMass& rock);
void append_rock_shard(PropMeshData& out, const RockShard& shard);

} // namespace Render::GL::BackendPipelines
