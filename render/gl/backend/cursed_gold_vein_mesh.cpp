#include "cursed_gold_vein_mesh.h"

#include <array>

#include "rock_outcrop_mesh.h"

namespace {

using Render::GL::BackendPipelines::RockMass;
using Render::GL::BackendPipelines::RockShard;

constexpr std::array<RockMass, 14> k_rock{{
    {{-0.05F, -0.06F, 0.02F}, 0.62F, 0.55F, 0.19F, 0.03F, -0.02F, 11},
    {{-0.24F, -0.05F, 0.20F}, 0.46F, 0.42F, 0.16F, -0.03F, 0.02F, 23},
    {{0.26F, -0.05F, -0.22F}, 0.43F, 0.40F, 0.15F, 0.02F, -0.03F, 37},
    {{-0.27F, -0.03F, 0.05F}, 0.36F, 0.30F, 0.40F, -0.06F, 0.03F, 5},
    {{0.31F, -0.03F, -0.09F}, 0.33F, 0.28F, 0.36F, 0.05F, -0.04F, 61},
    {{0.13F, -0.03F, 0.34F}, 0.25F, 0.22F, 0.29F, 0.02F, 0.04F, 73},
    {{-0.08F, -0.03F, -0.42F}, 0.22F, 0.20F, 0.25F, -0.02F, -0.03F, 89},
    {{0.02F, -0.03F, 0.03F}, 0.27F, 0.24F, 0.33F, 0.01F, 0.02F, 179},
    {{-0.60F, -0.07F, -0.27F}, 0.16F, 0.13F, 0.15F, 0.02F, 0.01F, 101},
    {{0.51F, -0.07F, 0.48F}, 0.14F, 0.16F, 0.13F, -0.01F, 0.02F, 113},
    {{-0.18F, -0.06F, 0.63F}, 0.12F, 0.11F, 0.11F, 0.01F, 0.02F, 127},
    {{0.58F, -0.07F, -0.35F}, 0.13F, 0.11F, 0.12F, 0.02F, -0.01F, 139},
    {{-0.48F, -0.06F, 0.45F}, 0.11F, 0.10F, 0.10F, -0.01F, 0.01F, 151},
    {{0.05F, -0.06F, -0.64F}, 0.10F, 0.12F, 0.09F, 0.01F, -0.02F, 163},
}};

constexpr std::array<RockShard, 9> k_shards{{
    {{0.00F, 0.20F, 0.02F}, {0.11F, 1.26F, 0.15F}, 0.175F, 7, 3},
    {{-0.10F, 0.18F, -0.05F}, {-0.42F, 0.99F, -0.32F}, 0.135F, 6, 17},
    {{0.15F, 0.18F, 0.13F}, {0.53F, 0.89F, 0.52F}, 0.125F, 6, 29},
    {{0.06F, 0.16F, -0.18F}, {0.19F, 0.82F, -0.58F}, 0.100F, 5, 41},
    {{-0.03F, 0.20F, 0.19F}, {-0.13F, 0.84F, 0.39F}, 0.084F, 5, 53},
    {{-0.29F, 0.14F, 0.09F}, {-0.48F, 0.78F, 0.21F}, 0.076F, 5, 67},
    {{0.32F, 0.12F, -0.11F}, {0.51F, 0.76F, -0.24F}, 0.070F, 5, 79},
    {{0.13F, 0.14F, 0.40F}, {0.22F, 0.72F, 0.59F}, 0.056F, 5, 97},
    {{0.22F, 0.15F, -0.30F}, {0.34F, 0.70F, -0.44F}, 0.050F, 5, 109},
}};

} // namespace

namespace Render::GL::BackendPipelines {

auto build_cursed_gold_vein_mesh() -> PropMeshData {
  PropMeshData mesh;
  mesh.vertices.reserve(8192);
  mesh.indices.reserve(8192);
  for (const auto& rock : k_rock) {
    append_rock_mass(mesh, rock);
  }
  for (const auto& shard : k_shards) {
    append_rock_shard(mesh, shard);
  }
  return mesh;
}

} // namespace Render::GL::BackendPipelines
