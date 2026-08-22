#pragma once

#include <cstdint>

namespace Render::GL {
class Mesh;
}

namespace Render::Humanoid {

enum class HumanoidMeshPart : std::uint8_t {

  TorsoNoBottomCap,
};

void build_humanoid_derived_meshes();

[[nodiscard]] auto humanoid_mesh_part(HumanoidMeshPart part) -> Render::GL::Mesh*;

} // namespace Render::Humanoid
