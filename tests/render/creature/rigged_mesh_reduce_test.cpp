#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <set>
#include <utility>
#include <vector>

#include "render/creature/gpu/rigged_mesh_reduce.h"
#include "render/creature/pipeline/creature_submission_context.h"
#include "render/geom/rigged_mesh.h"

namespace {

using Render::GL::RiggedMesh;
using Render::GL::RiggedVertex;

constexpr int k_grid = 40;

auto rigid_vertex(float x,
                  float y,
                  std::uint8_t bone,
                  std::uint8_t role) -> RiggedVertex {
  RiggedVertex vertex;

  vertex.position_bone_local = {x, y, 0.05F * std::sin(x * 3.0F)};
  vertex.normal_bone_local = {0.0F, 0.0F, 1.0F};
  vertex.tex_coord = {x, y};
  vertex.bone_indices = {bone, 0, 0, 0};
  vertex.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
  vertex.color_role = role;
  return vertex;
}

auto two_part_sheet() -> std::shared_ptr<RiggedMesh> {
  std::vector<RiggedVertex> vertices;
  for (int y = 0; y <= k_grid; ++y) {
    for (int x = 0; x <= k_grid; ++x) {
      const bool left = x <= k_grid / 2;
      vertices.push_back(rigid_vertex(static_cast<float>(x) / k_grid,
                                      static_cast<float>(y) / k_grid,
                                      left ? 1U : 2U,
                                      left ? 1U : 2U));
    }
  }
  std::vector<std::uint32_t> indices;
  const auto at = [](int x, int y) {
    return static_cast<std::uint32_t>((y * (k_grid + 1)) + x);
  };
  for (int y = 0; y < k_grid; ++y) {
    for (int x = 0; x < k_grid; ++x) {
      indices.insert(indices.end(), {at(x, y), at(x + 1, y), at(x, y + 1)});
      indices.insert(indices.end(), {at(x + 1, y), at(x + 1, y + 1), at(x, y + 1)});
    }
  }
  return std::make_shared<RiggedMesh>(std::move(vertices), std::move(indices));
}

} // namespace

TEST(RiggedMeshReduce, KeepsTheVerticesAndDropsTriangles) {
  const auto full = two_part_sheet();
  const auto reduced =
      Render::GL::reduce_rigged_mesh(*full, Render::GL::k_reduced_geometry_max_error);
  ASSERT_NE(reduced, nullptr);
  EXPECT_LT(reduced->index_count(), full->index_count() / 2U);
  EXPECT_EQ(reduced->index_count() % 3U, 0U);

  ASSERT_EQ(reduced->vertex_count(), full->vertex_count());
  for (const auto index : reduced->get_indices()) {
    EXPECT_LT(index, full->vertex_count());
  }
}

TEST(RiggedMeshReduce, NeverMergesAcrossBonesOrColourRoles) {
  const auto full = two_part_sheet();
  const auto reduced =
      Render::GL::reduce_rigged_mesh(*full, Render::GL::k_reduced_geometry_max_error);
  ASSERT_NE(reduced, nullptr);
  const auto& vertices = reduced->get_vertices();
  const auto& indices = reduced->get_indices();

  std::set<std::vector<std::uint32_t>> full_triangles;
  const auto& full_indices = full->get_indices();
  for (std::size_t i = 0; i < full_indices.size(); i += 3U) {
    full_triangles.insert(
        {full_indices[i], full_indices[i + 1U], full_indices[i + 2U]});
  }
  for (std::size_t i = 0; i < indices.size(); i += 3U) {
    const auto& a = vertices[indices[i]];
    const auto& b = vertices[indices[i + 1U]];
    const auto& c = vertices[indices[i + 2U]];
    const bool mixed = a.color_role != b.color_role || a.color_role != c.color_role ||
                       a.bone_indices[0] != b.bone_indices[0] ||
                       a.bone_indices[0] != c.bone_indices[0];
    if (mixed) {
      EXPECT_TRUE(
          full_triangles.contains({indices[i], indices[i + 1U], indices[i + 2U]}))
          << "triangle " << i / 3U << " crosses the seam but was invented";
    }
  }
}

TEST(RiggedMeshReduce, LeavesMeshesThatCannotShrinkAlone) {
  std::vector<RiggedVertex> vertices{rigid_vertex(0.0F, 0.0F, 1U, 1U),
                                     rigid_vertex(1.0F, 0.0F, 1U, 1U),
                                     rigid_vertex(0.0F, 1.0F, 1U, 1U)};
  const RiggedMesh single(std::move(vertices), {0U, 1U, 2U});
  EXPECT_EQ(Render::GL::reduce_rigged_mesh(single, 0.006F), nullptr);
}

TEST(RiggedMeshReduce, HeightIsTheUpExtent) {
  const auto full = two_part_sheet();
  EXPECT_FLOAT_EQ(Render::GL::rigged_mesh_height(*full), 1.0F);
}

TEST(ReducedGeometryRule, SwitchesAtTheOnScreenHeight) {
  using Render::Creature::Pipeline::k_reduced_geometry_max_height_px;
  using Render::Creature::Pipeline::ReducedGeometryRule;
  constexpr float k_focal_px = 1000.0F;
  const ReducedGeometryRule rule(QVector3D(0.0F, 0.0F, 0.0F), k_focal_px);

  QMatrix4x4 world;
  world.scale(0.5F);
  const float switch_distance = k_focal_px / k_reduced_geometry_max_height_px;
  QMatrix4x4 near_world;
  near_world.translate(0.0F, 0.0F, switch_distance * 0.9F);
  near_world *= world;
  QMatrix4x4 far_world;
  far_world.translate(0.0F, 0.0F, switch_distance * 1.1F);
  far_world *= world;
  EXPECT_FALSE(rule.small_on_screen(near_world, 2.0F));
  EXPECT_TRUE(rule.small_on_screen(far_world, 2.0F));
  EXPECT_FALSE(ReducedGeometryRule{}.small_on_screen(far_world, 2.0F));
}
