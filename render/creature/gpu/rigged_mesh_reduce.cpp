#include "render/creature/gpu/rigged_mesh_reduce.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <meshoptimizer.h>
#include <utility>
#include <vector>

#include "render/geom/rigged_mesh.h"

namespace Render::GL {

namespace {

// Normals keep the shading of a simplified surface; texture coordinates keep
// the painted hair, face and pelt patterns in place.
constexpr std::size_t k_attribute_count = 5;
constexpr float k_normal_weight = 0.5F;
constexpr float k_tex_coord_weight = 1.0F;
constexpr float k_min_saving = 0.10F;

// Weights are compared in steps of 1/16: a vertex merged into a neighbour of
// the same class follows that neighbour's skinning, which differs by at most
// this much bone influence, under half a pixel at the sizes the copy is drawn.
constexpr float k_weight_steps = 16.0F;

// The skinning a vertex follows: its bones with their quantised weights. Bones
// whose weight rounds to nothing do not count.
auto skin_class(const RiggedVertex& vertex) noexcept -> std::uint64_t {
  std::uint64_t key = 0;
  for (std::size_t k = 0; k < vertex.bone_indices.size(); ++k) {
    const auto steps = static_cast<std::uint64_t>(
        std::lround(vertex.bone_weights[k] * k_weight_steps));
    const std::uint64_t bone = steps != 0U ? vertex.bone_indices[k] : 0U;
    key = (key << 16U) | (bone << 8U) | steps;
  }
  return key;
}

// Vertices simplification must not move or merge away: every vertex of a
// triangle whose corners follow different skinning or carry different colour
// roles. Everything else collapses only within one skinning class, so the copy
// animates like the full mesh.
auto locked_vertices(const std::vector<RiggedVertex>& vertices,
                     const std::vector<std::uint32_t>& indices,
                     std::size_t triangle_index_count) -> std::vector<unsigned char> {
  std::vector<unsigned char> locked(vertices.size(), 0U);
  std::vector<std::uint64_t> classes(vertices.size());
  for (std::size_t v = 0; v < vertices.size(); ++v) {
    classes[v] = skin_class(vertices[v]);
  }
  for (std::size_t i = 0; i < triangle_index_count; i += 3U) {
    const std::uint32_t ia = indices[i];
    const std::uint32_t ib = indices[i + 1U];
    const std::uint32_t ic = indices[i + 2U];
    const bool same_skin = classes[ia] == classes[ib] && classes[ia] == classes[ic];
    const auto& a = vertices[ia];
    const auto& b = vertices[ib];
    const auto& c = vertices[ic];
    const bool same_role = a.color_role == b.color_role && a.color_role == c.color_role;
    if (!same_skin || !same_role) {
      locked[indices[i]] = 1U;
      locked[indices[i + 1U]] = 1U;
      locked[indices[i + 2U]] = 1U;
    }
  }
  return locked;
}

} // namespace

auto reduce_rigged_mesh(const RiggedMesh& mesh,
                        float max_error) -> std::shared_ptr<RiggedMesh> {
  const auto& vertices = mesh.get_vertices();
  const auto& indices = mesh.get_indices();
  const std::size_t triangle_index_count = indices.size() - (indices.size() % 3U);
  if (vertices.empty() || triangle_index_count < 3U) {
    return nullptr;
  }
  for (std::size_t i = 0; i < triangle_index_count; ++i) {
    if (indices[i] >= vertices.size()) {
      return nullptr;
    }
  }

  const std::vector<unsigned char> locked =
      locked_vertices(vertices, indices, triangle_index_count);
  const std::array<float, k_attribute_count> attribute_weights{k_normal_weight,
                                                               k_normal_weight,
                                                               k_normal_weight,
                                                               k_tex_coord_weight,
                                                               k_tex_coord_weight};
  static_assert(offsetof(RiggedVertex, tex_coord) ==
                    offsetof(RiggedVertex, normal_bone_local) + 3U * sizeof(float),
                "normal and texture coordinates must be adjacent attributes");

  std::vector<std::uint32_t> reduced(triangle_index_count);
  float result_error = 0.0F;
  // The error bound decides where simplification stops; the index target only
  // keeps it from stopping early.
  const std::size_t reduced_count =
      meshopt_simplifyWithAttributes(reduced.data(),
                                     indices.data(),
                                     triangle_index_count,
                                     vertices.front().position_bone_local.data(),
                                     vertices.size(),
                                     sizeof(RiggedVertex),
                                     vertices.front().normal_bone_local.data(),
                                     sizeof(RiggedVertex),
                                     attribute_weights.data(),
                                     k_attribute_count,
                                     locked.data(),
                                     0U,
                                     max_error,
                                     meshopt_SimplifyLockBorder,
                                     &result_error);
  if (reduced_count == 0U ||
      static_cast<float>(reduced_count) >
          static_cast<float>(triangle_index_count) * (1.0F - k_min_saving)) {
    return nullptr;
  }
  reduced.resize(reduced_count);
  meshopt_optimizeVertexCache(
      reduced.data(), reduced.data(), reduced.size(), vertices.size());
  return std::make_shared<RiggedMesh>(vertices, std::move(reduced));
}

auto rigged_mesh_height(const RiggedMesh& mesh) -> float {
  float low = std::numeric_limits<float>::max();
  float high = std::numeric_limits<float>::lowest();
  for (const auto& vertex : mesh.get_vertices()) {
    low = std::min(low, vertex.position_bone_local[1]);
    high = std::max(high, vertex.position_bone_local[1]);
  }
  return high > low ? high - low : 0.0F;
}

} // namespace Render::GL
