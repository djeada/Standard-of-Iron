#include <QJsonArray>

#include <memory>

#include <gtest/gtest.h>

#include "game/core/component_core.h"
#include "game/core/event_manager.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Session::ScopedSession;
using Game::Session::SessionContext;
using Game::Systems::NationID;
using Game::Systems::OwnerType;
using Game::Units::SpawnType;

constexpr int k_rome = 1;
constexpr int k_carthage = 2;
constexpr int k_tombs = 99;

class BattleStatsTest : public ::testing::Test {
protected:
  void SetUp() override {
    scope = std::make_unique<ScopedSession>(session);
    Game::Systems::initialize_default_content(session.nations());

    auto& owners = session.owners();
    owners.register_owner_with_id(k_rome, OwnerType::Player, "Scipio");
    owners.register_owner_with_id(k_carthage, OwnerType::AI, "Hasdrubal");
    owners.register_owner_with_id(k_tombs, OwnerType::AI, "Iron Sepulcher");
    owners.set_owner_team(k_rome, 1);
    owners.set_owner_team(k_carthage, 2);
    owners.set_owner_team(k_tombs, 9);
    session.nations().set_player_nation(k_rome, NationID::RomanRepublic);
    session.nations().set_player_nation(k_carthage, NationID::Carthage);
    session.nations().set_player_nation(k_tombs, NationID::IronSepulcher);

    for (const int owner : {k_rome, k_carthage, k_tombs}) {
      session.stats().mark_game_start(owner);
    }
  }

  void TearDown() override { scope.reset(); }

  auto squad(int owner, SpawnType type = SpawnType::Swordsman) -> Engine::Core::EntityID {
    auto* entity = session.world().create_entity();
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 1.0F);
    unit->owner_id = owner;
    unit->spawn_type = type;
    unit->nation_id = session.nations().get_nation_for_player(owner) != nullptr
                          ? session.nations().get_nation_for_player(owner)->id
                          : NationID::RomanRepublic;
    return entity->get_id();
  }

  auto men(Engine::Core::EntityID id) -> int {
    return Game::Systems::squad_men(
        *session.world().try_get<Engine::Core::UnitComponent>(id));
  }

  void die(Engine::Core::EntityID id,
           int owner,
           int killer_owner,
           SpawnType type = SpawnType::Swordsman,
           bool disbanded = false) {
    session.stats().on_unit_died(Engine::Core::UnitDiedEvent(
        id, owner, type, killer_owner != 0 ? 1U : 0U, killer_owner, disbanded));
  }

  auto stats(int owner) -> const Game::Systems::PlayerStats& {
    const auto* found = session.stats().get_stats(owner);
    EXPECT_NE(found, nullptr);
    static const Game::Systems::PlayerStats none;
    return found != nullptr ? *found : none;
  }

  SessionContext session;
  std::unique_ptr<ScopedSession> scope;
};

TEST_F(BattleStatsTest, EveryFigureCountsSoldiersNotSquads) {
  const auto legion = squad(k_rome);
  const int full = men(legion);
  ASSERT_GT(full, 1) << "a swordsman squad is more than one man";

  session.stats().on_unit_recruited(
      Engine::Core::UnitRecruitedEvent(legion, k_rome, SpawnType::Swordsman));
  EXPECT_EQ(stats(k_rome).troops_recruited, full);

  die(legion, k_rome, k_carthage);
  EXPECT_EQ(stats(k_rome).losses, full);
  EXPECT_EQ(stats(k_carthage).enemies_killed, full)
      << "the killer is credited with the same men the victim lost";
}

TEST_F(BattleStatsTest, TrainedCountsOnlySoldiersABarracksRecruited) {
  const auto legion = squad(k_rome);
  const int full = men(legion);

  session.stats().on_unit_recruited(
      Engine::Core::UnitRecruitedEvent(legion, k_rome, SpawnType::Swordsman));
  session.stats().on_unit_recruited(Engine::Core::UnitRecruitedEvent(
      squad(k_rome, SpawnType::Builder), k_rome, SpawnType::Builder));
  session.stats().on_unit_recruited(Engine::Core::UnitRecruitedEvent(
      squad(k_rome, SpawnType::Civilian), k_rome, SpawnType::Civilian));
  EXPECT_EQ(stats(k_rome).troops_recruited, full)
      << "builder crews and civilians are not soldiers trained";

  // A squad split, scripted reinforcement or awakened undead spawns a unit
  // with is_initial_spawn = false but is not a recruit.
  session.stats().on_unit_spawned(
      Engine::Core::UnitSpawnedEvent(squad(k_rome), k_rome, SpawnType::Swordsman, false));
  session.stats().on_unit_spawned(Engine::Core::UnitSpawnedEvent(
      squad(k_rome, SpawnType::Builder), k_rome, SpawnType::Builder, false));
  EXPECT_EQ(stats(k_rome).troops_recruited, full)
      << "a split or a spawn that no barracks trained must not inflate Trained";
}

TEST_F(BattleStatsTest, ADisbandedArmyIsNotALoss) {
  const auto legion = squad(k_rome);
  const auto commander = squad(k_rome, SpawnType::RomanFieldCommander);

  die(commander, k_rome, k_carthage, SpawnType::RomanFieldCommander);
  EXPECT_EQ(stats(k_rome).losses, 1);
  EXPECT_EQ(stats(k_carthage).enemies_killed, 1);

  die(legion, k_rome, 0, SpawnType::Swordsman, true);
  EXPECT_EQ(stats(k_rome).losses, 1)
      << "the collapse that follows a defeat disbands the troops; nobody fell";
  EXPECT_EQ(stats(k_carthage).enemies_killed, 1)
      << "nobody is credited with a disbanding";
}

TEST_F(BattleStatsTest, TalliesStopWhenTheMatchEnds) {
  const auto legion = squad(k_rome);
  const auto spare = squad(k_rome);
  const auto commander = squad(k_rome, SpawnType::RomanFieldCommander);

  die(commander, k_rome, k_carthage, SpawnType::RomanFieldCommander);
  session.stats().mark_game_end(k_rome);
  session.stats().mark_game_end(k_carthage);
  const int losses_at_the_end = stats(k_rome).losses;
  const int kills_at_the_end = stats(k_carthage).enemies_killed;

  die(legion, k_rome, k_carthage);
  die(spare, k_rome, k_carthage);
  session.stats().on_unit_spawned(
      Engine::Core::UnitSpawnedEvent(legion, k_rome, SpawnType::Barracks, false));
  session.stats().on_unit_recruited(
      Engine::Core::UnitRecruitedEvent(legion, k_rome, SpawnType::Swordsman));
  EXPECT_EQ(stats(k_rome).troops_recruited, 0);
  EXPECT_EQ(stats(k_rome).losses, losses_at_the_end)
      << "the log line and the report must read the same numbers";
  EXPECT_EQ(stats(k_carthage).enemies_killed, kills_at_the_end);
  EXPECT_EQ(stats(k_rome).barracks_owned, 0);
}

TEST_F(BattleStatsTest, TheDeadOfTheWildAreCreditedToWhoKilledThem) {
  const auto legion = squad(k_rome);
  const int full = men(legion);
  const auto second = squad(k_carthage);

  die(legion, k_rome, k_tombs);
  EXPECT_EQ(stats(k_rome).losses, full);
  EXPECT_EQ(stats(k_rome).losses_to_wild, full)
      << "a squad the Iron Sepulcher killed is a loss to the wild";
  EXPECT_EQ(stats(k_tombs).enemies_killed, full)
      << "the tombs hold the kill; it is not lost";

  die(second, k_carthage, Game::Core::NEUTRAL_OWNER_ID);
  EXPECT_EQ(stats(k_carthage).losses_to_wild, men(second))
      << "wildlife and other neutrals count as wild too";
  ASSERT_NE(session.stats().get_stats(Game::Core::NEUTRAL_OWNER_ID), nullptr);
  EXPECT_EQ(stats(Game::Core::NEUTRAL_OWNER_ID).enemies_killed, men(second));
}

TEST_F(BattleStatsTest, AKillByARivalArmyIsNotAWildLoss) {
  const auto legion = squad(k_rome);
  die(legion, k_rome, k_carthage);
  EXPECT_EQ(stats(k_rome).losses_to_wild, 0);
}

TEST_F(BattleStatsTest, FriendlyFireEarnsNoKill) {
  session.owners().set_owner_team(k_carthage, 1);
  const auto legion = squad(k_rome);
  die(legion, k_rome, k_carthage);
  EXPECT_EQ(stats(k_carthage).enemies_killed, 0);
}

TEST_F(BattleStatsTest, WildLossesSurviveASaveRoundTrip) {
  const auto legion = squad(k_rome);
  die(legion, k_rome, k_tombs);
  const int wild = stats(k_rome).losses_to_wild;
  ASSERT_GT(wild, 0);

  const QJsonArray counters = session.stats().serialize_counters();
  session.stats().clear();
  session.stats().mark_game_start(k_rome);
  session.stats().restore_counters(counters);
  EXPECT_EQ(stats(k_rome).losses_to_wild, wild);
}

} // namespace
