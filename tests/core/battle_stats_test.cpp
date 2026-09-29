#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "app/core/entity_cache.h"
#include "app/world/battle_stats.h"
#include "game/core/component_gameplay.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::UnitDiedEvent;
using Engine::Core::UnitSpawnedEvent;
using Game::Units::SpawnType;

constexpr int k_local = 1;
constexpr int k_enemy = 2;

class BattleStatsTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session.owners().register_owner_with_id(
        k_enemy, Game::Systems::OwnerType::AI, "Rival");
  }

  Game::Session::SessionContext m_session;
  App::World::BattleStats m_stats;
};

TEST_F(BattleStatsTest, OnlyEnemyTroopsKilledByTheLocalPlayerCount) {
  auto& world = m_session.world();
  const auto foot = SpawnType::Archer;

  EXPECT_TRUE(m_stats.note_unit_died(
      UnitDiedEvent(11, k_enemy, foot, 5, k_local), &world, k_local));
  EXPECT_GE(m_stats.enemy_troops_defeated(), 1);
  EXPECT_EQ(m_stats.enemy_units_defeated(), 1);
  const int troops = m_stats.enemy_troops_defeated();

  EXPECT_FALSE(m_stats.note_unit_died(
      UnitDiedEvent(12, k_local, foot, 6, k_enemy), &world, k_local))
      << "losses are not kills";
  EXPECT_FALSE(m_stats.note_unit_died(
      UnitDiedEvent(13, k_enemy, foot, 6, k_enemy), &world, k_local))
      << "kills by someone else are not the player's";
  EXPECT_FALSE(m_stats.note_unit_died(
      UnitDiedEvent(14, k_enemy, SpawnType::Barracks, 5, k_local), &world, k_local))
      << "structures are not troops";
  EXPECT_EQ(m_stats.enemy_troops_defeated(), troops);
  EXPECT_EQ(m_stats.enemy_units_defeated(), 1);
}

TEST_F(BattleStatsTest, ResetReportsWhetherTheVisibleCounterChanged) {
  EXPECT_FALSE(m_stats.reset()) << "nothing to clear";
  (void)m_stats.note_unit_died(
      UnitDiedEvent(11, k_enemy, SpawnType::Archer, 5, k_local),
      &m_session.world(),
      k_local);
  EXPECT_TRUE(m_stats.reset());
  EXPECT_EQ(m_stats.enemy_troops_defeated(), 0);
  EXPECT_EQ(m_stats.enemy_units_defeated(), 0);
}

TEST_F(BattleStatsTest, TheCountersSurviveASaveRoundTripAndNewerVersionsAreIgnored) {
  (void)m_stats.note_unit_died(
      UnitDiedEvent(11, k_enemy, SpawnType::Archer, 5, k_local),
      &m_session.world(),
      k_local);
  const int troops = m_stats.enemy_troops_defeated();
  const QJsonObject saved = m_stats.serialize(&m_session);
  EXPECT_EQ(saved.value("version").toInt(), 1);
  EXPECT_EQ(saved.value("enemy_units_defeated").toInt(), 1);

  App::World::BattleStats restored;
  EXPECT_TRUE(restored.restore(saved, &m_session));
  EXPECT_EQ(restored.enemy_troops_defeated(), troops);
  EXPECT_EQ(restored.enemy_units_defeated(), 1);
  EXPECT_FALSE(restored.restore(saved, &m_session))
      << "an unchanged counter is not news";

  QJsonObject future = saved;
  future["version"] = 2;
  future["enemy_units_defeated"] = 99;
  EXPECT_FALSE(restored.restore(future, &m_session));
  EXPECT_EQ(restored.enemy_units_defeated(), 1);
  EXPECT_FALSE(restored.restore(QJsonObject{}, &m_session));
}

TEST_F(BattleStatsTest, PlayerStatsAreZeroForAnUnknownOwner) {
  const QVariantMap stats = App::World::BattleStats::player_stats(m_session, 99);
  EXPECT_EQ(stats.value("troopsRecruited").toInt(), 0);
  EXPECT_FALSE(stats.value("gameEnded").toBool());
}

TEST(BattleStatsDefeatTextTest, TheAnnouncementNamesTheCommanderWhenThereIsOne) {
  EXPECT_EQ(App::World::format_defeat_announcement(
                {.owner_id = 2, .ally = false, .owner_name = "Rome"}),
            QStringLiteral("Rome has been defeated."));
  EXPECT_EQ(App::World::format_defeat_announcement(
                {.owner_id = 2, .ally = true, .owner_name = "Carthage"}),
            QStringLiteral("Our ally Carthage has been defeated."));
  EXPECT_EQ(App::World::format_defeat_announcement({.owner_id = 2,
                                                    .ally = true,
                                                    .owner_name = "Carthage",
                                                    .commander_name = "Hanno"}),
            QStringLiteral("Our ally Carthage is finished - Hanno has fallen."));
  EXPECT_EQ(App::World::format_defeat_announcement({.owner_id = 2,
                                                    .ally = false,
                                                    .owner_name = "Rome",
                                                    .commander_name = "Scipio"}),
            QStringLiteral("Rome is finished - Scipio has fallen."));
}

class EntityCacheEventsTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "Rival");
  }

  Game::Systems::OwnerRegistry m_owners;
  EntityCache m_cache;
};

TEST_F(EntityCacheEventsTest, LocalSpawnsAndDeathsMoveTheTroopTotalAndNeverBelowZero) {
  m_cache.apply_spawn(
      UnitSpawnedEvent(1, k_local, SpawnType::Archer), k_local, m_owners);
  const int after_spawn = m_cache.player_troop_count;
  EXPECT_GT(after_spawn, 0);

  m_cache.apply_death(UnitDiedEvent(1, k_local, SpawnType::Archer), k_local, m_owners);
  EXPECT_EQ(m_cache.player_troop_count, 0);
  m_cache.apply_death(UnitDiedEvent(2, k_local, SpawnType::Archer), k_local, m_owners);
  EXPECT_EQ(m_cache.player_troop_count, 0) << "the total is clamped at zero";
}

TEST_F(EntityCacheEventsTest, BarracksAreTrackedForTheLocalPlayerAndAiOpponents) {
  m_cache.apply_spawn(
      UnitSpawnedEvent(1, k_local, SpawnType::Barracks), k_local, m_owners);
  EXPECT_TRUE(m_cache.player_barracks_alive);
  EXPECT_EQ(m_cache.player_troop_count, 0) << "a barracks is not a troop";
  m_cache.apply_death(
      UnitDiedEvent(1, k_local, SpawnType::Barracks), k_local, m_owners);
  EXPECT_FALSE(m_cache.player_barracks_alive);

  m_cache.apply_spawn(
      UnitSpawnedEvent(2, k_enemy, SpawnType::Barracks), k_local, m_owners);
  m_cache.apply_spawn(
      UnitSpawnedEvent(3, k_enemy, SpawnType::Barracks), k_local, m_owners);
  EXPECT_EQ(m_cache.enemy_barracks_count, 2);
  EXPECT_TRUE(m_cache.enemy_barracks_alive);
  m_cache.apply_death(
      UnitDiedEvent(2, k_enemy, SpawnType::Barracks), k_local, m_owners);
  EXPECT_TRUE(m_cache.enemy_barracks_alive);
  m_cache.apply_death(
      UnitDiedEvent(3, k_enemy, SpawnType::Barracks), k_local, m_owners);
  EXPECT_FALSE(m_cache.enemy_barracks_alive);
  m_cache.apply_death(
      UnitDiedEvent(4, k_enemy, SpawnType::Barracks), k_local, m_owners);
  EXPECT_EQ(m_cache.enemy_barracks_count, 0);
}

} // namespace
