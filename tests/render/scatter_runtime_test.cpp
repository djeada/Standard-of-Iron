#include "game/map/terrain.h"
#include "game/map/visibility_service.h"
#include "render/decoration_gpu.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/boulder_renderer.h"
#include "render/ground/dead_tree_renderer.h"
#include "render/ground/ground_utils.h"
#include "render/ground/scatter_renderer_state.h"
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

TEST(ScatterRuntimeTest, DirectUploadDecisionRequiresDirtyOrMissingBuffer) {
  using Render::Ground::Scatter::direct_needs_buffer_upload;

  EXPECT_FALSE(direct_needs_buffer_upload(true, false, true))
      << "Empty scatter chunks should not upload in stable or dirty frames.";
  EXPECT_TRUE(direct_needs_buffer_upload(false, false, false))
      << "A populated chunk without a GPU buffer needs one initial upload.";
  EXPECT_TRUE(direct_needs_buffer_upload(false, true, true))
      << "Dirty populated chunks need an upload.";
  EXPECT_FALSE(direct_needs_buffer_upload(false, true, false))
      << "Stable populated chunks with a buffer must not upload.";
}

TEST(ScatterRuntimeTest,
     FilteredVisibilityDecisionIgnoresStableFramesWithCachedBuffer) {
  using Render::Ground::Scatter::filtered_needs_visibility_rebuild;

  EXPECT_FALSE(filtered_needs_visibility_rebuild(true, true, false, true, 2, 1))
      << "Empty scatter chunks should not rebuild visibility.";
  EXPECT_TRUE(filtered_needs_visibility_rebuild(false, true, false, true, 1, 1))
      << "Dirty filtered chunks need a visibility pass.";
  EXPECT_TRUE(
      filtered_needs_visibility_rebuild(false, false, true, false, 2, 1))
      << "Visibility version changes need a new filtered instance list.";
  EXPECT_TRUE(
      filtered_needs_visibility_rebuild(false, false, false, false, 1, 1))
      << "A visible cached list without a buffer needs re-upload.";
  EXPECT_FALSE(
      filtered_needs_visibility_rebuild(false, false, true, false, 1, 1))
      << "Stable filtered chunks with cached visibility and buffer do no work.";
}

TEST(ScatterRuntimeTest, DryGrassColorKeepsContrastOnMediterraneanSoil) {
  QVector3D const soil{0.52F, 0.44F, 0.32F};
  QVector3D const yellow_blade{0.78F, 0.72F, 0.45F};

  QVector3D const adjusted = Render::Ground::contrast_grass_blade_color(
      yellow_blade, soil, Game::Map::GroundType::GrassDry, 0.85F);

  EXPECT_LT(Render::Ground::color_luminance(adjusted),
            Render::Ground::color_luminance(soil) - 0.10F);
  EXPECT_LT(adjusted.y(), yellow_blade.y());
}

TEST(ScatterRuntimeTest, NonDryGrassColorIsUnchangedByContrastAdjustment) {
  QVector3D const soil{0.28F, 0.24F, 0.18F};
  QVector3D const blade{0.30F, 0.60F, 0.28F};

  QVector3D const adjusted = Render::Ground::contrast_grass_blade_color(
      blade, soil, Game::Map::GroundType::SoilFertile, 0.85F);

  EXPECT_FLOAT_EQ(adjusted.x(), blade.x());
  EXPECT_FLOAT_EQ(adjusted.y(), blade.y());
  EXPECT_FLOAT_EQ(adjusted.z(), blade.z());
}

TEST(ScatterRuntimeTest, DirectGpuReadyAllowsEmptyScatterBuffers) {
  std::vector<Render::GL::StoneInstanceGpu> instances;
  std::unique_ptr<Render::GL::Buffer> buffer;

  EXPECT_TRUE(Render::Ground::Scatter::is_direct_gpu_ready(instances, buffer));

  instances.push_back(
      {QVector4D(0.0F, 0.0F, 0.0F, 1.0F), QVector4D(1.0F, 1.0F, 1.0F, 0.0F)});
  EXPECT_FALSE(Render::Ground::Scatter::is_direct_gpu_ready(instances, buffer));
}

TEST(ScatterRuntimeTest, DirectRendererStateResetClearsCpuTrackingState) {
  Render::Ground::Scatter::DirectRendererState<Render::GL::StoneInstanceGpu,
                                               Render::GL::StoneBatchParams>
      state;
  state.instances.push_back(
      {QVector4D(0.0F, 0.0F, 0.0F, 1.0F), QVector4D(1.0F, 1.0F, 1.0F, 0.0F)});
  state.instance_count = 1;
  state.instances_dirty = true;

  state.reset_instances();

  EXPECT_TRUE(state.instances.empty());
  EXPECT_EQ(state.instance_count, 0U);
  EXPECT_FALSE(state.instances_dirty);
  EXPECT_TRUE(state.is_gpu_ready());
  EXPECT_FALSE(state.last_sync_stats.did_upload_or_rebuild());
}

TEST(ScatterRuntimeTest, FilteredRendererStateResetClearsVisibilityState) {
  Render::Ground::Scatter::FilteredRendererState<Render::GL::PlantInstanceGpu,
                                                 Render::GL::PlantBatchParams>
      state;
  state.instances.push_back({QVector4D(0.0F, 0.0F, 0.0F, 1.0F),
                             QVector4D(1.0F, 1.0F, 1.0F, 0.0F),
                             QVector4D(0.0F, 0.0F, 0.0F, 0.0F)});
  state.visible_instances = state.instances;
  state.instance_count = 1;
  state.instances_dirty = true;
  state.cached_visibility_version = 7;
  state.visibility_dirty = false;

  state.reset_instances();

  EXPECT_TRUE(state.instances.empty());
  EXPECT_TRUE(state.visible_instances.empty());
  EXPECT_EQ(state.instance_count, 0U);
  EXPECT_FALSE(state.instances_dirty);
  EXPECT_EQ(state.cached_visibility_version, 0U);
  EXPECT_TRUE(state.visibility_dirty);
  EXPECT_TRUE(state.is_gpu_ready());
  EXPECT_FALSE(state.last_sync_stats.did_upload_or_rebuild());
}

TEST(ScatterRuntimeTest, SyncStatsAggregateUploadAndRebuildCounters) {
  Render::Ground::Scatter::SyncStats stats;
  stats.visibility_rebuilds = 1;
  stats.buffer_uploads = 2;

  Render::Ground::Scatter::SyncStats more;
  more.buffer_resets = 3;
  stats += more;

  EXPECT_TRUE(stats.did_upload_or_rebuild());
  EXPECT_EQ(stats.visibility_rebuilds, 1U);
  EXPECT_EQ(stats.buffer_uploads, 2U);
  EXPECT_EQ(stats.buffer_resets, 3U);
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

TEST(ScatterRuntimeTest, LargeRockyMapsGetProceduralBouldersAndLogs) {
  Game::Map::TerrainHeightMap height_map(96, 96, 1.0F);
  Game::Map::BiomeSettings biome_settings;
  Game::Map::apply_ground_type_defaults(biome_settings,
                                        Game::Map::GroundType::SoilRocky);
  biome_settings.seed = 4242U;
  biome_settings.rock_exposure = 0.85F;
  biome_settings.moisture_level = 0.20F;
  biome_settings.plant_density = 0.35F;

  std::vector<Game::Map::WorldProp> no_authored_props;

  Render::GL::BoulderRenderer boulders;
  boulders.configure(height_map, biome_settings, no_authored_props);
  EXPECT_GT(boulders.instance_count(), 0U);

  Render::GL::DeadTreeRenderer dead_trees;
  dead_trees.configure(height_map, biome_settings, no_authored_props);
  EXPECT_GT(dead_trees.instance_count(), 0U);
}

} // namespace
