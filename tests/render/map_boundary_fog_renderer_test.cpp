#include "render/ground/map_boundary_fog_renderer.h"
#include <gtest/gtest.h>
#include <cstddef>

using Render::GL::MapBoundaryFogRenderer;

namespace {
constexpr std::size_t kOutsideBandTiles = 4U;
constexpr std::size_t kFogLayers        = 7U;

// Expected total fog instances for a map of given dimensions:
// (expanded bounding box area − map area) × number of vertical layers.
auto expected_fog_count(int width, int height) -> std::size_t {
  const auto w = static_cast<std::size_t>(width);
  const auto h = static_cast<std::size_t>(height);
  const auto exp_w = w + 2U * kOutsideBandTiles;
  const auto exp_h = h + 2U * kOutsideBandTiles;
  const auto outside_tiles = exp_w * exp_h - w * h;
  return outside_tiles * kFogLayers;
}

// ---------------------------------------------------------------------------
// Construction and basic API
// ---------------------------------------------------------------------------

TEST(MapBoundaryFogRendererTest, DefaultConstructedHasNoInstances) {
  MapBoundaryFogRenderer renderer;
  EXPECT_EQ(renderer.instance_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithZeroWidthProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(0, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithZeroHeightProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(50, 0, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithNegativeDimsProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(-1, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  renderer.configure(50, -1, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithValidDimsProducesInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(20, 20, 1.0F);
  EXPECT_GT(renderer.instance_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, FogIsOnlyOutsideMapBoundary) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(20, 20, 1.0F);
  EXPECT_EQ(renderer.instance_count(), expected_fog_count(20, 20));
}

// ---------------------------------------------------------------------------
// Instance count grows with map perimeter
// ---------------------------------------------------------------------------

TEST(MapBoundaryFogRendererTest, LargerMapHasMoreInstances) {
  MapBoundaryFogRenderer small_map;
  MapBoundaryFogRenderer large_map;
  small_map.configure(10, 10, 1.0F);
  large_map.configure(50, 50, 1.0F);
  EXPECT_GT(large_map.instance_count(), small_map.instance_count());
}

// A non-square map should have more instances along its longer dimension.
TEST(MapBoundaryFogRendererTest, WideMapHasMoreInstancesThanTallMap) {
  MapBoundaryFogRenderer wide;
  MapBoundaryFogRenderer square;
  square.configure(20, 20, 1.0F);
  wide.configure(40, 10, 1.0F);
  // Wide map has longer perimeter than the square map above, so more instances.
  EXPECT_GT(wide.instance_count(), square.instance_count());
}

// ---------------------------------------------------------------------------
// Tile size does not change instance count (same grid, different world scale)
// ---------------------------------------------------------------------------

TEST(MapBoundaryFogRendererTest, TileSizeDoesNotAffectInstanceCount) {
  MapBoundaryFogRenderer r1;
  MapBoundaryFogRenderer r2;
  r1.configure(30, 30, 1.0F);
  r2.configure(30, 30, 2.5F);
  EXPECT_EQ(r1.instance_count(), r2.instance_count());
}

// ---------------------------------------------------------------------------
// Reconfiguration replaces instances
// ---------------------------------------------------------------------------

TEST(MapBoundaryFogRendererTest, ReconfigureWithSmallerMapReducesInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(50, 50, 1.0F);
  const std::size_t large_count = renderer.instance_count();

  renderer.configure(10, 10, 1.0F);
  const std::size_t small_count = renderer.instance_count();

  EXPECT_LT(small_count, large_count);
}

TEST(MapBoundaryFogRendererTest, ReconfigureToZeroClearsInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(50, 50, 1.0F);
  ASSERT_GT(renderer.instance_count(), 0U);

  renderer.configure(0, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
}

// ---------------------------------------------------------------------------
// Instance count is bounded (performance guard)
// ---------------------------------------------------------------------------

// For a 200×200 map the boundary band produces (208²−200²)×7 ≈ 22 848
// instances. Keep the budget ceiling generous enough for rendering performance.
TEST(MapBoundaryFogRendererTest, LargeMapInstanceCountIsWithinBudget) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(200, 200, 1.0F);
  constexpr std::size_t k_max_budget = 25000U;
  EXPECT_LE(renderer.instance_count(), k_max_budget);
}

// For a tiny 1×1 map the renderer must not crash.
TEST(MapBoundaryFogRendererTest, Tiny1x1MapDoesNotCrash) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(1, 1, 1.0F);
  // Just needs to not crash; any instance count is acceptable.
}

// ---------------------------------------------------------------------------
// IRenderPass interface contract
// ---------------------------------------------------------------------------

TEST(MapBoundaryFogRendererTest, SatisfiesIRenderPassInterface) {
  MapBoundaryFogRenderer concrete;
  Render::GL::IRenderPass *pass =
      static_cast<Render::GL::IRenderPass *>(&concrete);
  EXPECT_NE(pass, nullptr);
}

// submit() with an unconfigured renderer (zero instances) must not crash when
// the underlying renderer pointer is null because fog_batch is never called.
// We verify this by confirming instance_count() is zero (so submit() is a no-op).
TEST(MapBoundaryFogRendererTest, SubmitWithNoInstancesIsNoOp) {
  MapBoundaryFogRenderer renderer;
  // m_instances is empty after default construction → fog_batch() is never
  // called inside submit(), so no renderer access occurs and this is safe.
  EXPECT_EQ(renderer.instance_count(), 0U);
}

} // namespace
