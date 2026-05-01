

#include "render/creature/part_graph.h"
#include "render/creature/skeleton.h"
#include "render/creature/spec.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/render_archetype.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_bake.h"
#include "render/rigged_mesh_cache.h"
#include "render/static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>

namespace {

using namespace Render::Creature;
using Render::GL::RenderArchetype;
using Render::GL::RenderArchetypeBuilder;
using Render::GL::RenderArchetypeLod;
using Render::GL::RiggedMeshCache;
using Render::GL::RiggedVertex;

constexpr float kEps = 1e-4F;

struct OneBoneGraph {
  static constexpr BoneIndex kBoneA = 0;
  std::array<BoneDef, 1> bones{BoneDef{"A", kInvalidBone}};
  SkeletonTopology topology{std::span<const BoneDef>{bones}, {}};
  PartGraph graph{};
  std::array<BoneWorldMatrix, 1> bind_pose{QMatrix4x4{}};
};

auto make_simple_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder b("test_attachment");
    b.use_lod(RenderArchetypeLod::Full);

    b.add_palette_box(QVector3D{0.0F, 0.0F, 0.0F}, QVector3D{1.0F, 1.0F, 1.0F},
                      2);
    b.add_box(QVector3D{0.0F, 1.0F, 0.0F}, QVector3D{0.5F, 0.5F, 0.5F},
              QVector3D{1.0F, 0.0F, 0.0F});
    return std::move(b).build();
  }();
  return archetype;
}

TEST(StaticAttachmentBake, EmptyAttachmentsLeaveBakeUnchanged) {
  OneBoneGraph t;
  BakeInput baseline{
      &t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}, {}};
  auto a = bake_rigged_mesh_cpu(baseline);

  BakeInput same_with_explicit_empty{
      &t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
      std::span<const StaticAttachmentSpec>{}};
  auto b = bake_rigged_mesh_cpu(same_with_explicit_empty);
  EXPECT_EQ(a.vertices.size(), b.vertices.size());
  EXPECT_EQ(a.indices.size(), b.indices.size());
}

TEST(StaticAttachmentBake, AttachmentVerticesAppendedToBodyMesh) {
  OneBoneGraph t;

  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = OneBoneGraph::kBoneA;
  spec.palette_role_remap[2] = 7;
  spec.override_color_role = 4;
  std::array<StaticAttachmentSpec, 1> attachments{spec};

  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *unit_cube = Render::GL::get_unit_cube();
  ASSERT_NE(unit_cube, nullptr);
  EXPECT_EQ(baked.vertices.size(), unit_cube->get_vertices().size() * 2)
      << "two cubes worth of vertices, both from the attachment";
  EXPECT_EQ(baked.indices.size(), unit_cube->get_indices().size() * 2);

  for (RiggedVertex const &v : baked.vertices) {
    EXPECT_EQ(v.bone_indices[0], OneBoneGraph::kBoneA);
    EXPECT_FLOAT_EQ(v.bone_weights[0], 1.0F);
    EXPECT_FLOAT_EQ(v.bone_weights[1], 0.0F);
    EXPECT_FLOAT_EQ(v.bone_weights[2], 0.0F);
    EXPECT_FLOAT_EQ(v.bone_weights[3], 0.0F);
  }
}

TEST(StaticAttachmentBake, PaletteRemapAndOverrideRoleAreApplied) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = OneBoneGraph::kBoneA;
  spec.palette_role_remap[2] = 9;
  spec.override_color_role = 3;
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *unit_cube = Render::GL::get_unit_cube();
  std::size_t const cube_n = unit_cube->get_vertices().size();

  for (std::size_t i = 0; i < cube_n; ++i) {
    EXPECT_EQ(baked.vertices[i].color_role, 9U) << "palette draw vertex " << i;
  }

  for (std::size_t i = cube_n; i < baked.vertices.size(); ++i) {
    EXPECT_EQ(baked.vertices[i].color_role, 3U) << "fixed draw vertex " << i;
  }
}

TEST(StaticAttachmentBake, LocalOffsetTranslatesAttachmentVertices) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = OneBoneGraph::kBoneA;
  spec.palette_role_remap[2] = 1;
  spec.override_color_role = 1;
  spec.local_offset.translate(10.0F, 0.0F, 0.0F);
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  for (RiggedVertex const &v : baked.vertices) {
    EXPECT_GE(v.position_bone_local[0], 9.0F);
  }
}

TEST(StaticAttachmentBake, OutOfRangeSocketIsSkipped) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = 99;
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  EXPECT_TRUE(baked.vertices.empty());
  EXPECT_TRUE(baked.indices.empty());
}

TEST(StaticAttachmentBake, NullArchetypeAttachmentIsNoop) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  EXPECT_TRUE(baked.vertices.empty());
}

TEST(StaticAttachmentBake, AttachmentsHashKeysCacheEntries) {
  RiggedMeshCache cache;

  auto const &spec_ref = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  StaticAttachmentSpec att{};
  att.archetype = &make_simple_archetype();
  att.socket_bone_index = 0;
  std::array<StaticAttachmentSpec, 1> attachments{att};

  auto const *no_attach =
      cache.get_or_bake(spec_ref, Render::Creature::CreatureLOD::Full, bind);
  auto const *with_attach =
      cache.get_or_bake(spec_ref, Render::Creature::CreatureLOD::Full, bind, 0,
                        std::span<const StaticAttachmentSpec>{attachments});
  auto const *no_attach_again =
      cache.get_or_bake(spec_ref, Render::Creature::CreatureLOD::Full, bind);

  ASSERT_NE(no_attach, nullptr);
  ASSERT_NE(with_attach, nullptr);
  EXPECT_NE(no_attach, with_attach)
      << "attachment hash must produce a distinct cache entry";
  EXPECT_EQ(no_attach, no_attach_again)
      << "two body-only calls must hit the same cache slot";
  EXPECT_GT(with_attach->mesh->vertex_count(), no_attach->mesh->vertex_count())
      << "merged mesh must have more vertices than body-only";

  const auto h0 = Render::Creature::static_attachments_hash(nullptr, 0);
  const auto h1 =
      Render::Creature::static_attachments_hash(attachments.data(), 1);
  EXPECT_NE(h0, h1);
}

TEST(StaticAttachmentBake, AttachmentsCoexistWithPrimitiveGraph) {

  std::array<BoneDef, 1> bones{BoneDef{"A", kInvalidBone}};
  SkeletonTopology topology{std::span<const BoneDef>{bones}, {}};
  std::array<PrimitiveInstance, 1> prims{};
  prims[0].debug_name = "sphere";
  prims[0].shape = PrimitiveShape::Sphere;
  prims[0].params.anchor_bone = 0;
  prims[0].params.radius = 1.0F;
  prims[0].color_role = 6;

  PartGraph graph{};
  graph.primitives = std::span<const PrimitiveInstance>{prims};
  std::array<BoneWorldMatrix, 1> bind_pose{QMatrix4x4{}};

  StaticAttachmentSpec att{};
  att.archetype = &make_simple_archetype();
  att.socket_bone_index = 0;
  att.override_color_role = 3;
  std::array<StaticAttachmentSpec, 1> attachments{att};

  BakeInput input{&graph, std::span<const BoneWorldMatrix>{bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto *sphere = Render::GL::get_unit_sphere();
  auto *cube = Render::GL::get_unit_cube();
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(cube, nullptr);
  EXPECT_EQ(baked.vertices.size(),
            sphere->get_vertices().size() + cube->get_vertices().size() * 2);
  EXPECT_EQ(baked.indices.size(),
            sphere->get_indices().size() + cube->get_indices().size() * 2);

  std::size_t const sphere_n = sphere->get_vertices().size();
  for (std::size_t i = 0; i < sphere_n; ++i) {
    EXPECT_EQ(baked.vertices[i].color_role, 6U);
  }
}

} // namespace
