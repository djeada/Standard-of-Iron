#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/decoration_gpu.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/boulder_renderer.h"
#include "render/ground/dead_tree_renderer.h"
#include "render/ground/iron_ore_renderer.h"
#include "render/ground/magic_shrine_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/scatter_renderer_state.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/world_view.h"

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

TEST(ScatterRuntimeTest, DryGrassColorKeepsContrastOnMediterraneanSoil) {
  QVector3D const soil{0.52F, 0.44F, 0.32F};
  QVector3D const yellow_blade{0.78F, 0.72F, 0.45F};

  QVector3D const adjusted = Render::Ground::contrast_grass_blade_color(
      yellow_blade, soil, Game::Map::GroundType::GrassDry, 0.85F);

  EXPECT_LT(Render::Ground::color_luminance(adjusted),
            Render::Ground::color_luminance(soil) - 0.055F);
  EXPECT_LT(adjusted.y(), yellow_blade.y());
}

TEST(ScatterRuntimeTest, GreenGrassBladesSitDeeperThanTheGroundTheyGrowFrom) {
  QVector3D const soil{0.28F, 0.24F, 0.18F};
  QVector3D const blade{0.30F, 0.60F, 0.28F};

  QVector3D const adjusted = Render::Ground::contrast_grass_blade_color(
      blade, soil, Game::Map::GroundType::SoilFertile, 0.85F);

  EXPECT_LT(Render::Ground::color_luminance(adjusted),
            Render::Ground::color_luminance(blade));

  auto green_excess = [](const QVector3D& color) {
    return color.y() - std::max(color.x(), color.z());
  };
  EXPECT_GT(green_excess(adjusted) /
                std::max(Render::Ground::color_luminance(adjusted), 1e-4F),
            green_excess(blade) /
                std::max(Render::Ground::color_luminance(blade), 1e-4F));
}

TEST(ScatterRuntimeTest, GrassBladeToningDiffersBetweenBiomes) {
  QVector3D const soil{0.28F, 0.24F, 0.18F};
  QVector3D const blade{0.30F, 0.60F, 0.28F};

  QVector3D const fertile = Render::Ground::contrast_grass_blade_color(
      blade, soil, Game::Map::GroundType::SoilFertile, 0.5F);
  QVector3D const alpine = Render::Ground::contrast_grass_blade_color(
      blade, soil, Game::Map::GroundType::AlpineMix, 0.5F);
  QVector3D const dry = Render::Ground::contrast_grass_blade_color(
      blade, soil, Game::Map::GroundType::GrassDry, 0.5F);

  EXPECT_GT((fertile - alpine).length(), 0.02F);
  EXPECT_GT((fertile - dry).length(), 0.02F);
  EXPECT_GT((alpine - dry).length(), 0.02F);
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
                                                 Render::GL::FoliageBatchParams>
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
  renderer.set_world_view(Render::WorldView::of_active_session());
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
  boulders.set_world_view(Render::WorldView::of_active_session());
  boulders.configure(height_map, biome_settings, no_authored_props, no_authored_props);
  EXPECT_GT(boulders.instance_count(), 0U);

  Render::GL::DeadTreeRenderer dead_trees;
  dead_trees.set_world_view(Render::WorldView::of_active_session());
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
  plants.set_world_view(Render::WorldView::of_active_session());
  plants.configure(*height_map, map_def.biome, map_def.world_props);
  Render::GL::StoneRenderer stones;
  stones.set_world_view(Render::WorldView::of_active_session());
  stones.configure(*height_map, map_def.biome, map_def.world_props);
  Render::GL::BoulderRenderer boulders;
  boulders.set_world_view(Render::WorldView::of_active_session());
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
  scatter.set_world_view(Render::WorldView::of_active_session());
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
  renderer.set_world_view(Render::WorldView::of_active_session());
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
  renderer.set_world_view(Render::WorldView::of_active_session());
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

TEST(ScatterRuntimeTest, RuntimePlantedShrineReachesTheScatterPass) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 48;
  map_def.grid.height = 48;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 909U;

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  Render::GL::TerrainScatterManager scatter;
  scatter.set_world_view(Render::WorldView::of_active_session());
  scatter.configure(*height_map,
                    terrain.biome_settings(),
                    terrain.authored_world_props(),
                    terrain.world_props());
  ASSERT_EQ(scatter.magic_shrine()->instance_count(), 0U);

  Game::Map::WorldProp shrine;
  shrine.type = Game::Map::WorldProp::Type::MagicShrine;
  ASSERT_NE(terrain.add_world_prop_at_world(shrine, 4.0F, -3.0F), 0U);

  scatter.refresh_runtime_world_props(terrain.world_props());

  EXPECT_EQ(scatter.magic_shrine()->instance_count(), 1U)
      << "an undead zone that plants its shrine mid-run must still be drawn";

  terrain.clear();
}

TEST(ScatterRuntimeTest, WorldPropRefreshReusesTheProceduralBiomeScatter) {
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(make_tree_map_definition(Game::Map::GroundType::ForestMud, 8081U));
  auto const* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  const auto& seed_props = terrain.authored_world_props();
  auto runtime_props = terrain.world_props();

  Render::GL::PineRenderer pines;
  pines.set_world_view(Render::WorldView::of_active_session());
  Render::GL::BoulderRenderer boulders;
  boulders.set_world_view(Render::WorldView::of_active_session());
  Render::GL::DeadTreeRenderer dead_trees;
  dead_trees.set_world_view(Render::WorldView::of_active_session());
  pines.configure(*height_map, terrain.biome_settings(), seed_props, runtime_props);
  boulders.configure(*height_map, terrain.biome_settings(), seed_props, runtime_props);
  dead_trees.configure(
      *height_map, terrain.biome_settings(), seed_props, runtime_props);

  ASSERT_GT(pines.instance_count(), 0U)
      << "forest mud must scatter pines procedurally for this test to mean anything";
  ASSERT_EQ(pines.procedural_generations_for_test(), 1U);
  ASSERT_EQ(boulders.procedural_generations_for_test(), 1U);
  ASSERT_EQ(dead_trees.procedural_generations_for_test(), 1U);

  const std::size_t pine_count = pines.instance_count();
  const std::size_t boulder_count = boulders.instance_count();
  const std::size_t dead_tree_count = dead_trees.instance_count();

  Game::Map::WorldProp shrine;
  shrine.type = Game::Map::WorldProp::Type::MagicShrine;
  shrine.x = 12.0F;
  shrine.z = 9.0F;
  runtime_props.push_back(shrine);

  pines.refresh_world_props(runtime_props, false);
  boulders.refresh_world_props(runtime_props, false);
  dead_trees.refresh_world_props(runtime_props);

  EXPECT_EQ(pines.procedural_generations_for_test(), 1U)
      << "planting a prop must not re-run the map-wide pine scatter";
  EXPECT_EQ(boulders.procedural_generations_for_test(), 1U);
  EXPECT_EQ(dead_trees.procedural_generations_for_test(), 1U);

  EXPECT_EQ(pines.instance_count(), pine_count);
  EXPECT_EQ(boulders.instance_count(), boulder_count);
  EXPECT_EQ(dead_trees.instance_count(), dead_tree_count)
      << "an unrelated prop must not shift the biome scatter around it";

  pines.configure(*height_map, terrain.biome_settings(), seed_props, runtime_props);
  EXPECT_EQ(pines.procedural_generations_for_test(), 1U)
      << "a full reconfigure must drop the cache and generate exactly once";
  EXPECT_EQ(pines.instance_count(), pine_count);

  terrain.clear();
}

struct FakeScatterInstance {
  QVector4D pos_scale;
};

using FakeChunk = Render::Ground::Scatter::SpatialChunk<FakeScatterInstance>;

auto fake_position(const FakeScatterInstance& instance) -> const QVector4D& {
  return instance.pos_scale;
}

auto make_grid_instances(int side, float spacing) -> std::vector<FakeScatterInstance> {
  std::vector<FakeScatterInstance> instances;
  instances.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
  for (int z = 0; z < side; ++z) {
    for (int x = 0; x < side; ++x) {
      instances.push_back({QVector4D(static_cast<float>(x) * spacing,
                                     0.0F,
                                     static_cast<float>(z) * spacing,
                                     1.0F)});
    }
  }
  return instances;
}

TEST(ScatterRuntimeTest, SpatialPartitionGivesEveryChunkAContiguousInstanceSlice) {
  auto instances = make_grid_instances(24, 4.0F);
  const std::size_t total = instances.size();
  std::vector<FakeChunk> chunks;

  Render::Ground::Scatter::rebuild_spatial_partition(instances, chunks, fake_position);

  ASSERT_GT(chunks.size(), 1U) << "a 92-unit grid must span several 24-unit chunks";
  EXPECT_EQ(instances.size(), total) << "partitioning reorders, it must not drop";

  std::size_t expected_first = 0;
  std::size_t counted = 0;
  for (const auto& chunk : chunks) {
    EXPECT_EQ(chunk.first, expected_first)
        << "chunk slices must tile the instance vector without gaps";
    EXPECT_GT(chunk.count, 0U);
    EXPECT_EQ(chunk.accepted.size(), chunk.count);
    EXPECT_EQ(chunk.visible_count, 0U);
    EXPECT_FALSE(chunk.all_accepted);

    for (std::size_t i = 0; i < chunk.count; ++i) {
      const auto& position = instances[chunk.first + i].pos_scale;
      EXPECT_LE((position.toVector3D() - chunk.center).length(), chunk.radius)
          << "the chunk bounding sphere must contain every instance it owns";
    }
    expected_first += chunk.count;
    counted += chunk.count;
  }
  EXPECT_EQ(counted, total);
}

TEST(ScatterRuntimeTest, UnchangedVisibilityLeavesChunkAcceptanceAlone) {
  auto instances = make_grid_instances(12, 4.0F);
  std::vector<FakeChunk> chunks;
  Render::Ground::Scatter::rebuild_spatial_partition(instances, chunks, fake_position);
  ASSERT_FALSE(chunks.empty());

  std::vector<std::uint8_t> flags;
  std::vector<FakeScatterInstance> packed;
  auto reveal_west = [](float world_x, float) {
    return world_x < 24.0F;
  };

  bool any_changed_first_pass = false;
  for (auto& chunk : chunks) {
    any_changed_first_pass |= Render::Ground::Scatter::refresh_chunk_acceptance(
        instances, chunk, fake_position, reveal_west, flags, packed);
  }
  EXPECT_TRUE(any_changed_first_pass)
      << "the first scan must always report a change so the buffers get filled";

  for (auto& chunk : chunks) {
    EXPECT_FALSE(Render::Ground::Scatter::refresh_chunk_acceptance(
        instances, chunk, fake_position, reveal_west, flags, packed))
        << "re-scanning against the same visibility must not request an upload";
  }
}

TEST(ScatterRuntimeTest, RevealingGroundOnlyDirtiesTheChunksItTouches) {
  auto instances = make_grid_instances(12, 6.0F);
  std::vector<FakeChunk> chunks;
  Render::Ground::Scatter::rebuild_spatial_partition(instances, chunks, fake_position);
  ASSERT_GT(chunks.size(), 2U);

  std::vector<std::uint8_t> flags;
  std::vector<FakeScatterInstance> packed;
  auto reveal_west = [](float world_x, float) {
    return world_x < 24.0F;
  };
  auto reveal_more = [](float world_x, float) {
    return world_x < 48.0F;
  };

  for (auto& chunk : chunks) {
    Render::Ground::Scatter::refresh_chunk_acceptance(
        instances, chunk, fake_position, reveal_west, flags, packed);
  }

  std::size_t changed_chunks = 0;
  for (auto& chunk : chunks) {
    if (Render::Ground::Scatter::refresh_chunk_acceptance(
            instances, chunk, fake_position, reveal_more, flags, packed)) {
      ++changed_chunks;
    }
  }

  EXPECT_GT(changed_chunks, 0U) << "widening the reveal must expose more scatter";
  EXPECT_LT(changed_chunks, chunks.size())
      << "chunks outside the newly revealed band must not re-upload";

  std::size_t visible_total = 0;
  for (const auto& chunk : chunks) {
    visible_total += chunk.visible_count;
    if (chunk.center.x() < 24.0F) {
      EXPECT_TRUE(chunk.all_accepted);
    }
  }
  EXPECT_GT(visible_total, 0U);
  EXPECT_LT(visible_total, instances.size());
}

} // namespace
