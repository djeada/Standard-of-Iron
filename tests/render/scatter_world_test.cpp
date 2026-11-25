#include <QVector3D>

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/scatter/scatter_world.h"
#include "game/map/scatter/world_prop_clearance_index.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "render/frame/world_view.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/boulder_renderer.h"
#include "render/ground/iron_ore_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter/scatter_request.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/tree_renderer.h"

namespace {

using Game::Map::ScatterWorld;
using Game::Map::TerrainService;
using Game::Map::WorldProp;
using Game::Systems::BuildingCollisionRegistry;
using Render::Ground::Scatter::ScatterGrid;
using Render::Ground::Scatter::ScatterRequest;

template <typename Instance>
auto same_bytes(const std::vector<Instance>& a,
                const std::vector<Instance>& b) -> bool {
  return a.size() == b.size() &&
         (a.empty() ||
          std::memcmp(a.data(), b.data(), a.size() * sizeof(Instance)) == 0);
}

class ScatterWorldTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 64;
    map_def.grid.height = 64;
    map_def.grid.tile_size = 1.0F;
    map_def.biome.seed = 4242U;
    Game::Map::apply_ground_type_defaults(map_def.biome,
                                          Game::Map::GroundType::SoilRocky);
    map_def.world_props.push_back(
        {.type = WorldProp::Type::PineTree, .x = 20.0F, .z = 20.0F});
    map_def.world_props.push_back(
        {.type = WorldProp::Type::Boulder, .x = 24.0F, .z = 24.0F});
    map_def.world_props.push_back(
        {.type = WorldProp::Type::IronOre, .x = 28.0F, .z = 28.0F});
    map_def.roads.push_back({.start = QVector3D(-20.0F, 0.0F, -6.0F),
                             .end = QVector3D(20.0F, 0.0F, -6.0F),
                             .width = 3.0F});
    terrain().initialize(map_def);
    BuildingCollisionRegistry::instance().register_building(
        9001U, "barracks", -8.0F, 10.0F, 1);
  }

  void TearDown() override {
    BuildingCollisionRegistry::instance().unregister_building(9001U);
    terrain().clear();
  }

  static auto terrain() -> TerrainService& { return TerrainService::instance(); }

  static auto grid() -> ScatterGrid {
    const auto* height_map = terrain().get_height_map();
    return {height_map->get_width(),
            height_map->get_height(),
            height_map->get_tile_size(),
            &height_map->get_height_data(),
            &height_map->getTerrainTypes()};
  }

  static void configure(Render::GL::TerrainScatterManager& scatter) {
    scatter.set_world_view(
        Render::WorldView::of(Game::Session::SessionContext::active()));
    scatter.configure(*terrain().get_height_map(),
                      terrain().biome_settings(),
                      terrain().authored_world_props(),
                      terrain().world_props());
  }
};

TEST_F(ScatterWorldTest, AGeneratorFedASnapshotMatchesTheLiveServices) {
  const auto world = ScatterWorld::capture_live();
  const auto& biome = terrain().biome_settings();
  const auto& props = terrain().world_props();
  const ScatterRequest live{grid(), nullptr, biome, terrain(), props};
  const ScatterRequest frozen{
      grid(), nullptr, biome, world->terrain(), props, world.get()};
  using namespace Render::Ground::Scatter;

  EXPECT_TRUE(same_bytes(generate_stones(live), generate_stones(frozen)));
  EXPECT_TRUE(same_bytes(generate_plants(live), generate_plants(frozen)));
  EXPECT_TRUE(same_bytes(generate_grass(live, 1.0F).instances,
                         generate_grass(frozen, 1.0F).instances));
  EXPECT_TRUE(same_bytes(generate_boulders(live), generate_boulders(frozen)));
  EXPECT_TRUE(same_bytes(generate_iron_ore(live), generate_iron_ore(frozen)));
  EXPECT_TRUE(same_bytes(generate_trees(live, Game::Map::TreeSpecies::Pine, {}),
                         generate_trees(frozen, Game::Map::TreeSpecies::Pine, {})));
  EXPECT_FALSE(generate_grass(frozen, 1.0F).instances.empty());
}

TEST_F(ScatterWorldTest, ASnapshotDoesNotSeeLaterChangesToTheLiveMap) {
  const auto world = ScatterWorld::capture_live();
  const auto& biome = terrain().biome_settings();
  const std::vector<WorldProp> props = terrain().world_props();
  const ScatterRequest frozen{
      grid(), nullptr, biome, world->terrain(), props, world.get()};
  const auto stones_before = Render::Ground::Scatter::generate_stones(frozen);
  const auto grass_before =
      Render::Ground::Scatter::generate_grass(frozen, 1.0F).instances;
  const std::size_t props_before = world->terrain().world_props().size();

  BuildingCollisionRegistry::instance().register_building(9002U, "home", 6.0F, 6.0F, 1);
  WorldProp boulder;
  boulder.type = WorldProp::Type::Boulder;
  ASSERT_NE(terrain().add_world_prop_at_world(boulder, -4.0F, 4.0F), 0U);

  EXPECT_EQ(world->terrain().world_props().size(), props_before);
  EXPECT_FALSE(world->buildings().is_point_in_building(6.0F, 6.0F))
      << "a building registered after the capture leaked into the snapshot";
  EXPECT_TRUE(world->buildings().is_point_in_building(-8.0F, 10.0F));
  EXPECT_TRUE(
      same_bytes(Render::Ground::Scatter::generate_stones(frozen), stones_before));
  EXPECT_TRUE(same_bytes(
      Render::Ground::Scatter::generate_grass(frozen, 1.0F).instances, grass_before));

  BuildingCollisionRegistry::instance().unregister_building(9002U);
}

TEST_F(ScatterWorldTest, AWorkerRefreshAdoptsTheSameInstancesAsASynchronousOne) {
  Render::GL::TerrainScatterManager sync;
  Render::GL::TerrainScatterManager async;
  configure(sync);
  configure(async);

  for (const auto& prop : std::vector<WorldProp>(terrain().world_props())) {
    if (prop.type == WorldProp::Type::PineTree ||
        prop.type == WorldProp::Type::Boulder) {
      EXPECT_TRUE(terrain().harvest_world_prop(prop.id));
    }
  }

  sync.refresh_runtime_world_props(terrain().world_props());
  async.schedule_runtime_world_props_refresh(terrain().world_props(),
                                             ScatterWorld::capture_live());
  async.finish_scheduled_refreshes();
  EXPECT_FALSE(async.has_scheduled_refreshes());

  const auto pine = Game::Map::TreeSpecies::Pine;
  EXPECT_TRUE(same_bytes(sync.tree(pine)->instances(), async.tree(pine)->instances()));
  EXPECT_TRUE(same_bytes(sync.boulder()->instances(), async.boulder()->instances()));
  EXPECT_TRUE(same_bytes(sync.iron_ore()->instances(), async.iron_ore()->instances()));
  EXPECT_TRUE(same_bytes(sync.stone()->instances(), async.stone()->instances()));
  EXPECT_TRUE(same_bytes(sync.plant()->instances(), async.plant()->instances()));
}

TEST_F(ScatterWorldTest, ALayerKeepsItsInstancesUntilTheRefreshIsAdopted) {
  Render::GL::TerrainScatterManager scatter;
  configure(scatter);
  const auto pine = Game::Map::TreeSpecies::Pine;
  const auto trees_before = scatter.tree(pine)->instances();
  const auto boulders_before = scatter.boulder()->instances();
  ASSERT_FALSE(trees_before.empty());
  ASSERT_FALSE(boulders_before.empty());

  for (const auto& prop : std::vector<WorldProp>(terrain().world_props())) {
    if (prop.type == WorldProp::Type::PineTree ||
        prop.type == WorldProp::Type::Boulder) {
      EXPECT_TRUE(terrain().harvest_world_prop(prop.id));
    }
  }
  scatter.schedule_runtime_world_props_refresh(terrain().world_props(),
                                               ScatterWorld::capture_live());

  EXPECT_TRUE(same_bytes(scatter.tree(pine)->instances(), trees_before));
  EXPECT_TRUE(same_bytes(scatter.boulder()->instances(), boulders_before));

  scatter.finish_scheduled_refreshes();
  EXPECT_LT(scatter.tree(pine)->instances().size(), trees_before.size());
  EXPECT_LT(scatter.boulder()->instances().size(), boulders_before.size());
}

TEST_F(ScatterWorldTest, ReconfiguringDropsARefreshStillInFlight) {
  Render::GL::TerrainScatterManager scatter;
  configure(scatter);
  const std::size_t boulders_before = scatter.boulder()->instances().size();
  ASSERT_GT(boulders_before, 0U);

  scatter.schedule_runtime_world_props_refresh(terrain().world_props(),
                                               ScatterWorld::capture_live());

  for (const auto& prop : std::vector<WorldProp>(terrain().world_props())) {
    if (prop.type == WorldProp::Type::Boulder) {
      EXPECT_TRUE(terrain().harvest_world_prop(prop.id));
    }
  }
  configure(scatter);
  const std::size_t boulders_after_configure = scatter.boulder()->instances().size();
  ASSERT_LT(boulders_after_configure, boulders_before);

  scatter.finish_scheduled_refreshes();
  EXPECT_FALSE(scatter.has_scheduled_refreshes());
  EXPECT_EQ(scatter.boulder()->instances().size(), boulders_after_configure)
      << "a refresh planned for the old map brought the harvested boulder back";
}

} // namespace
