// Stage 0 guard test — locks in the draw-order invariant that regressed earlier
// and caused selection rings to render underneath terrain. The sort order must
// be driven by DrawCmd type alone (terrain -> meshes -> selection rings ->
// UI overlays); CommandPriority is scheduling metadata and MUST NOT influence
// the sort key, otherwise Critical-priority selection rings sort before the
// terrain that should render beneath them.

#include "render/draw_queue.h"
#include "render/frame_budget.h"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

using Render::CommandPriority;
using Render::GL::DrawQueue;
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

  // Submit in reverse-desired order with INVERTED priorities to catch any
  // regression that re-introduces priority into the sort key.
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
  feature.kind = TerrainFeatureCmd::Kind::Road;
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
  // Explicit regression guard: a Critical SelectionRing submitted before a
  // Low-priority Mesh must still sort after the mesh. This is the exact
  // bug that produced "no selection ring visible" in-game.
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
  first.kind = TerrainFeatureCmd::Kind::Riverbank;
  first.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x1));
  queue.submit(first);

  TerrainFeatureCmd second;
  second.kind = TerrainFeatureCmd::Kind::Riverbank;
  second.visibility.texture =
      reinterpret_cast<Texture *>(static_cast<std::uintptr_t>(0x2));
  queue.submit(second);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2U);
  EXPECT_NE(queue.sort_key_for_sorted(0), queue.sort_key_for_sorted(1));
}

TEST(FrameBudgetConfig, PartialRenderDefaultsOff) {
  // Stage 0 flipped this default so gameplay-critical commands are never
  // skipped mid-queue. Leaving partial rendering opt-in avoids the flicker
  // regression that came from dropping Normal-priority units past the
  // hard deadline.
  Render::FrameBudgetConfig cfg;
  EXPECT_FALSE(cfg.allow_partial_render);
}

} // namespace
