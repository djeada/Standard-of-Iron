#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "render/decoration_gpu.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/boulder_renderer.h"
#include "render/ground/dead_tree_renderer.h"
#include "render/ground/ground_utils.h"
#include "render/ground/iron_ore_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/scatter_renderer_state.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_scatter_manager.h"

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::current_path();
  while (!path.empty()) {
    if (std::filesystem::exists(path / "assets" / "maps" /
                                "map_campania_campaign.json") &&
        std::filesystem::exists(path / "render" / "ground" /
                                "terrain_scatter_manager.cpp")) {
      return path;
    }
    const auto parent = path.parent_path();
    if (parent == path) {
      break;
    }
    path = parent;
  }
  return std::filesystem::current_path();
}

auto make_snapshot() -> Game::Map::VisibilityService::Snapshot {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 4;
  snapshot.height = 4;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 1.5F;
  snapshot.half_height = 1.5F;
  snapshot.cells.assign(16,
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  snapshot.cells[10] = static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);
  snapshot.cells[5] = static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored);
  return snapshot;
}

auto make_tree_map_definition(Game::Map::GroundType ground_type,
                              std::uint32_t seed) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 72;
  map_def.grid.height = 72;
  map_def.grid.tile_size = 1.0F;
  Game::Map::apply_ground_type_defaults(map_def.biome, ground_type);
  map_def.biome.seed = seed;
  map_def.biome.ground_irregularity_enabled = true;
  map_def.biome.irregularity_amplitude =
      std::max(0.65F, map_def.biome.irregularity_amplitude);
  return map_def;
}

TEST(ScatterRuntimeTest, CollectVisibleInstancesFiltersBySnapshot) {
  std::vector<Render::GL::PlantInstanceGpu> instances(3);
  instances[0].pos_scale = QVector4D(0.0F, 0.0F, 0.0F, 1.0F);
  instances[1].pos_scale = QVector4D(-1.0F, 0.0F, -1.0F, 1.0F);
  instances[2].pos_scale = QVector4D(5.0F, 0.0F, 5.0F, 1.0F);

  const auto visible = Render::Ground::Scatter::collect_visible_instances(
      instances,
      make_snapshot(),
      [](const Render::GL::PlantInstanceGpu& instance) -> const QVector4D& {
        return instance.pos_scale;
      });

  ASSERT_EQ(visible.size(), 1U);
  EXPECT_FLOAT_EQ(visible.front().pos_scale.x(), 0.0F);
  EXPECT_FLOAT_EQ(visible.front().pos_scale.z(), 0.0F);
}

TEST(ScatterRuntimeTest, FilteredGpuReadyReflectsVisibilityState) {
  std::vector<Render::GL::PlantInstanceGpu> const instances(1);
  std::vector<Render::GL::PlantInstanceGpu> const visible_instances;
  std::unique_ptr<Render::GL::Buffer> const buffer;

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
  EXPECT_TRUE(filtered_needs_visibility_rebuild(false, false, true, false, 2, 1))
      << "Visibility version changes need a new filtered instance list.";
  EXPECT_TRUE(filtered_needs_visibility_rebuild(false, false, false, false, 1, 1))
      << "A visible cached list without a buffer needs re-upload.";
  EXPECT_FALSE(filtered_needs_visibility_rebuild(false, false, true, false, 1, 1))
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
  std::unique_ptr<Render::GL::Buffer> const buffer;

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

TEST(ScatterRuntimeTest, BiomeRendererConfiguresLargeTerrainWithoutReallocationCrash) {
  Game::Map::TerrainHeightMap const height_map(160, 160, 1.0F);
  Game::Map::BiomeSettings const biome_settings;

  Render::GL::BiomeRenderer renderer;
  renderer.configure(height_map, biome_settings);

  EXPECT_GT(renderer.instance_count(), 0U);

  renderer.configure(height_map, biome_settings);
  EXPECT_GT(renderer.instance_count(), 0U);
}

TEST(ScatterRuntimeTest, LargeRockyMapsGetProceduralBouldersAndLogs) {
  Game::Map::TerrainHeightMap const height_map(96, 96, 1.0F);
  Game::Map::BiomeSettings biome_settings;
  Game::Map::apply_ground_type_defaults(biome_settings,
                                        Game::Map::GroundType::SoilRocky);
  biome_settings.seed = 4242U;
  biome_settings.rock_exposure = 0.85F;
  biome_settings.moisture_level = 0.20F;
  biome_settings.plant_density = 0.35F;

  std::vector<Game::Map::WorldProp> const no_authored_props;

  Render::GL::BoulderRenderer boulders;
  boulders.configure(height_map, biome_settings, no_authored_props, no_authored_props);
  EXPECT_GT(boulders.instance_count(), 0U);

  Render::GL::DeadTreeRenderer dead_trees;
  dead_trees.configure(height_map, biome_settings, no_authored_props);
  EXPECT_GT(dead_trees.instance_count(), 0U);
}

TEST(ScatterRuntimeTest, CampaniaCampaignMaintainsRichNaturalScatter) {
  const auto root = find_repo_root();
  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromStdString(
          (root / "assets" / "maps" / "map_campania_campaign.json").string()),
      map_def,
      &error))
      << error.toStdString();

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  Render::GL::PlantRenderer plants;
  plants.configure(*height_map, map_def.biome, map_def.world_props);
  Render::GL::StoneRenderer stones;
  stones.configure(*height_map, map_def.biome, map_def.world_props);
  Render::GL::BoulderRenderer boulders;
  boulders.configure(
      *height_map, map_def.biome, map_def.world_props, map_def.world_props);

  EXPECT_GE(plants.instance_count(), 2500U);
  EXPECT_LE(plants.instance_count(), 7000U);
  EXPECT_GE(stones.instance_count(), 140U);
  EXPECT_LE(stones.instance_count(), 900U);
  EXPECT_GE(boulders.instance_count(), 14U);
  EXPECT_LE(boulders.instance_count(), 120U);

  terrain.clear();
}

TEST(ScatterRuntimeTest, RuntimePropRefreshDoesNotRescatterPlantsOrStones) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 4242U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 20.0F, .z = 20.0F});
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 24.0F, .z = 24.0F});
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::IronOre, .x = 28.0F, .z = 28.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  Render::GL::TerrainScatterManager scatter;
  scatter.configure(*height_map,
                    terrain.biome_settings(),
                    terrain.authored_world_props(),
                    terrain.world_props());

  const std::size_t plant_count_before = scatter.plant()->instance_count();
  const std::size_t stone_count_before = scatter.stone()->instance_count();
  const std::size_t pine_count_before = scatter.pine()->instance_count();
  const std::size_t boulder_count_before = scatter.boulder()->instance_count();
  const std::size_t iron_ore_count_before = scatter.iron_ore()->instance_count();

  ASSERT_GT(pine_count_before, 0U);
  ASSERT_GT(boulder_count_before, 0U);
  ASSERT_GT(iron_ore_count_before, 0U);

  std::uint64_t harvested_tree_id = 0;
  std::uint64_t harvested_boulder_id = 0;
  std::uint64_t harvested_iron_ore_id = 0;
  for (const auto& prop : terrain.world_props()) {
    if (harvested_tree_id == 0 && prop.type == Game::Map::WorldProp::Type::PineTree) {
      harvested_tree_id = prop.id;
    } else if (harvested_boulder_id == 0 &&
               prop.type == Game::Map::WorldProp::Type::Boulder) {
      harvested_boulder_id = prop.id;
    } else if (harvested_iron_ore_id == 0 &&
               prop.type == Game::Map::WorldProp::Type::IronOre) {
      harvested_iron_ore_id = prop.id;
    }
  }

  ASSERT_NE(harvested_tree_id, 0U);
  ASSERT_NE(harvested_boulder_id, 0U);
  ASSERT_NE(harvested_iron_ore_id, 0U);

  EXPECT_TRUE(terrain.harvest_world_prop(harvested_tree_id));
  EXPECT_TRUE(terrain.harvest_world_prop(harvested_boulder_id));
  EXPECT_TRUE(terrain.harvest_world_prop(harvested_iron_ore_id));

  scatter.refresh_runtime_world_props(terrain.world_props());

  EXPECT_EQ(scatter.plant()->instance_count(), plant_count_before);
  EXPECT_EQ(scatter.stone()->instance_count(), stone_count_before);
  EXPECT_LT(scatter.pine()->instance_count(), pine_count_before);
  EXPECT_LT(scatter.boulder()->instance_count(), boulder_count_before);
  EXPECT_LT(scatter.iron_ore()->instance_count(), iron_ore_count_before);

  terrain.clear();
}

TEST(ScatterRuntimeTest, ProceduralPinesUseResolvedSurfaceHeightAndReducedScaleRange) {
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(make_tree_map_definition(Game::Map::GroundType::ForestMud, 1337U));
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  Render::GL::PineRenderer renderer;
  renderer.configure(*height_map,
                     terrain.biome_settings(),
                     terrain.authored_world_props(),
                     terrain.world_props());

  ASSERT_GT(renderer.instance_count(), 0U);

  auto const scatter_rules =
      Game::Map::make_scatter_rules(terrain.biome_settings().ground_type);
  for (auto const& instance : renderer.instances_for_test()) {
    EXPECT_NEAR(instance.pos_scale.y(),
                terrain.resolve_surface_world_y(
                    instance.pos_scale.x(), instance.pos_scale.z(), 0.0F, 0.0F),
                0.001F);
    EXPECT_LE(instance.pos_scale.w(),
              scatter_rules.pine_scale_max * height_map->get_tile_size() * 1.18F +
                  0.001F);
  }

  terrain.clear();
}

TEST(ScatterRuntimeTest, ProceduralOlivesUseResolvedSurfaceHeightAndReducedScaleRange) {
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(make_tree_map_definition(Game::Map::GroundType::GrassDry, 4242U));
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  Render::GL::OliveRenderer renderer;
  renderer.configure(*height_map,
                     terrain.biome_settings(),
                     terrain.authored_world_props(),
                     terrain.world_props());

  ASSERT_GT(renderer.instance_count(), 0U);

  auto const scatter_rules =
      Game::Map::make_scatter_rules(terrain.biome_settings().ground_type);
  for (auto const& instance : renderer.instances_for_test()) {
    EXPECT_NEAR(instance.pos_scale.y(),
                terrain.resolve_surface_world_y(
                    instance.pos_scale.x(), instance.pos_scale.z(), 0.0F, 0.0F),
                0.001F);
    EXPECT_LE(instance.pos_scale.w(),
              scatter_rules.olive_scale_max * height_map->get_tile_size() * 1.22F +
                  0.001F);
  }

  terrain.clear();
}

} // namespace
