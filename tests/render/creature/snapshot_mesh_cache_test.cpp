// Tests for SnapshotMeshCache: pre-skinned body geometry for low-frame
// animation states (Idle, Dead). The cache lives on the Renderer and is
// consulted by CreaturePipeline::submit_requests when the target
// archetype + state is flagged as a snapshot state. These tests work
// directly against the cache (no GL context needed) by feeding a hand-
// rolled RiggedMeshEntry — the exact same shape that
// rigged_entry_ensure_skin_atlas_from_blob produces in production.

#include "render/bone_palette_arena.h"
#include "render/creature/render_request.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"
#include "render/snapshot_mesh_cache.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace {

using Render::Creature::AnimationStateId;
using Render::GL::RiggedMeshEntry;
using Render::GL::RiggedVertex;
using Render::GL::SnapshotMeshCache;

auto make_two_bone_quad_entry() -> std::unique_ptr<RiggedMeshEntry> {
  auto entry = std::make_unique<RiggedMeshEntry>();

  std::vector<RiggedVertex> vertices(4);
  for (auto &v : vertices) {
    v.position_bone_local = {0.0F, 0.0F, 0.0F};
    v.normal_bone_local = {0.0F, 1.0F, 0.0F};
    v.tex_coord = {0.0F, 0.0F};
    v.bone_indices = {0, 0, 0, 0};
    v.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    v.color_role = 1;
  }

  vertices[0].position_bone_local = {-1.0F, 0.0F, 0.0F};
  vertices[1].position_bone_local = {1.0F, 0.0F, 0.0F};
  vertices[2].position_bone_local = {0.0F, 1.0F, 0.0F};

  vertices[3].position_bone_local = {0.0F, 0.0F, 0.0F};
  vertices[3].bone_indices = {1, 0, 0, 0};
  vertices[3].color_role = 2;

  std::vector<std::uint32_t> indices = {0, 1, 2, 0, 2, 3};

  entry->mesh = std::make_unique<Render::GL::RiggedMesh>(std::move(vertices),
                                                         std::move(indices));

  entry->skinned_bone_count = 2U;
  entry->skinned_frame_total = 2U;
  entry->skinned_palettes.assign(
      static_cast<std::size_t>(entry->skinned_frame_total) *
          entry->skinned_bone_count,
      QMatrix4x4{});

  // Frame 0: bone 0 = +Y translate(2), bone 1 = identity.
  QMatrix4x4 f0_b0;
  f0_b0.translate(0.0F, 2.0F, 0.0F);
  entry->skinned_palettes[0] = f0_b0;
  entry->skinned_palettes[1].setToIdentity();

  // Frame 1: bone 0 = identity, bone 1 = scale 3x.
  entry->skinned_palettes[2].setToIdentity();
  QMatrix4x4 f1_b1;
  f1_b1.scale(3.0F);
  entry->skinned_palettes[3] = f1_b1;

  return entry;
}

TEST(SnapshotMeshCache, IdentityPaletteIsAllIdentity) {
  const QMatrix4x4 *p = SnapshotMeshCache::identity_palette();
  ASSERT_NE(p, nullptr);
  for (std::size_t i = 0; i < Render::GL::BonePaletteArena::kPaletteWidth;
       ++i) {
    EXPECT_TRUE(p[i].isIdentity()) << "slot " << i;
  }
}

TEST(SnapshotMeshCache, BakesAndCachesEntry) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key key{};
  key.archetype = 0U;
  key.variant = 0U;
  key.state = AnimationStateId::Idle;
  key.frame_in_clip = 0U;

  const auto *snap = cache.get_or_bake(key, *source, /*global_frame=*/0U);
  ASSERT_NE(snap, nullptr);
  ASSERT_NE(snap->mesh, nullptr);
  EXPECT_EQ(snap->mesh->vertex_count(), 4U);
  EXPECT_EQ(snap->mesh->index_count(), 6U);
  EXPECT_EQ(cache.size(), 1U);

  // Cache hit returns the same pointer.
  const auto *snap2 = cache.get_or_bake(key, *source, 0U);
  EXPECT_EQ(snap, snap2);
  EXPECT_EQ(cache.size(), 1U);
}

TEST(SnapshotMeshCache, AppliesSkinningAtFrameZero) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key key{};
  key.frame_in_clip = 0U;
  const auto *snap = cache.get_or_bake(key, *source, 0U);
  ASSERT_NE(snap, nullptr);

  const auto &v = snap->mesh->get_vertices();
  ASSERT_EQ(v.size(), 4U);

  // Vertices 0..2 use bone 0 (translate +Y by 2).
  EXPECT_FLOAT_EQ(v[0].position_bone_local[0], -1.0F);
  EXPECT_FLOAT_EQ(v[0].position_bone_local[1], 2.0F);
  EXPECT_FLOAT_EQ(v[1].position_bone_local[0], 1.0F);
  EXPECT_FLOAT_EQ(v[1].position_bone_local[1], 2.0F);
  EXPECT_FLOAT_EQ(v[2].position_bone_local[1], 3.0F);

  // Vertex 3 uses bone 1 (identity at frame 0).
  EXPECT_FLOAT_EQ(v[3].position_bone_local[0], 0.0F);
  EXPECT_FLOAT_EQ(v[3].position_bone_local[1], 0.0F);

  // All baked vertices collapse to bone 0 with weight 1.
  for (const auto &bv : v) {
    EXPECT_EQ(bv.bone_indices[0], 0);
    EXPECT_FLOAT_EQ(bv.bone_weights[0], 1.0F);
    EXPECT_FLOAT_EQ(bv.bone_weights[1], 0.0F);
  }

  // Color role survives the bake.
  EXPECT_EQ(v[0].color_role, 1U);
  EXPECT_EQ(v[3].color_role, 2U);
}

TEST(SnapshotMeshCache, DifferentFramesProduceDifferentMeshes) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key key0{};
  key0.frame_in_clip = 0U;
  SnapshotMeshCache::Key key1{};
  key1.frame_in_clip = 1U;

  const auto *s0 = cache.get_or_bake(key0, *source, 0U);
  const auto *s1 = cache.get_or_bake(key1, *source, 1U);
  ASSERT_NE(s0, nullptr);
  ASSERT_NE(s1, nullptr);
  EXPECT_NE(s0, s1);
  EXPECT_EQ(cache.size(), 2U);

  // Vertex 3 uses bone 1: at frame 1 bone 1 scales by 3 (origin maps to
  // origin), at frame 0 bone 1 is identity (also origin -> origin).
  // Vertex 0 uses bone 0: frame 0 = translate +Y2, frame 1 = identity.
  EXPECT_FLOAT_EQ(s0->mesh->get_vertices()[0].position_bone_local[1], 2.0F);
  EXPECT_FLOAT_EQ(s1->mesh->get_vertices()[0].position_bone_local[1], 0.0F);
}

TEST(SnapshotMeshCache, OutOfRangeFrameReturnsNull) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key key{};
  EXPECT_EQ(cache.get_or_bake(key, *source, /*global_frame=*/99U), nullptr);
  EXPECT_EQ(cache.size(), 0U);
}

TEST(SnapshotMeshCache, EmptySourceMeshReturnsNull) {
  RiggedMeshEntry empty{};
  empty.skinned_bone_count = 1U;
  empty.skinned_frame_total = 1U;
  empty.skinned_palettes.resize(1);
  empty.skinned_palettes[0].setToIdentity();

  SnapshotMeshCache cache;
  SnapshotMeshCache::Key key{};
  EXPECT_EQ(cache.get_or_bake(key, empty, 0U), nullptr);
}

TEST(SnapshotMeshCache, ClearDropsAllEntries) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key key{};
  ASSERT_NE(cache.get_or_bake(key, *source, 0U), nullptr);
  EXPECT_EQ(cache.size(), 1U);

  cache.clear();
  EXPECT_EQ(cache.size(), 0U);
}

TEST(SnapshotMeshCache, KeyDistinguishesArchetypeVariantState) {
  auto source = make_two_bone_quad_entry();
  SnapshotMeshCache cache;

  SnapshotMeshCache::Key a{};
  a.archetype = 0U;
  a.variant = 0U;
  a.state = AnimationStateId::Idle;
  SnapshotMeshCache::Key b = a;
  b.archetype = 1U;
  SnapshotMeshCache::Key c = a;
  c.variant = 1U;
  SnapshotMeshCache::Key d = a;
  d.state = AnimationStateId::Dead;

  cache.get_or_bake(a, *source, 0U);
  cache.get_or_bake(b, *source, 0U);
  cache.get_or_bake(c, *source, 0U);
  cache.get_or_bake(d, *source, 0U);

  EXPECT_EQ(cache.size(), 4U);
}

} // namespace
