// Stage 15.5a — rigged mesh bake tests.
//
// Exercises the CPU-only bake pipeline (`bake_rigged_mesh_cpu`) on a
// minimal two-bone skeleton with one sphere (single-bone) and one
// cylinder (two-bone blend). No GL context is required; the shared
// unit meshes expose their CPU vertex/index arrays, so the bake runs
// entirely in host memory.

#include "render/creature/part_graph.h"
#include "render/creature/skeleton.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/primitives.h"
#include "render/horse/horse_spec.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_bake.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
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

auto has_bone_influence(const BakedRiggedMeshCpu &baked,
                        std::uint8_t bone) -> bool {
  for (auto const &v : baked.vertices) {
    for (std::size_t i = 0; i < v.bone_indices.size(); ++i) {
      if (v.bone_indices[i] == bone && v.bone_weights[i] > 0.05F) {
        return true;
      }
    }
  }
  return false;
}

auto skinned_position_at_bind(const RiggedVertex &vertex,
                              std::span<const BoneWorldMatrix> bind_pose)
    -> QVector3D {
  QVector3D out(0.0F, 0.0F, 0.0F);
  QVector4D const local(vertex.position_bone_local[0],
                        vertex.position_bone_local[1],
                        vertex.position_bone_local[2], 1.0F);
  for (std::size_t i = 0; i < vertex.bone_indices.size(); ++i) {
    float const weight = vertex.bone_weights[i];
    std::size_t const bone = vertex.bone_indices[i];
    if (weight <= 0.0F || bone >= bind_pose.size()) {
      continue;
    }
    out += (bind_pose[bone] * local).toVector3D() * weight;
  }
  return out;
}

auto baked_axis_spans(const BakedRiggedMeshCpu &baked,
                      std::span<const BoneWorldMatrix> bind_pose) -> QVector3D {
  QVector3D min_v(std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
  QVector3D max_v(std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest());
  for (auto const &vertex : baked.vertices) {
    QVector3D const p = skinned_position_at_bind(vertex, bind_pose);
    min_v.setX(std::min(min_v.x(), p.x()));
    min_v.setY(std::min(min_v.y(), p.y()));
    min_v.setZ(std::min(min_v.z(), p.z()));
    max_v.setX(std::max(max_v.x(), p.x()));
    max_v.setY(std::max(max_v.y(), p.y()));
    max_v.setZ(std::max(max_v.z(), p.z()));
  }
  return max_v - min_v;
}

auto baked_min_y(const BakedRiggedMeshCpu &baked,
                 std::span<const BoneWorldMatrix> bind_pose) -> float {
  float min_y = std::numeric_limits<float>::max();
  for (auto const &vertex : baked.vertices) {
    min_y = std::min(min_y, skinned_position_at_bind(vertex, bind_pose).y());
  }
  return min_y;
}

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

TEST(RiggedMeshBake, CylinderUsesCustomMeshOverrideWhenProvided) {
  ToyGraph t;
  t.prims[1].custom_mesh = Render::GL::get_unit_cube();
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *sphere = Render::GL::get_unit_sphere();
  auto *cube = Render::GL::get_unit_cube();
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(cube, nullptr);

  EXPECT_EQ(baked.vertices.size(),
            sphere->get_vertices().size() + cube->get_vertices().size());
  EXPECT_EQ(baked.indices.size(),
            sphere->get_indices().size() + cube->get_indices().size());
}

TEST(RiggedMeshBake, LowPolyCylinderMeshHasFewerVerticesThanDefault) {
  auto *default_cylinder = Render::GL::get_unit_cylinder();
  auto *low_poly_cylinder = Render::GL::get_unit_cylinder(6);

  ASSERT_NE(default_cylinder, nullptr);
  ASSERT_NE(low_poly_cylinder, nullptr);
  EXPECT_LT(low_poly_cylinder->get_vertices().size(),
            default_cylinder->get_vertices().size());
  EXPECT_LT(low_poly_cylinder->get_indices().size(),
            default_cylinder->get_indices().size());
}

TEST(RiggedMeshBake, TaperedCylinderNarrowsTowardTail) {
  auto *tapered = Render::GL::get_unit_tapered_cylinder(1.0F, 0.55F, 6);

  ASSERT_NE(tapered, nullptr);
  float anchor_radius = 0.0F;
  float tail_radius = 0.0F;
  for (auto const &vertex : tapered->get_vertices()) {
    float const radius = std::sqrt(vertex.position[0] * vertex.position[0] +
                                   vertex.position[2] * vertex.position[2]);
    if (vertex.position[1] <= -0.49F) {
      anchor_radius = std::max(anchor_radius, radius);
    }
    if (vertex.position[1] >= 0.49F) {
      tail_radius = std::max(tail_radius, radius);
    }
  }
  EXPECT_GT(anchor_radius, tail_radius * 1.7F);
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

TEST(RiggedMeshBake, HorseWholeMeshUsesArticulatedLegAndHeadBones) {
  auto const &spec = Render::Horse::horse_creature_spec();
  BakeInput input{&spec.lod_minimal, Render::Horse::horse_bind_palette()};
  auto baked = bake_rigged_mesh_cpu(input);

  ASSERT_FALSE(baked.vertices.empty());
  EXPECT_TRUE(has_bone_influence(
      baked, static_cast<std::uint8_t>(Render::Horse::HorseBone::KneeFL)));
  EXPECT_TRUE(has_bone_influence(
      baked, static_cast<std::uint8_t>(Render::Horse::HorseBone::ShoulderBR)));
  EXPECT_TRUE(has_bone_influence(
      baked, static_cast<std::uint8_t>(Render::Horse::HorseBone::Head)));
}

TEST(RiggedMeshBake, HorseFullRiggedMeshPreservesOverallScale) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto const bind = Render::Horse::horse_bind_palette();
  auto const full = bake_rigged_mesh_cpu(BakeInput{&spec.lod_full, bind});
  auto const minimal = bake_rigged_mesh_cpu(BakeInput{&spec.lod_minimal, bind});

  ASSERT_FALSE(full.vertices.empty());
  ASSERT_FALSE(minimal.vertices.empty());

  QVector3D const full_span = baked_axis_spans(full, bind);
  QVector3D const minimal_span = baked_axis_spans(minimal, bind);

  EXPECT_GT(full_span.x(), minimal_span.x() * 0.55F);
  EXPECT_GT(full_span.y(), minimal_span.y() * 0.55F);
  EXPECT_GT(full_span.z(), minimal_span.z() * 0.70F);
  EXPECT_LT(full_span.x(), minimal_span.x() * 1.40F);
  EXPECT_LT(full_span.y(), minimal_span.y() * 1.35F);
  EXPECT_LT(full_span.z(), minimal_span.z() * 1.35F);
}

TEST(RiggedMeshBake, HorseFullRiggedMeshStaysNearMinimalGroundContact) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto const bind = Render::Horse::horse_bind_palette();
  auto const full = bake_rigged_mesh_cpu(BakeInput{&spec.lod_full, bind});
  auto const minimal = bake_rigged_mesh_cpu(BakeInput{&spec.lod_minimal, bind});

  ASSERT_FALSE(full.vertices.empty());
  ASSERT_FALSE(minimal.vertices.empty());

  float const full_min_y = baked_min_y(full, bind);
  float const minimal_min_y = baked_min_y(minimal, bind);

  EXPECT_GT(full_min_y, minimal_min_y - 0.45F);
}

TEST(RiggedMeshBake, ElephantWholeMeshUsesKneeAndTrunkBones) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  BakeInput input{&spec.lod_minimal, Render::Elephant::elephant_bind_palette()};
  auto baked = bake_rigged_mesh_cpu(input);

  ASSERT_FALSE(baked.vertices.empty());
  EXPECT_TRUE(has_bone_influence(
      baked,
      static_cast<std::uint8_t>(Render::Elephant::ElephantBone::KneeFL)));
  EXPECT_TRUE(has_bone_influence(
      baked,
      static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootBR)));
  EXPECT_TRUE(has_bone_influence(
      baked,
      static_cast<std::uint8_t>(Render::Elephant::ElephantBone::TrunkTip)));
}

TEST(RiggedMeshBake, ElephantFullRiggedMeshPreservesOverallScale) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto const bind = Render::Elephant::elephant_bind_palette();
  auto const full = bake_rigged_mesh_cpu(BakeInput{&spec.lod_full, bind});
  auto const minimal = bake_rigged_mesh_cpu(BakeInput{&spec.lod_minimal, bind});

  ASSERT_FALSE(full.vertices.empty());
  ASSERT_FALSE(minimal.vertices.empty());

  QVector3D const full_span = baked_axis_spans(full, bind);
  QVector3D const minimal_span = baked_axis_spans(minimal, bind);

  EXPECT_GT(full_span.x(), minimal_span.x() * 0.70F);
  EXPECT_GT(full_span.y(), minimal_span.y() * 0.50F);
  EXPECT_GT(full_span.z(), minimal_span.z() * 0.70F);
  EXPECT_LT(full_span.x(), minimal_span.x() * 1.35F);
  EXPECT_LT(full_span.y(), minimal_span.y() * 1.35F);
  EXPECT_LT(full_span.z(), minimal_span.z() * 1.35F);
}

TEST(RiggedMeshBake, ElephantFullRiggedMeshStaysNearMinimalGroundContact) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto const bind = Render::Elephant::elephant_bind_palette();
  auto const full = bake_rigged_mesh_cpu(BakeInput{&spec.lod_full, bind});
  auto const minimal = bake_rigged_mesh_cpu(BakeInput{&spec.lod_minimal, bind});

  ASSERT_FALSE(full.vertices.empty());
  ASSERT_FALSE(minimal.vertices.empty());

  float const full_min_y = baked_min_y(full, bind);
  float const minimal_min_y = baked_min_y(minimal, bind);

  EXPECT_GT(full_min_y, minimal_min_y - 0.25F);
}

} // namespace
