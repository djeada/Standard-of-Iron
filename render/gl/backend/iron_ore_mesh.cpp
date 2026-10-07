#include "iron_ore_mesh.h"

#include <array>

#include "rock_outcrop_mesh.h"

namespace {

using Render::GL::BackendPipelines::RockMass;
using Render::GL::BackendPipelines::RockShard;

constexpr std::array<RockMass, 9> k_rock{{
    {{-0.20F, -0.06F, 0.00F}, 0.42F, 0.30F, 0.26F, -0.03F, 0.02F, 7},
    {{0.22F, -0.06F, -0.04F}, 0.36F, 0.27F, 0.23F, 0.03F, -0.02F, 19},
    {{-0.30F, -0.04F, 0.02F}, 0.26F, 0.20F, 0.30F, -0.05F, 0.03F, 31},
    {{0.06F, -0.04F, -0.06F}, 0.24F, 0.19F, 0.29F, 0.02F, -0.03F, 43},
    {{0.34F, -0.04F, 0.06F}, 0.20F, 0.16F, 0.29F, 0.04F, 0.02F, 57},
    {{-0.58F, -0.08F, 0.16F}, 0.15F, 0.12F, 0.14F, 0.02F, 0.01F, 71},
    {{0.55F, -0.08F, -0.14F}, 0.14F, 0.11F, 0.13F, -0.01F, 0.02F, 83},
    {{-0.10F, -0.07F, 0.34F}, 0.13F, 0.12F, 0.11F, 0.01F, 0.02F, 97},
    {{0.24F, -0.07F, -0.32F}, 0.12F, 0.11F, 0.10F, -0.01F, -0.02F, 109},
}};

constexpr std::array<RockShard, 11> k_blades{{
    {{-0.24F, 0.16F, 0.02F}, {-0.36F, 0.75F, 0.10F}, 0.120F, 5, 5},
    {{0.04F, 0.16F, -0.04F}, {0.14F, 0.70F, -0.18F}, 0.105F, 5, 23},
    {{0.30F, 0.13F, 0.05F}, {0.52F, 0.58F, 0.18F}, 0.090F, 5, 37},
    {{-0.12F, 0.11F, 0.14F}, {-0.24F, 0.54F, 0.36F}, 0.075F, 5, 51},
    {{0.18F, 0.10F, -0.16F}, {0.30F, 0.48F, -0.36F}, 0.068F, 5, 65},
    {{-0.44F, 0.08F, -0.06F}, {-0.66F, 0.40F, -0.16F}, 0.058F, 5, 79},
    {{0.00F, 0.14F, 0.10F}, {0.06F, 0.62F, 0.26F}, 0.070F, 4, 91},
    {{-0.30F, 0.10F, -0.14F}, {-0.46F, 0.50F, -0.36F}, 0.062F, 4, 103},
    {{0.44F, 0.08F, -0.08F}, {0.70F, 0.34F, -0.16F}, 0.050F, 4, 117},
    {{-0.06F, 0.12F, -0.20F}, {-0.10F, 0.58F, -0.40F}, 0.060F, 4, 131},
    {{0.16F, 0.10F, 0.20F}, {0.34F, 0.42F, 0.42F}, 0.048F, 4, 149},
}};

} // namespace

namespace Render::GL::BackendPipelines {

auto build_iron_ore_mesh() -> PropMeshData {
  PropMeshData mesh;
  mesh.vertices.reserve(6144);
  mesh.indices.reserve(6144);
  for (const auto& rock : k_rock) {
    append_rock_mass(mesh, rock);
  }
  for (const auto& blade : k_blades) {
    append_rock_shard(mesh, blade);
  }
  return mesh;
}

} // namespace Render::GL::BackendPipelines
