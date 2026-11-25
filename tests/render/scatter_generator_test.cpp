#include <QString>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_features.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "render/frame/terrain_scene_types.h"
#include "render/frame/world_view.h"
#include "render/ground/ground_ecology.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/scatter/scatter_budget.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/terrain_scatter_manager.h"

namespace {

using Render::GL::ScatterSpeciesId;
using Render::Ground::Scatter::ScatterGrid;
using Render::Ground::Scatter::ScatterRequest;
using WorldPropType = Game::Map::WorldProp::Type;

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::current_path();
  while (!path.empty()) {
    if (std::filesystem::exists(path / "assets" / "maps") &&
        std::filesystem::exists(path / "render" / "ground" / "scatter")) {
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

template <typename Instance>
auto same_bytes(const std::vector<Instance>& a,
                const std::vector<Instance>& b) -> bool {
  return a.size() == b.size() &&
         (a.empty() ||
          std::memcmp(a.data(), b.data(), a.size() * sizeof(Instance)) == 0);
}

auto grid_of(const Game::Map::TerrainHeightMap& height_map) -> ScatterGrid {
  return {height_map.get_width(),
          height_map.get_height(),
          height_map.get_tile_size(),
          &height_map.get_height_data(),
          &height_map.getTerrainTypes()};
}

constexpr std::array k_prop_types{WorldPropType::FireCamp,
                                  WorldPropType::Tent,
                                  WorldPropType::SupplyCart,
                                  WorldPropType::WeaponRack,
                                  WorldPropType::Ruins,
                                  WorldPropType::DeadTree,
                                  WorldPropType::Boulder,
                                  WorldPropType::PineTree,
                                  WorldPropType::OliveTree,
                                  WorldPropType::CypressTree,
                                  WorldPropType::PalmTree,
                                  WorldPropType::Plant,
                                  WorldPropType::IronOre,
                                  WorldPropType::MagicShrine,
                                  WorldPropType::AbandonedHome,
                                  WorldPropType::Statue,
                                  WorldPropType::CursedGoldVein};

class ScatterGeneratorTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_map.grid.width = 64;
    m_map.grid.height = 64;
    m_map.grid.tile_size = 1.0F;
    Game::Map::apply_ground_type_defaults(m_map.biome,
                                          Game::Map::GroundType::SoilFertile);
    m_map.biome.seed = 1234U;
    float offset = -24.0F;
    for (const auto type : k_prop_types) {
      m_map.world_props.push_back({.type = type, .x = offset, .z = 10.0F});
      offset += 3.0F;
    }
    terrain().initialize(m_map);
    ASSERT_NE(terrain().get_height_map(), nullptr);
    m_ecology = Render::Ground::shared_ground_ecology(
        &terrain(), *terrain().get_height_map(), m_map.biome.seed);
  }

  void TearDown() override { terrain().clear(); }

  static auto terrain() -> Game::Map::TerrainService& {
    return Game::Map::TerrainService::instance();
  }

  [[nodiscard]] auto
  request(const Game::Map::BiomeSettings& biome) const -> ScatterRequest {
    return {grid_of(*terrain().get_height_map()),
            m_ecology.get(),
            biome,
            terrain(),
            terrain().world_props()};
  }
  [[nodiscard]] auto request() const -> ScatterRequest { return request(m_map.biome); }

  Game::Map::MapDefinition m_map;
  std::shared_ptr<const Render::Ground::GroundEcologyField> m_ecology;
};

TEST_F(ScatterGeneratorTest, EveryGeneratorReturnsTheSameInstancesForTheSameRequest) {
  using namespace Render::Ground::Scatter;
  const auto req = request();

  const auto grass_a = generate_grass(req, 1.0F);
  const auto grass_b = generate_grass(req, 1.0F);
  EXPECT_FALSE(grass_a.instances.empty());
  EXPECT_TRUE(same_bytes(grass_a.instances, grass_b.instances));

  EXPECT_TRUE(same_bytes(generate_stones(req), generate_stones(req)));
  EXPECT_TRUE(same_bytes(generate_plants(req), generate_plants(req)));
  EXPECT_TRUE(same_bytes(generate_boulders(req), generate_boulders(req)));
  EXPECT_TRUE(
      same_bytes(generate_firecamps(req).instances, generate_firecamps(req).instances));
  for (const auto species : {Game::Map::TreeSpecies::Pine,
                             Game::Map::TreeSpecies::Olive,
                             Game::Map::TreeSpecies::Cypress,
                             Game::Map::TreeSpecies::Palm}) {
    EXPECT_TRUE(
        same_bytes(generate_trees(req, species, {}), generate_trees(req, species, {})));
  }
  for (const auto& generate : {&generate_tents,
                               &generate_supply_carts,
                               &generate_weapon_racks,
                               &generate_ruins,
                               &generate_dead_trees,
                               &generate_iron_ore,
                               &generate_magic_shrines,
                               &generate_abandoned_homes,
                               &generate_statues,
                               &generate_cursed_gold_veins}) {
    EXPECT_TRUE(same_bytes(generate(req), generate(req)));
  }
}

TEST_F(ScatterGeneratorTest, WorldPropLayersPlaceTheirPropsAndNothingElse) {
  using namespace Render::Ground::Scatter;
  const auto req = request();
  EXPECT_EQ(generate_tents(req).size(), 1U);
  EXPECT_EQ(generate_supply_carts(req).size(), 1U);
  EXPECT_EQ(generate_weapon_racks(req).size(), 1U);
  EXPECT_EQ(generate_ruins(req).size(), 1U);
  EXPECT_EQ(generate_dead_trees(req).size(), 1U);

  EXPECT_GE(generate_boulders(req).size(), 1U);
  EXPECT_GE(generate_iron_ore(req).size(), 1U);
  EXPECT_EQ(generate_magic_shrines(req).size(), 1U);
  EXPECT_EQ(generate_abandoned_homes(req).size(), 1U);
  EXPECT_EQ(generate_statues(req).size(), 1U);
  EXPECT_EQ(generate_cursed_gold_veins(req).size(), 1U);
  EXPECT_EQ(generate_firecamps(req).instances.size(), 1U);
  EXPECT_EQ(generate_firecamps(req).decor.size(), 1U);
  EXPECT_EQ(generate_trees(req, Game::Map::TreeSpecies::Olive, {}).size(), 1U);

  const std::vector<Game::Map::WorldProp> none;
  const ScatterRequest empty{req.grid, req.ecology, req.biome, req.terrain, none};
  EXPECT_TRUE(generate_tents(empty).empty());
  EXPECT_TRUE(generate_statues(empty).empty());
  EXPECT_TRUE(generate_trees(empty, Game::Map::TreeSpecies::Pine, {}).empty());
}

TEST_F(ScatterGeneratorTest, TheBiomeSeedMovesProceduralScatter) {
  using namespace Render::Ground::Scatter;
  auto reseeded = m_map.biome;
  reseeded.seed += 1U;
  EXPECT_FALSE(same_bytes(generate_grass(request(), 1.0F).instances,
                          generate_grass(request(reseeded), 1.0F).instances));
  EXPECT_FALSE(
      same_bytes(generate_plants(request()), generate_plants(request(reseeded))));
}

TEST_F(ScatterGeneratorTest, PlantDensityDrivesHowManyPlantsGrow) {
  using namespace Render::Ground::Scatter;
  const std::vector<Game::Map::WorldProp> no_props;
  auto sparse = m_map.biome;
  sparse.plant_density = 0.2F;
  auto dense = m_map.biome;
  dense.plant_density = 1.6F;
  auto bare = m_map.biome;
  bare.plant_density = 0.0F;
  const auto with = [&](const Game::Map::BiomeSettings& biome) {
    return ScatterRequest{grid_of(*terrain().get_height_map()),
                          m_ecology.get(),
                          biome,
                          terrain(),
                          no_props};
  };
  EXPECT_TRUE(generate_plants(with(bare)).empty())
      << "no plant density and no authored plants means no plants";
  EXPECT_GT(generate_plants(with(dense)).size(), generate_plants(with(sparse)).size());
}

TEST_F(ScatterGeneratorTest, GrassThinsWithTheGraphicsProfileDensity) {
  using namespace Render::Ground::Scatter;
  const auto full = generate_grass(request(), 1.0F);
  const auto low = generate_grass(request(), 0.3F);
  EXPECT_FLOAT_EQ(full.generated_density, 1.0F);
  EXPECT_FLOAT_EQ(low.generated_density, 0.3F);
  EXPECT_LT(low.instances.size(), full.instances.size());
  EXPECT_GT(full.typical_blade_height, 0.0F);
}

TEST(ScatterGeneratorExclusion, StonesAndPlantsStayOffAMountain) {
  using namespace Render::Ground::Scatter;
  Game::Map::TerrainHeightMap height_map(64, 64, 1.0F);
  height_map.build_from_features({Game::Map::TerrainFeature{
      .type = Game::Map::TerrainType::Mountain,
      .center_x = 0.0F,
      .center_z = 0.0F,
      .radius = 14.0F,
      .height = 9.0F,
  }});
  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome, Game::Map::GroundType::SoilFertile);
  biome.seed = 77U;
  const Game::Map::TerrainService no_terrain;
  const std::vector<Game::Map::WorldProp> no_props;
  const ScatterRequest req{grid_of(height_map), nullptr, biome, no_terrain, no_props};

  constexpr float k_core = 9.0F;
  const auto off_core = [](float x, float z) {
    return std::hypot(x, z) > k_core;
  };

  const auto stones = generate_stones(req);
  const auto plants = generate_plants(req);
  for (const auto& stone : stones) {
    EXPECT_TRUE(off_core(stone.pos_scale.x(), stone.pos_scale.z()));
  }
  for (const auto& plant : plants) {
    EXPECT_TRUE(off_core(plant.pos_scale.x(), plant.pos_scale.z()));
  }
}

TEST(ScatterBudget, ApplyKeepsTheFirstInstancesAndReportsTheCut) {
  std::vector<int> instances{1, 2, 3, 4, 5};
  EXPECT_FALSE(Render::Ground::Scatter::apply_instance_budget(instances, 5U));
  EXPECT_EQ(instances.size(), 5U);
  EXPECT_TRUE(Render::Ground::Scatter::apply_instance_budget(instances, 3U));
  EXPECT_EQ(instances, (std::vector<int>{1, 2, 3}));
}

TEST(ScatterBudget, BudgetsAreCeilingsScaledByMapSizeAndGrassDensity) {
  using Render::Ground::Scatter::k_min_layer_budget;
  using Render::Ground::Scatter::scatter_instance_budget;
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Grass, 0U, 1.0F), 4'400'000U);
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Grass, 0U, 0.65F), 2'860'000U);
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Grass, 0U, 0.3F), 1'320'000U);
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Grass, 0U, 0.0F), 440'000U)
      << "grass never budgets below a tenth of full density, like its generator";
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Stone, 400U * 400U, 1.0F),
            160'000U);
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Plant, 400U * 400U, 0.3F),
            640'000U);
  EXPECT_EQ(scatter_instance_budget(ScatterSpeciesId::Tent, 10U, 1.0F),
            k_min_layer_budget);
}

TEST_F(ScatterGeneratorTest, ALayerCutToItsBudgetSaysSo) {
  Render::GL::PlantRenderer plants;
  plants.set_world_view(Render::WorldView::of(Game::Session::SessionContext::active()));
  plants.configure(*terrain().get_height_map(), m_map.biome, m_map.world_props);
  const std::size_t full = plants.instance_count();
  ASSERT_GT(full, 3U);
  EXPECT_FALSE(plants.budget_hit());

  plants.set_instance_budget(3U);
  plants.configure(*terrain().get_height_map(), m_map.biome, m_map.world_props);
  EXPECT_EQ(plants.instance_count(), 3U);
  EXPECT_TRUE(plants.budget_hit());
}

// The budgets are ceilings: on every shipped map each layer stays under its
// budget, so the cut never changes what a real map draws.
TEST(ScatterBudget, NoShippedMapReachesAScatterBudget) {
  const auto maps = find_repo_root() / "assets" / "maps";
  ASSERT_TRUE(std::filesystem::exists(maps));
  std::size_t checked = 0;
  for (const auto& entry : std::filesystem::directory_iterator(maps)) {
    if (entry.path().extension() != ".json") {
      continue;
    }
    Game::Map::MapDefinition map_def;
    QString error;
    if (!Game::Map::MapLoader::load_from_json_file(
            QString::fromStdString(entry.path().string()), map_def, &error)) {
      continue;
    }
    auto& terrain = Game::Map::TerrainService::instance();
    terrain.initialize(map_def);
    const auto* height_map = terrain.get_height_map();
    ASSERT_NE(height_map, nullptr) << entry.path();

    Render::GL::TerrainScatterManager scatter;
    scatter.set_world_view(
        Render::WorldView::of(Game::Session::SessionContext::active()));
    scatter.configure(*height_map,
                      terrain.biome_settings(),
                      terrain.authored_world_props(),
                      terrain.world_props());
    for (const auto& layer : scatter.memory()) {
      EXPECT_FALSE(layer.budget_hit)
          << entry.path().filename() << ": "
          << Render::GL::scatter_species_name(layer.species) << " made "
          << layer.instances << " of a budget of " << layer.budget;
      EXPECT_GT(layer.budget, 0U);
    }
    terrain.clear();
    ++checked;
  }
  EXPECT_GT(checked, 10U) << "the shipped maps were not found or did not load";
}

} // namespace
