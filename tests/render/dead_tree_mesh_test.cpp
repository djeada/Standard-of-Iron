#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

#include "render/gl/backend/dead_tree_mesh.h"

namespace {

using Render::GL::BackendPipelines::build_dead_tree_mesh;

TEST(DeadTreeMeshTest, SawsTheButtEndFlatAndSplintersTheTip) {
  const auto mesh = build_dead_tree_mesh();

  ASSERT_FALSE(mesh.vertices.empty());
  ASSERT_FALSE(mesh.indices.empty());

  int butt_cut_vertices = 0;
  int tip_shard_vertices = 0;
  int axis_aligned_tip_vertices = 0;

  for (const auto& [pos, normal] : mesh.vertices) {
    if (std::abs(pos.x() + 1.12F) < 0.001F && std::abs(normal.x() + 1.0F) < 0.001F) {
      ++butt_cut_vertices;
    }
    if (pos.x() > 1.13F) {
      ++tip_shard_vertices;
      if (std::abs(normal.x() - 1.0F) < 0.001F) {
        ++axis_aligned_tip_vertices;
      }
    }
  }

  EXPECT_GE(butt_cut_vertices, 12);
  EXPECT_GE(tip_shard_vertices, 12);
  EXPECT_EQ(axis_aligned_tip_vertices, 0);
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
  EXPECT_NEAR(body_z_sum / static_cast<float>(body_vertex_count), 0.0F, 0.06F);
}

TEST(DeadTreeMeshTest, TapersMonotonicallyFromButtToTip) {
  const auto mesh = build_dead_tree_mesh();

  const auto radius_near = [&](float x) {
    float max_offset = 0.0F;
    for (const auto& [pos, normal] : mesh.vertices) {
      if (std::abs(pos.x() - x) > 0.02F || std::abs(normal.x()) > 0.60F) {
        continue;
      }
      const float dy = pos.y() - 0.150F;
      const float dz = pos.z();
      max_offset = std::max(max_offset, std::sqrt(dy * dy + dz * dz));
    }
    return max_offset;
  };

  const float butt = radius_near(-0.75F);
  const float middle = radius_near(0.00F);
  const float tip = radius_near(0.75F);

  ASSERT_GT(butt, 0.0F);
  ASSERT_GT(middle, 0.0F);
  ASSERT_GT(tip, 0.0F);

  EXPECT_GT(butt, middle);
  EXPECT_GT(middle, tip);
}

TEST(DeadTreeMeshTest, StaysAFallenLogWithNoUprightTrunk) {
  const auto mesh = build_dead_tree_mesh();

  float x_min = std::numeric_limits<float>::max();
  float x_max = std::numeric_limits<float>::lowest();
  float y_max = std::numeric_limits<float>::lowest();

  for (const auto& [pos, normal] : mesh.vertices) {
    x_min = std::min(x_min, pos.x());
    x_max = std::max(x_max, pos.x());
    y_max = std::max(y_max, pos.y());
  }

  EXPECT_LT(y_max, 0.60F);
  EXPECT_GT(x_max - x_min, 2.0F);
  EXPECT_GT(x_max - x_min, (y_max + 0.10F) * 3.0F);
}

TEST(DeadTreeMeshTest, BuildsBranchStubsAsSolidPrisms) {
  const auto mesh = build_dead_tree_mesh();

  ASSERT_EQ(mesh.indices.size() % 3, 0U);

  for (std::size_t tri = 0; tri < mesh.indices.size(); tri += 3) {
    const auto& p0 = mesh.vertices[mesh.indices[tri]].first;
    const auto& p1 = mesh.vertices[mesh.indices[tri + 1]].first;
    const auto& p2 = mesh.vertices[mesh.indices[tri + 2]].first;
    const float area =
        QVector3D::crossProduct(p1 - p0, p2 - p0).length() * 0.5F;
    EXPECT_GT(area, 1.0e-6F) << "degenerate triangle at index " << tri;
  }
}

} // namespace
