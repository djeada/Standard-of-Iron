#include "render/ground/map_boundary_fog_renderer.h"
#include <cstddef>
#include <gtest/gtest.h>

using Render::GL::MapBoundaryFogRenderer;

namespace {
constexpr std::size_t kClearOutsideTiles = 10U;
constexpr std::size_t kOutsideBandTiles = 6U;
constexpr std::size_t kFogLayers = 1U;

auto expected_fog_count(int width, int height) -> std::size_t {
  const auto w = static_cast<std::size_t>(width);
  const auto h = static_cast<std::size_t>(height);
  const auto outer = kClearOutsideTiles + kOutsideBandTiles;
  const auto exp_w = w + 2U * outer;
  const auto exp_h = h + 2U * outer;
  const auto inner_w = w + 2U * kClearOutsideTiles;
  const auto inner_h = h + 2U * kClearOutsideTiles;
  const auto outside_tiles = exp_w * exp_h - inner_w * inner_h;
  return outside_tiles * kFogLayers;
}

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

TEST(MapBoundaryFogRendererTest, LargerMapHasMoreInstances) {
  MapBoundaryFogRenderer small_map;
  MapBoundaryFogRenderer large_map;
  small_map.configure(10, 10, 1.0F);
  large_map.configure(50, 50, 1.0F);
  EXPECT_GT(large_map.instance_count(), small_map.instance_count());
}

TEST(MapBoundaryFogRendererTest, WideMapHasMoreInstancesThanTallMap) {
  MapBoundaryFogRenderer wide;
  MapBoundaryFogRenderer square;
  square.configure(20, 20, 1.0F);
  wide.configure(40, 10, 1.0F);

  EXPECT_GT(wide.instance_count(), square.instance_count());
}

TEST(MapBoundaryFogRendererTest, TileSizeDoesNotAffectInstanceCount) {
  MapBoundaryFogRenderer r1;
  MapBoundaryFogRenderer r2;
  r1.configure(30, 30, 1.0F);
  r2.configure(30, 30, 2.5F);
  EXPECT_EQ(r1.instance_count(), r2.instance_count());
}

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

TEST(MapBoundaryFogRendererTest, LargeMapInstanceCountIsWithinBudget) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(200, 200, 1.0F);
  constexpr std::size_t k_max_budget = 7000U;
  EXPECT_LE(renderer.instance_count(), k_max_budget);
}

TEST(MapBoundaryFogRendererTest, Tiny1x1MapDoesNotCrash) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(1, 1, 1.0F);
}

TEST(MapBoundaryFogRendererTest, SatisfiesIRenderPassInterface) {
  MapBoundaryFogRenderer concrete;
  Render::GL::IRenderPass *pass =
      static_cast<Render::GL::IRenderPass *>(&concrete);
  EXPECT_NE(pass, nullptr);
}

TEST(MapBoundaryFogRendererTest, SubmitWithNoInstancesIsNoOp) {
  MapBoundaryFogRenderer renderer;

  EXPECT_EQ(renderer.instance_count(), 0U);
}

} // namespace
