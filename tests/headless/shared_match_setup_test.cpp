#include <QString>

#include <cstdint>
#include <gtest/gtest.h>

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/match_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/world_digest.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"

namespace {

constexpr const char* k_map = "assets/maps/map_forest.json";
constexpr int k_local_owner = 1;

struct SetupOutcome {
  bool ok = false;
  QString map_name;
  int grid_width = 0;
  int grid_height = 0;
  std::size_t entity_count = 0;
  std::uint64_t digest = 0;
};

auto set_up_match(std::uint64_t seed, int local_owner) -> SetupOutcome {
  Game::Session::SessionContext session(
      Game::Session::SessionContext::Config{.rng_seed = seed});
  const Game::Session::ScopedSession scope(session);

  Game::Systems::initialize_default_content(session.nations());
  session.nations().clear_player_assignments();
  session.owners().set_local_player_id(local_owner);

  Game::Map::MapTransformer::set_local_owner_id(local_owner);
  Game::Map::MapTransformer::set_spectator_mode(false);
  Game::Map::MapTransformer::setPlayerTeamOverrides({});
  Game::Map::MapTransformer::set_base_assignments({});

  const auto loaded =
      Game::Map::load_match(QString::fromLatin1(k_map), session.world());

  SetupOutcome outcome;
  outcome.ok = loaded.ok;
  if (!loaded.ok) {
    return outcome;
  }

  Game::Systems::NavGrid::initialize(loaded.grid_width, loaded.grid_height);

  outcome.map_name = loaded.map_name;
  outcome.grid_width = loaded.grid_width;
  outcome.grid_height = loaded.grid_height;
  outcome.entity_count =
      session.world().collect_entities_with<Engine::Core::UnitComponent>().size();
  outcome.digest = Game::Session::world_digest(session.world());
  return outcome;
}

class SharedMatchSetupTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

} // namespace

TEST_F(SharedMatchSetupTest, TheSharedLoaderBuildsARealMap) {
  const auto outcome = set_up_match(7, k_local_owner);

  ASSERT_TRUE(outcome.ok) << "the shared match loader could not read " << k_map;
  EXPECT_FALSE(outcome.map_name.isEmpty());
  EXPECT_GT(outcome.grid_width, 0);
  EXPECT_GT(outcome.grid_height, 0);
  EXPECT_GT(outcome.entity_count, 0U);
}

TEST_F(SharedMatchSetupTest, TheSameMapAndSeedProduceTheSameInitialState) {
  const auto first = set_up_match(7, k_local_owner);
  const auto second = set_up_match(7, k_local_owner);

  ASSERT_TRUE(first.ok);
  ASSERT_TRUE(second.ok);
  EXPECT_EQ(first.grid_width, second.grid_width);
  EXPECT_EQ(first.grid_height, second.grid_height);
  EXPECT_EQ(first.entity_count, second.entity_count);
  EXPECT_EQ(first.digest, second.digest)
      << "both hosts must reach the same authoritative state from the same map, "
         "player configuration and seed";
}
