#pragma once

#include <memory>

namespace Render::GL {

class RiggedMesh;

inline constexpr float k_reduced_geometry_max_error = 0.006F;

[[nodiscard]] auto reduce_rigged_mesh(const RiggedMesh& mesh,
                                      float max_error) -> std::shared_ptr<RiggedMesh>;

[[nodiscard]] auto rigged_mesh_height(const RiggedMesh& mesh) -> float;

} // namespace Render::GL
