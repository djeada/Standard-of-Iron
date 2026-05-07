

#include "render/draw_queue.h"
#include "render/frame_budget.h"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

using Render::CommandPriority;
using Render::GL::DrawQueue;
using Render::GL::Mesh;
using Render::GL::MeshCmd;
using Render::GL::MeshCmdIndex;
using Render::GL::SelectionRingCmd;
using Render::GL::SelectionRingCmdIndex;
using Render::GL::TerrainFeatureCmd;
using Render::GL::TerrainFeatureCmdIndex;
using Render::GL::TerrainSurfaceCmd;
using Render::GL::TerrainSurfaceCmdIndex;
using Render::GL::Texture;

TEST(DrawQueueSortOrder, TerrainBeforeMeshBeforeSelectionRing) {
  DrawQueue queue;

  SelectionRingCmd ring;
  ring.priority = CommandPriority::Critical;
  queue.submit(ring);

  MeshCmd mesh;
  mesh.priority = CommandPriority::Low;
  queue.submit(mesh);

  TerrainSurfaceCmd terrain;
  terrain.priority = CommandPriority::Low;
  queue.submit(terrain);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  EXPECT_EQ(queue.get_sorted(0).index(), TerrainSurfaceCmdIndex)
      << "Terrain must render first (beneath everything).";
  EXPECT_EQ(queue.get_sorted(1).index(), MeshCmdIndex)
      << "Meshes (units) must render between terrain and selection rings.";
  EXPECT_EQ(queue.get_sorted(2).index(), SelectionRingCmdIndex)
      << "Selection rings must render last so they appear on top of units "
         "and terrain, regardless of CommandPriority.";
}

TEST(DrawQueueSortOrder, ExplicitTerrainCommandsStayBeforeGameplayMeshes) {
  DrawQueue queue;

  MeshCmd mesh;
  queue.submit(mesh);

  TerrainFeatureCmd feature;
  feature.kind = Render::GL::LinearFeatureKind::Road;
  queue.submit(feature);

  TerrainSurfaceCmd surface;
  queue.submit(surface);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  EXPECT_EQ(queue.get_sorted(0).index(), TerrainSurfaceCmdIndex);
  EXPECT_EQ(queue.get_sorted(1).index(), TerrainFeatureCmdIndex);
  EXPECT_EQ(queue.get_sorted(2).index(), MeshCmdIndex);
}

TEST(DrawQueueSortOrder, PriorityDoesNotInvertTypeOrder) {

  DrawQueue queue;

  SelectionRingCmd ring;
  ring.priority = CommandPriority::Critical;
  queue.submit(ring);

  MeshCmd mesh;
  mesh.priority = CommandPriority::Low;
  queue.submit(mesh);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2U);
  EXPECT_EQ(queue.get_sorted(0).index(), MeshCmdIndex);
  EXPECT_EQ(queue.get_sorted(1).index(), SelectionRingCmdIndex);
}

TEST(DrawQueueSortOrder, RiverbankVisibilityTextureAffectsSortKey) {
  DrawQueue queue;

  TerrainFeatureCmd first;
  first.kind = Render::GL::LinearFeatureKind::Riverbank;
  first.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x1));
  queue.submit(first);

  TerrainFeatureCmd second;
  second.kind = Render::GL::LinearFeatureKind::Riverbank;
  second.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x2));
  queue.submit(second);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2U);
  EXPECT_NE(queue.sort_key_for_sorted(0), queue.sort_key_for_sorted(1));
}

TEST(DrawQueuePreparedBatches, TerrainSurfaceCommandsSharePreparedBatch) {
  DrawQueue queue;

  TerrainSurfaceCmd first;
  first.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x1));
  first.params.is_ground_plane = false;
  queue.submit(first);

  TerrainSurfaceCmd second;
  second.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x2));
  second.params.is_ground_plane = false;
  queue.submit(second);

  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].type, Render::GL::DrawCmdType::TerrainSurface);
  EXPECT_EQ(batches[0].kind, Render::GL::PreparedBatchKind::Single);
  EXPECT_EQ(batches[0].count, 2U);
}

TEST(DrawQueuePreparedBatches,
     TerrainSurfaceCommandsSplitWhenGroundPlaneStateDiffers) {
  DrawQueue queue;

  TerrainSurfaceCmd first;
  first.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x1));
  first.params.is_ground_plane = false;
  queue.submit(first);

  TerrainSurfaceCmd second;
  second.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x2));
  second.params.is_ground_plane = true;
  queue.submit(second);

  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].count, 1U);
  EXPECT_EQ(batches[1].count, 1U);
}

TEST(DrawQueuePreparedBatches, TerrainFeatureCommandsSharePreparedBatch) {
  DrawQueue queue;

  TerrainFeatureCmd first;
  first.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x1));
  first.kind = Render::GL::LinearFeatureKind::Road;
  queue.submit(first);

  TerrainFeatureCmd second;
  second.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x2));
  second.kind = Render::GL::LinearFeatureKind::Road;
  queue.submit(second);

  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].type, Render::GL::DrawCmdType::TerrainFeature);
  EXPECT_EQ(batches[0].kind, Render::GL::PreparedBatchKind::Single);
  EXPECT_EQ(batches[0].count, 2U);
}

TEST(DrawQueuePreparedBatches,
     TerrainFeatureCommandsSplitWhenVisibilityResourcesDiffer) {
  DrawQueue queue;

  TerrainFeatureCmd first;
  first.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x1));
  first.kind = Render::GL::LinearFeatureKind::Riverbank;
  first.visibility.enabled = true;
  first.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x1));
  queue.submit(first);

  TerrainFeatureCmd second;
  second.mesh = reinterpret_cast<Mesh *>(static_cast<std::uintptr_t>(0x2));
  second.kind = Render::GL::LinearFeatureKind::Riverbank;
  second.visibility.enabled = true;
  second.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x2));
  queue.submit(second);

  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].count, 1U);
  EXPECT_EQ(batches[1].count, 1U);
}

TEST(DrawQueueSortOrder, AlreadySortedInputsKeepSubmissionOrder) {
  DrawQueue queue;

  TerrainSurfaceCmd terrain;
  queue.submit(terrain);

  MeshCmd mesh;
  queue.submit(mesh);

  SelectionRingCmd ring;
  queue.submit(ring);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  EXPECT_EQ(queue.get_sorted(0).index(), TerrainSurfaceCmdIndex);
  EXPECT_EQ(queue.get_sorted(1).index(), MeshCmdIndex);
  EXPECT_EQ(queue.get_sorted(2).index(), SelectionRingCmdIndex);
}

TEST(DrawQueueMemory, HighWaterMarkTrackedAfterClear) {
  DrawQueue queue;

  for (int i = 0; i < 50; ++i) {
    MeshCmd cmd;
    queue.submit(cmd);
  }
  queue.sort_for_batching();

  EXPECT_EQ(queue.items_high_water(), 0U)
      << "High-water mark is updated by clear(), not sort_for_batching()";
  EXPECT_EQ(queue.prepared_high_water(), 0U);

  queue.clear();

  EXPECT_EQ(queue.items_high_water(), 50U);
  EXPECT_GE(queue.prepared_high_water(), 1U);
}

TEST(DrawQueueMemory, ReserveForFramePreservesCapacityAcrossClear) {
  DrawQueue queue;

  for (int i = 0; i < 100; ++i) {
    MeshCmd cmd;
    queue.submit(cmd);
  }
  queue.sort_for_batching();
  queue.clear();
  queue.reserve_for_frame();

  EXPECT_GE(queue.items().capacity(), 100U)
      << "reserve_for_frame() must retain capacity from previous frame "
         "high-water mark.";

  queue.clear();
  EXPECT_GE(queue.items().capacity(), 100U)
      << "Subsequent clear() must not shrink reserved capacity.";
}

TEST(FrameBudgetConfig, PartialRenderDefaultsOff) {

  Render::FrameBudgetConfig cfg;
  EXPECT_FALSE(cfg.allow_partial_render);
}

} // namespace
