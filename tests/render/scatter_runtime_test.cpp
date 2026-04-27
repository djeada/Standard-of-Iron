#include "game/map/terrain.h"
#include "game/map/visibility_service.h"
#include "render/decoration_gpu.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/scatter_runtime.h"
#include <gtest/gtest.h>

namespace {

auto make_snapshot() -> Game::Map::VisibilityService::Snapshot {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 4;
  snapshot.height = 4;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 1.5F;
  snapshot.half_height = 1.5F;
  snapshot.cells.assign(
      16, static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  snapshot.cells[10] =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);
  snapshot.cells[5] =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored);
  return snapshot;
}

TEST(ScatterRuntimeTest, CollectVisibleInstancesFiltersBySnapshot) {
  std::vector<Render::GL::PlantInstanceGpu> instances(3);
  instances[0].pos_scale = QVector4D(0.0F, 0.0F, 0.0F, 1.0F);
  instances[1].pos_scale = QVector4D(-1.0F, 0.0F, -1.0F, 1.0F);
  instances[2].pos_scale = QVector4D(5.0F, 0.0F, 5.0F, 1.0F);

  const auto visible = Render::Ground::Scatter::collect_visible_instances(
      instances, make_snapshot(),
      [](const Render::GL::PlantInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_scale;
      });

  ASSERT_EQ(visible.size(), 1U);
  EXPECT_FLOAT_EQ(visible.front().pos_scale.x(), 0.0F);
  EXPECT_FLOAT_EQ(visible.front().pos_scale.z(), 0.0F);
}

TEST(ScatterRuntimeTest, FilteredGpuReadyReflectsVisibilityState) {
  std::vector<Render::GL::PlantInstanceGpu> instances(1);
  std::vector<Render::GL::PlantInstanceGpu> visible_instances;
  std::unique_ptr<Render::GL::Buffer> buffer;

  EXPECT_TRUE(Render::Ground::Scatter::is_filtered_gpu_ready(
      instances, visible_instances, buffer, false));
  EXPECT_FALSE(Render::Ground::Scatter::is_filtered_gpu_ready(
      instances, visible_instances, buffer, true));
}

TEST(ScatterRuntimeTest, DirectGpuReadyAllowsEmptyScatterBuffers) {
  std::vector<Render::GL::StoneInstanceGpu> instances;
  std::unique_ptr<Render::GL::Buffer> buffer;

  EXPECT_TRUE(Render::Ground::Scatter::is_direct_gpu_ready(instances, buffer));

  instances.push_back(
      {QVector4D(0.0F, 0.0F, 0.0F, 1.0F), QVector4D(1.0F, 1.0F, 1.0F, 0.0F)});
  EXPECT_FALSE(Render::Ground::Scatter::is_direct_gpu_ready(instances, buffer));
}

TEST(ScatterRuntimeTest,
     BiomeRendererConfiguresLargeTerrainWithoutReallocationCrash) {
  Game::Map::TerrainHeightMap height_map(160, 160, 1.0F);
  Game::Map::BiomeSettings biome_settings;

  Render::GL::BiomeRenderer renderer;
  renderer.configure(height_map, biome_settings);

  EXPECT_GT(renderer.instance_count(), 0U);

  renderer.configure(height_map, biome_settings);
  EXPECT_GT(renderer.instance_count(), 0U);
}

} // namespace
