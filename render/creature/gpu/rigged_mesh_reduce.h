#pragma once

#include <memory>

namespace Render::GL {

class RiggedMesh;

// How far a reduced mesh may drift from the full one, as a fraction of the
// full mesh's extent. Drawn only while the creature is at most
// k_reduced_geometry_max_height_px tall on screen, the drift stays well under
// half a pixel.
inline constexpr float k_reduced_geometry_max_error = 0.006F;

// Builds the small-on-screen copy of a full creature mesh: the same vertices
// (every cloak, strap and beard kept) with fewer triangles, simplified so its
// shape stays within `max_error` of the full mesh. Simplification merges
// vertices only within one skinning class and one colour role, so the copy
// animates and colours like the full mesh.
//
// Returns null when simplifying would remove less than a tenth of the
// triangles; the full mesh is drawn then.
[[nodiscard]] auto reduce_rigged_mesh(const RiggedMesh& mesh,
                                      float max_error) -> std::shared_ptr<RiggedMesh>;

// Height of the mesh's bounding box along its up axis, in mesh units.
[[nodiscard]] auto rigged_mesh_height(const RiggedMesh& mesh) -> float;

} // namespace Render::GL
