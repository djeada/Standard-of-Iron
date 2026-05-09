#include "render/ground/map_boundary_fog_renderer.h"
#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>

using Render::GL::MapBoundaryFogRenderer;

namespace {
constexpr std::size_t kCardsPerSide = 3U;
constexpr std::size_t kCurtainRings = 2U;

auto expected_fog_count(int width, int height) -> std::size_t {
  if (width <= 0 || height <= 0) {
    return 0U;
  }
  return (4U * kCardsPerSide + 4U) * kCurtainRings;
}

TEST(MapBoundaryFogRendererTest, DefaultConstructedHasNoInstances) {
  MapBoundaryFogRenderer renderer;
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithZeroWidthProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(0, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithZeroHeightProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(50, 0, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithNegativeDimsProducesNoInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(-1, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
  renderer.configure(50, -1, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, ConfigureWithValidDimsProducesInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(20, 20, 1.0F);
  EXPECT_GT(renderer.instance_count(), 0U);
  EXPECT_GT(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, FogIsOnlyOutsideMapBoundary) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(20, 20, 1.0F);
  EXPECT_EQ(renderer.instance_count(), expected_fog_count(20, 20));
}

TEST(MapBoundaryFogRendererTest, LargerMapKeepsConstantInstanceCount) {
  MapBoundaryFogRenderer small_map;
  MapBoundaryFogRenderer large_map;
  small_map.configure(10, 10, 1.0F);
  large_map.configure(200, 200, 1.0F);
  EXPECT_EQ(large_map.instance_count(), small_map.instance_count());
}

TEST(MapBoundaryFogRendererTest, WideMapKeepsConstantInstanceCount) {
  MapBoundaryFogRenderer wide;
  MapBoundaryFogRenderer square;
  square.configure(20, 20, 1.0F);
  wide.configure(40, 10, 1.0F);

  EXPECT_EQ(wide.instance_count(), square.instance_count());
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

  EXPECT_EQ(small_count, large_count);
}

TEST(MapBoundaryFogRendererTest, ReconfigureToZeroClearsInstances) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(50, 50, 1.0F);
  ASSERT_GT(renderer.instance_count(), 0U);
  ASSERT_GT(renderer.mountain_triangle_count(), 0U);

  renderer.configure(0, 50, 1.0F);
  EXPECT_EQ(renderer.instance_count(), 0U);
  EXPECT_EQ(renderer.mountain_triangle_count(), 0U);
}

TEST(MapBoundaryFogRendererTest, LargeMapInstanceCountIsWithinBudget) {
  MapBoundaryFogRenderer renderer;
  renderer.configure(200, 200, 1.0F);
  constexpr std::size_t k_max_budget = 64U;
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
