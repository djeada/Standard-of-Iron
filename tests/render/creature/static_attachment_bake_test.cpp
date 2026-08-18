#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "render/creature/archetype_registry.h"
#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/spec.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/render_archetype.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_bake.h"
#include "render/rigged_mesh_cache.h"
#include "render/snapshot_mesh_cache.h"
#include "render/static_attachment_spec.h"

namespace {

using namespace Render::Creature;
using Render::GL::RenderArchetype;
using Render::GL::RenderArchetypeBuilder;
using Render::GL::RenderArchetypeLod;
using Render::GL::RiggedMeshCache;
using Render::GL::RiggedVertex;

struct OneBoneGraph {
  static constexpr BoneIndex k_bone_a = 0;
  std::array<BoneDef, 1> bones{BoneDef{"A", k_invalid_bone}};
  SkeletonTopology topology{std::span<const BoneDef>{bones}, {}};
  PartGraph graph{};
  std::array<BoneWorldMatrix, 1> bind_pose{QMatrix4x4{}};
};

class RuntimeBakeGuardReset {
public:
  RuntimeBakeGuardReset() { Render::Creature::set_runtime_bake_forbidden(false); }
  ~RuntimeBakeGuardReset() { Render::Creature::set_runtime_bake_forbidden(false); }
};

auto make_simple_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder b("test_attachment");
    b.use_lod(RenderArchetypeLod::Full);

    b.add_palette_box(QVector3D{0.0F, 0.0F, 0.0F}, QVector3D{1.0F, 1.0F, 1.0F}, 2);
    b.add_box(QVector3D{0.0F, 1.0F, 0.0F},
              QVector3D{0.5F, 0.5F, 0.5F},
              QVector3D{1.0F, 0.0F, 0.0F});
    return std::move(b).build();
  }();
  return archetype;
}

TEST(StaticAttachmentBake, EmptyAttachmentsLeaveBakeUnchanged) {
  OneBoneGraph t;
  BakeInput baseline{&t.graph, std::span<const BoneWorldMatrix>{t.bind_pose}, {}};
  auto a = bake_rigged_mesh_cpu(baseline);

  BakeInput same_with_explicit_empty{&t.graph,
                                     std::span<const BoneWorldMatrix>{t.bind_pose},
                                     std::span<const StaticAttachmentSpec>{}};
  auto b = bake_rigged_mesh_cpu(same_with_explicit_empty);
  EXPECT_EQ(a.vertices.size(), b.vertices.size());
  EXPECT_EQ(a.indices.size(), b.indices.size());
}

TEST(StaticAttachmentBake, AttachmentVerticesAppendedToBodyMesh) {
  OneBoneGraph t;

  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = OneBoneGraph::k_bone_a;
  spec.palette_role_remap[2] = 7;
  spec.override_color_role = 4;
  std::array<StaticAttachmentSpec, 1> attachments{spec};

  BakeInput input{&t.graph,
                  std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto* unit_cube = Render::GL::get_unit_cube();
  ASSERT_NE(unit_cube, nullptr);
  EXPECT_EQ(baked.vertices.size(), unit_cube->get_vertices().size() * 2)
      << "two cubes worth of vertices, both from the attachment";
  EXPECT_EQ(baked.indices.size(), unit_cube->get_indices().size() * 2);

  for (RiggedVertex const& v : baked.vertices) {
    EXPECT_EQ(v.bone_indices[0], OneBoneGraph::k_bone_a);
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
  spec.socket_bone_index = OneBoneGraph::k_bone_a;
  spec.palette_role_remap[2] = 9;
  spec.override_color_role = 3;
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph,
                  std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto* unit_cube = Render::GL::get_unit_cube();
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
  spec.socket_bone_index = OneBoneGraph::k_bone_a;
  spec.palette_role_remap[2] = 1;
  spec.override_color_role = 1;
  spec.local_offset.translate(10.0F, 0.0F, 0.0F);
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph,
                  std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  for (RiggedVertex const& v : baked.vertices) {
    EXPECT_GE(v.position_bone_local[0], 9.0F);
  }
}

TEST(StaticAttachmentBake, OutOfRangeSocketIsSkipped) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  spec.archetype = &make_simple_archetype();
  spec.socket_bone_index = 99;
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph,
                  std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  EXPECT_TRUE(baked.vertices.empty());
  EXPECT_TRUE(baked.indices.empty());
}

TEST(StaticAttachmentBake, NullArchetypeAttachmentIsNoop) {
  OneBoneGraph t;
  StaticAttachmentSpec spec{};
  std::array<StaticAttachmentSpec, 1> attachments{spec};
  BakeInput input{&t.graph,
                  std::span<const BoneWorldMatrix>{t.bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);
  EXPECT_TRUE(baked.vertices.empty());
}

TEST(StaticAttachmentBake, AttachmentsHashKeysCacheEntries) {
  RiggedMeshCache cache;

  auto const& spec_ref = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  StaticAttachmentSpec att{};
  att.archetype = &make_simple_archetype();
  att.socket_bone_index = 0;
  std::array<StaticAttachmentSpec, 1> attachments{att};

  auto const* no_attach =
      cache.get_or_bake(spec_ref, Render::Creature::CreatureLOD::Full, bind);
  auto const* with_attach =
      cache.get_or_bake(spec_ref,
                        Render::Creature::CreatureLOD::Full,
                        bind,
                        0,
                        std::span<const StaticAttachmentSpec>{attachments});
  auto const* no_attach_again =
      cache.get_or_bake(spec_ref, Render::Creature::CreatureLOD::Full, bind);

  ASSERT_NE(no_attach, nullptr);
  ASSERT_NE(with_attach, nullptr);
  EXPECT_NE(no_attach, with_attach)
      << "attachment hash must produce a distinct cache entry";
  EXPECT_EQ(no_attach, no_attach_again)
      << "two body-only calls must hit the same cache slot";
  EXPECT_EQ(with_attach->mesh, no_attach->mesh)
      << "full-detail bodies must be shared across equipment combinations";
  ASSERT_EQ(with_attach->attachment_meshes.size(), 1U);
  EXPECT_GT(with_attach->attachment_meshes.front()->vertex_count(), 0U);

  const auto h0 = Render::Creature::static_attachments_hash(nullptr, 0);
  const auto h1 = Render::Creature::static_attachments_hash(attachments.data(), 1);
  EXPECT_NE(h0, h1);
}

TEST(StaticAttachmentBake, AttachmentsCoexistWithPrimitiveGraph) {
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

  BakeInput input{&graph,
                  std::span<const BoneWorldMatrix>{bind_pose},
                  std::span<const StaticAttachmentSpec>{attachments}};
  auto baked = bake_rigged_mesh_cpu(input);

  auto* sphere = Render::GL::get_unit_sphere();
  auto* cube = Render::GL::get_unit_cube();
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

auto head_attachment() -> StaticAttachmentSpec {

  static const auto archetype = [] {
    const std::array<Render::GL::GeneratedEquipmentPrimitive, 1> primitives{{
        Render::GL::generated_sphere(QVector3D(0.0F, 0.0F, 1.4F), 0.30F, 0U),
    }};
    return Render::GL::build_generated_equipment_archetype("test/head_marker",
                                                           primitives);
  }();

  const auto bind_palette = Render::Humanoid::humanoid_bind_palette();
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  return Render::Equipment::build_static_attachment({
      .archetype = &archetype,
      .socket_bone_index =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head),
      .bind_radius = bind_frames.head.radius,
      .bind_socket_transform =
          bind_palette[static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Head)],
  });
}

TEST(StaticAttachmentBake, TheGeneratedArchetypeActuallyHasDrawsAndBoneRoom) {
  const std::array<Render::GL::GeneratedEquipmentPrimitive, 1> primitives{{
      Render::GL::generated_sphere(QVector3D(0.0F, 0.0F, 1.4F), 0.30F, 0U),
  }};
  const auto archetype =
      Render::GL::build_generated_equipment_archetype("test/probe", primitives);
  const auto& slice = archetype.lods[0];
  EXPECT_FALSE(slice.draws.empty()) << "the builder produced no draws at all";
  std::size_t verts = 0;
  for (const auto& draw : slice.draws) {
    if (draw.mesh != nullptr) {
      verts += draw.mesh->get_vertices().size();
    }
  }
  EXPECT_GT(verts, 0U) << "the draws carry no vertices";

  const auto bind = Render::Humanoid::humanoid_bind_palette();
  const auto spec = head_attachment();
  EXPECT_LT(static_cast<std::size_t>(spec.socket_bone_index), bind.size())
      << "the head bone is outside the bind pose, so the baker skips it";
}

TEST(StaticAttachmentBake, GeneratedGeometryReachesTheBakedMesh) {
  const RuntimeBakeGuardReset guard;
  RiggedMeshCache cache;
  const auto& spec = Render::Humanoid::humanoid_creature_spec();
  const auto bind = Render::Humanoid::humanoid_bind_palette();

  const auto* bare = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(bare, nullptr);
  ASSERT_NE(bare->mesh, nullptr);
  const auto bare_vertices = bare->mesh->vertex_count();
  ASSERT_GT(bare_vertices, 0U);

  const std::array<StaticAttachmentSpec, 1> attachments{{head_attachment()}};
  ASSERT_NE(attachments[0].archetype, nullptr);

  const auto* attached =
      cache.get_or_bake(spec, CreatureLOD::Full, bind, 0, attachments);
  ASSERT_NE(attached, nullptr);
  ASSERT_NE(attached->mesh, nullptr);

  EXPECT_NE(attached, bare) << "an attachment changes the key, so it must not hit the "
                               "cache entry baked without one";

  EXPECT_EQ(attached->mesh->vertex_count(), bare_vertices)
      << "the body mesh is shared and must not change";
  ASSERT_FALSE(attached->attachment_meshes.empty())
      << "the attachment's geometry never made it into a baked mesh";
  std::size_t attachment_vertices = 0;
  for (const auto& mesh : attached->attachment_meshes) {
    ASSERT_NE(mesh, nullptr);
    attachment_vertices += mesh->vertex_count();
  }
  EXPECT_GT(attachment_vertices, 0U);
}

TEST(StaticAttachmentBake, PrehashedLookupSeesTheAttachmentToo) {
  const RuntimeBakeGuardReset guard;
  RiggedMeshCache cache;
  const auto& spec = Render::Humanoid::humanoid_creature_spec();
  const auto bind = Render::Humanoid::humanoid_bind_palette();

  const auto* bare = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(bare, nullptr);
  const auto bare_vertices = bare->mesh->vertex_count();

  const std::array<StaticAttachmentSpec, 1> attachments{{head_attachment()}};
  const auto hash =
      Render::Creature::static_attachments_hash(attachments.data(), attachments.size());

  const auto* attached = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Full, bind, 0, attachments, hash, 1U, 0U);
  ASSERT_NE(attached, nullptr);
  ASSERT_FALSE(attached->attachment_meshes.empty())
      << "the prehashed path dropped the attachment";
  EXPECT_GT(attached->attachment_meshes.front()->vertex_count(), 0U);
  EXPECT_GT(bare_vertices, 0U);
}

TEST(StaticAttachmentBake, DerivedArchetypeCarriesItsAttachmentIntoTheRenderHandle) {
  const RuntimeBakeGuardReset guard;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  const auto base_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  const auto* base_desc = registry.get(base_id);
  ASSERT_NE(base_desc, nullptr);
  const auto base_count = base_desc->bake_attachment_count;
  ASSERT_LT(base_count, Render::Creature::ArchetypeDescriptor::k_max_bake_attachments);

  Render::Creature::ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = "test/derived_with_marker";
  desc.bake_attachments[desc.bake_attachment_count++] = head_attachment();
  const auto derived_id = registry.register_archetype(desc);
  ASSERT_NE(derived_id, Render::Creature::k_invalid_archetype);

  const auto* derived_desc = registry.get(derived_id);
  ASSERT_NE(derived_desc, nullptr);
  EXPECT_EQ(derived_desc->bake_attachment_count, base_count + 1);
  EXPECT_EQ(derived_desc->attachments_view().size(),
            static_cast<std::size_t>(base_count + 1));

  bool created = false;
  const auto handle_id =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance()
          .get_or_create(
              Render::Creature::Pipeline::k_humanoid_sword_asset, derived_id, &created);
  ASSERT_NE(handle_id, Render::Creature::k_invalid_creature_render_asset_handle)
      << "the handle registry refused a derived archetype";

  const auto* handle =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance().get(
          handle_id);
  ASSERT_NE(handle, nullptr);
  EXPECT_TRUE(handle->has_static_attachments);
  EXPECT_EQ(handle->attachments.size(), static_cast<std::size_t>(base_count + 1));
  ASSERT_FALSE(handle->attachments.empty());
  EXPECT_NE(handle->attachments.back().archetype, nullptr)
      << "the handle's attachment lost its geometry archetype";
}

TEST(StaticAttachmentBake, SnapshotKeepsTheAttachmentsTheBodyCameWith) {
  const RuntimeBakeGuardReset guard;

  const std::vector<Render::GL::RiggedVertex> body_vertices(24);
  const std::vector<std::uint32_t> body_indices{0U, 1U, 2U};
  const std::vector<Render::GL::RiggedVertex> attachment_vertices(9);
  const std::vector<std::uint32_t> attachment_indices{0U, 1U, 2U};

  Render::GL::RiggedMeshEntry source;
  source.mesh = std::make_shared<Render::GL::RiggedMesh>(body_vertices, body_indices);
  source.attachment_meshes.push_back(std::make_shared<Render::GL::RiggedMesh>(
      attachment_vertices, attachment_indices));

  auto palettes = std::make_shared<std::vector<QMatrix4x4>>(
      std::vector<QMatrix4x4>(Render::Humanoid::k_bone_count));
  source.skin_atlas = std::make_shared<Render::GL::RiggedSkinAtlas>();
  source.skin_atlas->palette_storage = palettes;
  source.skin_atlas->palettes = *palettes;
  source.skin_atlas->frame_total = 1U;
  source.skin_atlas->bone_count =
      static_cast<std::uint32_t>(Render::Humanoid::k_bone_count);

  Render::GL::SnapshotMeshCache cache;
  Render::GL::SnapshotMeshCache::Key key{};
  key.archetype = 1U;
  key.clip_id = 7U;

  const auto* snap = cache.get_or_bake(key, source, 0U);
  ASSERT_NE(snap, nullptr);
  ASSERT_NE(snap->mesh, nullptr);
  EXPECT_EQ(snap->mesh->vertex_count(), body_vertices.size());

  ASSERT_EQ(snap->attachment_meshes.size(), source.attachment_meshes.size())
      << "the snapshot dropped the body's attachments";
  ASSERT_NE(snap->attachment_meshes.front(), nullptr);
  EXPECT_EQ(snap->attachment_meshes.front()->vertex_count(),
            attachment_vertices.size());
  EXPECT_EQ(snap->attachment_meshes.front()->index_count(), attachment_indices.size());
}

} // namespace
