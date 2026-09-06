#include <gtest/gtest.h>
#include <vector>

#include "app/world/player_defeat_watcher.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;
constexpr int k_ally_owner = 3;
constexpr float k_past_the_poll = 0.6F;

class PlayerDefeatWatcherTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(
        k_local_owner, Game::Systems::OwnerType::Player, "Rome");
    owners.register_owner_with_id(
        k_enemy_owner, Game::Systems::OwnerType::AI, "Carthage");
    owners.register_owner_with_id(
        k_ally_owner, Game::Systems::OwnerType::AI, "Numidia");
    owners.set_local_player_id(k_local_owner);
    owners.set_owner_team(k_local_owner, 1);
    owners.set_owner_team(k_ally_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
  }

  void TearDown() override { Game::Systems::OwnerRegistry::instance().clear(); }

  static auto spawn(Engine::Core::World& world,
                    int owner_id,
                    const char* commander_name = nullptr) -> Engine::Core::EntityID {
    auto* entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->health = 100;
    if (commander_name != nullptr) {
      auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
      commander->display_name = commander_name;
    }
    return entity->get_id();
  }

  static void kill(Engine::Core::World& world, Engine::Core::EntityID id) {
    if (auto* unit = world.try_get<Engine::Core::UnitComponent>(id)) {
      unit->health = 0;
    }
  }

  std::vector<PlayerDefeatWatcher::Defeat> announced;

  auto sink() {
    return [this](const PlayerDefeatWatcher::Defeat& defeat) {
      announced.push_back(defeat);
    };
  }

  PlayerDefeatWatcher watcher;
};

TEST_F(PlayerDefeatWatcherTest, NamesTheCommanderOfTheSideThatIsWipedOut) {
  Engine::Core::World world;
  const auto local = spawn(world, k_local_owner);
  const auto enemy_commander = spawn(world, k_enemy_owner, "Hasdrubal Barca");
  const auto enemy_troop = spawn(world, k_enemy_owner);
  (void)local;

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  EXPECT_TRUE(announced.empty()) << "nobody is defeated while their army stands";

  kill(world, enemy_commander);
  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  EXPECT_TRUE(announced.empty()) << "a dead commander is not a defeated side";

  kill(world, enemy_troop);
  watcher.update(world, k_local_owner, k_past_the_poll, sink());

  ASSERT_EQ(announced.size(), 1U);
  EXPECT_EQ(announced.front().owner_id, k_enemy_owner);
  EXPECT_FALSE(announced.front().ally);
  EXPECT_EQ(announced.front().commander_name, QStringLiteral("Hasdrubal Barca"))
      << "the commander's name is only readable while they live, so it has to be "
         "remembered before the side is gone";
}

TEST_F(PlayerDefeatWatcherTest, ASideWithWavesStillToComeIsNotDefeated) {
  Engine::Core::World world;
  (void)spawn(world, k_local_owner);
  const auto enemy_troop = spawn(world, k_enemy_owner);
  bool waves_pending = true;
  const auto still_expected = [&waves_pending](int owner_id) {
    return owner_id == k_enemy_owner && waves_pending;
  };

  watcher.update(world, k_local_owner, k_past_the_poll, sink(), still_expected);
  kill(world, enemy_troop);
  watcher.update(world, k_local_owner, k_past_the_poll, sink(), still_expected);
  EXPECT_TRUE(announced.empty())
      << "a force whose next wave has not marched yet has not been beaten";

  waves_pending = false;
  watcher.update(world, k_local_owner, k_past_the_poll, sink(), still_expected);
  ASSERT_EQ(announced.size(), 1U);
  EXPECT_EQ(announced.front().owner_id, k_enemy_owner);
}

TEST_F(PlayerDefeatWatcherTest, AnAllyIsAnnouncedAsAnAlly) {
  Engine::Core::World world;
  const auto ally = spawn(world, k_ally_owner, "Masinissa");
  (void)spawn(world, k_enemy_owner);

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  kill(world, ally);
  watcher.update(world, k_local_owner, k_past_the_poll, sink());

  ASSERT_EQ(announced.size(), 1U);
  EXPECT_EQ(announced.front().owner_id, k_ally_owner);
  EXPECT_TRUE(announced.front().ally);
}

TEST_F(PlayerDefeatWatcherTest, EachSideIsAnnouncedOnlyOnce) {
  Engine::Core::World world;
  const auto enemy = spawn(world, k_enemy_owner);

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  kill(world, enemy);
  for (int tick = 0; tick < 5; ++tick) {
    watcher.update(world, k_local_owner, k_past_the_poll, sink());
  }

  EXPECT_EQ(announced.size(), 1U);
}

TEST_F(PlayerDefeatWatcherTest, TheLocalPlayerIsLeftToTheVictoryScreen) {
  Engine::Core::World world;
  const auto local = spawn(world, k_local_owner, "Scipio");
  (void)spawn(world, k_enemy_owner);

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  kill(world, local);
  watcher.update(world, k_local_owner, k_past_the_poll, sink());

  EXPECT_TRUE(announced.empty());
}

TEST_F(PlayerDefeatWatcherTest, ASideNeverSeenAliveIsNotAnnounced) {
  Engine::Core::World world;
  (void)spawn(world, k_enemy_owner);

  for (int tick = 0; tick < 4; ++tick) {
    watcher.update(world, k_local_owner, k_past_the_poll, sink());
  }

  EXPECT_TRUE(announced.empty())
      << "an owner with no units at load time has not been defeated, it simply "
         "has not spawned";
}

TEST_F(PlayerDefeatWatcherTest, TheWorldIsOnlyWalkedOnThePollInterval) {
  Engine::Core::World world;
  const auto enemy = spawn(world, k_enemy_owner);

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  kill(world, enemy);
  watcher.update(world, k_local_owner, 0.05F, sink());
  EXPECT_TRUE(announced.empty()) << "a sub-interval tick must not walk the world";

  watcher.update(world, k_local_owner, k_past_the_poll, sink());
  EXPECT_EQ(announced.size(), 1U);
}

} // namespace
