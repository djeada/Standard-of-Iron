// Stage 15.5a — rigged mesh bake tests.
//
// Exercises the CPU-only bake pipeline (`bake_rigged_mesh_cpu`) on a
// minimal two-bone skeleton with one sphere (single-bone) and one
// cylinder (two-bone blend). No GL context is required; the shared
// unit meshes expose their CPU vertex/index arrays, so the bake runs
// entirely in host memory.

#include "render/creature/part_graph.h"
#include "render/creature/skeleton.h"
#include "render/gl/primitives.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_bake.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>

namespace {

using namespace Render::Creature;
using Render::GL::RiggedVertex;

constexpr float kEps = 1e-5F;

struct ToyGraph {
  static constexpr BoneIndex kBoneA = 0;
  static constexpr BoneIndex kBoneB = 1;

  std::array<BoneDef, 2> bones{
      BoneDef{"A", kInvalidBone},
      BoneDef{"B", kBoneA},
  };
  SkeletonTopology topology{std::span<const BoneDef>{bones}, {}};

  std::array<PrimitiveInstance, 2> prims{};
  PartGraph graph{};

  std::array<BoneWorldMatrix, 2> bind_pose{QMatrix4x4{}, QMatrix4x4{}};

  ToyGraph() {
    prims[0].debug_name = "sphere_on_A";
    prims[0].shape = PrimitiveShape::Sphere;
    prims[0].params.anchor_bone = kBoneA;
    prims[0].params.radius = 1.0F;
    prims[0].color_role = 3;

    prims[1].debug_name = "cylinder_A_to_B";
    prims[1].shape = PrimitiveShape::Cylinder;
    prims[1].params.anchor_bone = kBoneA;
    prims[1].params.tail_bone = kBoneB;
    prims[1].params.head_offset = QVector3D{0.0F, 0.0F, 0.0F};
    prims[1].params.tail_offset = QVector3D{0.0F, 2.0F, 0.0F};
    prims[1].params.radius = 0.5F;
    prims[1].color_role = 5;

    graph.primitives = std::span<const PrimitiveInstance>{prims};
  }
};

TEST(RiggedMeshBake, EmptyGraphProducesEmptyOutput) {
  BakeInput input{};
  auto baked = bake_rigged_mesh_cpu(input);
  EXPECT_TRUE(baked.vertices.empty());
  EXPECT_TRUE(baked.indices.empty());
}

TEST(RiggedMeshBake, TwoPrimitiveGraphAccumulatesVertexAndIndexCounts) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *sphere = Render::GL::get_unit_sphere();
  auto *cylinder = Render::GL::get_unit_cylinder();
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(cylinder, nullptr);

  EXPECT_EQ(baked.vertices.size(),
            sphere->get_vertices().size() + cylinder->get_vertices().size());
  EXPECT_EQ(baked.indices.size(),
            sphere->get_indices().size() + cylinder->get_indices().size());
}

TEST(RiggedMeshBake, SphereVerticesAreSingleBoneAnchor) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *sphere = Render::GL::get_unit_sphere();
  ASSERT_NE(sphere, nullptr);
  std::size_t const sphere_n = sphere->get_vertices().size();
  ASSERT_LE(sphere_n, baked.vertices.size());

  for (std::size_t i = 0; i < sphere_n; ++i) {
    RiggedVertex const &v = baked.vertices[i];
    EXPECT_EQ(v.bone_indices[0], ToyGraph::kBoneA) << "vertex " << i;
    EXPECT_FLOAT_EQ(v.bone_weights[0], 1.0F) << "vertex " << i;
    EXPECT_FLOAT_EQ(v.bone_weights[1], 0.0F) << "vertex " << i;
    EXPECT_FLOAT_EQ(v.bone_weights[2], 0.0F) << "vertex " << i;
    EXPECT_FLOAT_EQ(v.bone_weights[3], 0.0F) << "vertex " << i;
    EXPECT_EQ(v.color_role, 3U) << "vertex " << i;
  }
}

TEST(RiggedMeshBake, CylinderVerticesBlendAlongAxis) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *sphere = Render::GL::get_unit_sphere();
  auto *cylinder = Render::GL::get_unit_cylinder();
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(cylinder, nullptr);

  std::size_t const sphere_n = sphere->get_vertices().size();
  auto const &cyl_verts = cylinder->get_vertices();
  ASSERT_EQ(baked.vertices.size(), sphere_n + cyl_verts.size());

  bool saw_anchor_end = false;
  bool saw_tail_end = false;
  for (std::size_t i = 0; i < cyl_verts.size(); ++i) {
    RiggedVertex const &v = baked.vertices[sphere_n + i];
    float const src_y = cyl_verts[i].position[1];

    // Weight sum must equal 1.
    float const sum = v.bone_weights[0] + v.bone_weights[1] +
                      v.bone_weights[2] + v.bone_weights[3];
    EXPECT_NEAR(sum, 1.0F, kEps) << "cyl vertex " << i;

    // Secondary slots (2,3) unused.
    EXPECT_FLOAT_EQ(v.bone_weights[2], 0.0F);
    EXPECT_FLOAT_EQ(v.bone_weights[3], 0.0F);

    // Primary bone is the anchor (A), secondary is the tail (B).
    EXPECT_EQ(v.bone_indices[0], ToyGraph::kBoneA);
    EXPECT_EQ(v.bone_indices[1], ToyGraph::kBoneB);

    if (std::abs(src_y + 0.5F) < 1e-4F) {
      // y == -0.5 → fully anchor.
      EXPECT_NEAR(v.bone_weights[0], 1.0F, kEps);
      EXPECT_NEAR(v.bone_weights[1], 0.0F, kEps);
      saw_anchor_end = true;
    } else if (std::abs(src_y - 0.5F) < 1e-4F) {
      // y == +0.5 → fully tail.
      EXPECT_NEAR(v.bone_weights[0], 0.0F, kEps);
      EXPECT_NEAR(v.bone_weights[1], 1.0F, kEps);
      saw_tail_end = true;
    }
    EXPECT_EQ(v.color_role, 5U);
  }
  EXPECT_TRUE(saw_anchor_end);
  EXPECT_TRUE(saw_tail_end);
}

TEST(RiggedMeshBake, AllVertexPositionsAreFinite) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  ASSERT_FALSE(baked.vertices.empty());
  for (RiggedVertex const &v : baked.vertices) {
    for (float p : v.position_bone_local) {
      EXPECT_TRUE(std::isfinite(p));
    }
    for (float n : v.normal_bone_local) {
      EXPECT_TRUE(std::isfinite(n));
    }
    for (float w : v.bone_weights) {
      EXPECT_TRUE(std::isfinite(w));
      EXPECT_GE(w, 0.0F);
      EXPECT_LE(w, 1.0F);
    }
  }
}

TEST(RiggedMeshBake, IndicesReferenceValidVertices) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto const n = static_cast<std::uint32_t>(baked.vertices.size());
  ASSERT_GT(n, 0U);
  for (std::uint32_t idx : baked.indices) {
    EXPECT_LT(idx, n);
  }
}

TEST(RiggedMeshBake, GlWrapperExposesCpuSizes) {
  ToyGraph t;
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto mesh = bake_rigged_mesh(input);
  ASSERT_NE(mesh, nullptr);

  auto *sphere = Render::GL::get_unit_sphere();
  auto *cylinder = Render::GL::get_unit_cylinder();
  EXPECT_EQ(mesh->vertex_count(),
            sphere->get_vertices().size() + cylinder->get_vertices().size());
  EXPECT_EQ(mesh->index_count(),
            sphere->get_indices().size() + cylinder->get_indices().size());
}

} // namespace
