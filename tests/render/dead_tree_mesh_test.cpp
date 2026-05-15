#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

#include "render/gl/backend/dead_tree_mesh.h"

namespace {

using Render::GL::BackendPipelines::build_dead_tree_mesh;

TEST(DeadTreeMeshTest, BuildsFlatCutFacesAtBothEnds) {
  const auto mesh = build_dead_tree_mesh();

  ASSERT_FALSE(mesh.vertices.empty());
  ASSERT_FALSE(mesh.indices.empty());

  int neg_cut_vertices = 0;
  int pos_cut_vertices = 0;

  for (const auto& [pos, normal] : mesh.vertices) {
    if (std::abs(pos.x() + 1.12F) < 0.001F && std::abs(normal.x() + 1.0F) < 0.001F) {
      ++neg_cut_vertices;
    }
    if (std::abs(pos.x() - 1.12F) < 0.001F && std::abs(normal.x() - 1.0F) < 0.001F) {
      ++pos_cut_vertices;
    }
  }

  EXPECT_GE(neg_cut_vertices, 12);
  EXPECT_GE(pos_cut_vertices, 12);
}

TEST(DeadTreeMeshTest, KeepsMainTrunkCloseToRoundThroughTheMiddle) {
  const auto mesh = build_dead_tree_mesh();

  float body_y_min = std::numeric_limits<float>::max();
  float body_y_max = std::numeric_limits<float>::lowest();
  float body_z_min = std::numeric_limits<float>::max();
  float body_z_max = std::numeric_limits<float>::lowest();
  float body_z_sum = 0.0F;
  int body_vertex_count = 0;

  for (const auto& [pos, normal] : mesh.vertices) {
    if (std::abs(pos.x()) > 0.90F || pos.y() < -0.10F || pos.y() > 0.40F ||
        std::abs(pos.z()) > 0.26F || std::abs(normal.x()) > 0.60F) {
      continue;
    }
    body_y_min = std::min(body_y_min, pos.y());
    body_y_max = std::max(body_y_max, pos.y());
    body_z_min = std::min(body_z_min, pos.z());
    body_z_max = std::max(body_z_max, pos.z());
    body_z_sum += pos.z();
    ++body_vertex_count;
  }

  ASSERT_GE(body_vertex_count, 40);

  const float body_y_span = body_y_max - body_y_min;
  const float body_z_span = body_z_max - body_z_min;
  const float body_aspect = body_y_span / body_z_span;

  EXPECT_GT(body_y_span, 0.34F);
  EXPECT_GT(body_z_span, 0.30F);
  EXPECT_GT(body_aspect, 0.75F);
  EXPECT_LT(body_aspect, 1.15F);
  EXPECT_NEAR(body_z_sum / static_cast<float>(body_vertex_count), 0.0F, 0.03F);
}

} // namespace
