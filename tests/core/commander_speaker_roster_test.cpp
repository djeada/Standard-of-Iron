#include <gtest/gtest.h>

#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/mission/commander_speaker_roster.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"

namespace {

constexpr int k_local = 1;
constexpr int k_enemy = 2;
constexpr int k_ally = 3;
constexpr int k_headless_enemy = 4;
constexpr int k_undead = 99;
constexpr int k_unassigned = 5;
constexpr int k_neutral = 0;

class CommanderSpeakerRosterTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(
        k_neutral, Game::Systems::OwnerType::Neutral, "Neutral");
    owners.register_owner_with_id(k_local, Game::Systems::OwnerType::Player, "Player");
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "Rome");
    owners.register_owner_with_id(k_ally, Game::Systems::OwnerType::AI, "Carthage");
    owners.register_owner_with_id(
        k_headless_enemy, Game::Systems::OwnerType::AI, "Rome II");
    owners.set_local_player_id(k_local);
    owners.set_owner_team(k_local, 1);
    owners.set_owner_team(k_ally, 1);
    owners.set_owner_team(k_enemy, 2);
    owners.set_owner_team(k_headless_enemy, 2);

    m_nations.set_player_nation(k_local, Game::Systems::NationID::Carthage);
    m_nations.set_player_nation(k_enemy, Game::Systems::NationID::RomanRepublic);
    m_nations.set_player_nation(k_ally, Game::Systems::NationID::Carthage);
    m_nations.set_player_nation(k_headless_enemy,
                                Game::Systems::NationID::RomanRepublic);
  }

  Game::Systems::NationRegistry m_nations;

  void TearDown() override { Game::Systems::OwnerRegistry::instance().clear(); }

  static void spawn(Engine::Core::World& world,
                    int owner_id,
                    Game::Units::SpawnType spawn_type,
                    Game::Systems::NationID nation,
                    bool commander,
                    int health = 100) {
    auto* entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    unit->nation_id = nation;
    unit->health = health;
    if (commander) {
      entity->add_component<Engine::Core::CommanderComponent>();
    }
  }
};

TEST_F(CommanderSpeakerRosterTest, ReadsEveryNonLocalCommanderWithItsRelationship) {
  Engine::Core::World world;
  spawn(world,
        k_local,
        Game::Units::SpawnType::CarthageSwordCommander,
        Game::Systems::NationID::Carthage,
        true);
  spawn(world,
        k_enemy,
        Game::Units::SpawnType::RomanVeteranConsul,
        Game::Systems::NationID::RomanRepublic,
        true);
  spawn(world,
        k_ally,
        Game::Units::SpawnType::CarthageBowCommander,
        Game::Systems::NationID::Carthage,
        true);
  spawn(world,
        k_neutral,
        Game::Units::SpawnType::RomanLegionOrganizer,
        Game::Systems::NationID::RomanRepublic,
        true);

  spawn(world,
        k_headless_enemy,
        Game::Units::SpawnType::RomanFieldCommander,
        Game::Systems::NationID::RomanRepublic,
        true,
        0);
  spawn(world,
        k_headless_enemy,
        Game::Units::SpawnType::Spearman,
        Game::Systems::NationID::RomanRepublic,
        false);

  const auto roster = Game::Mission::build_commander_speaker_roster(
      world, Game::Systems::OwnerRegistry::instance(), m_nations, k_local);

  ASSERT_EQ(roster.size(), 3U);
  EXPECT_EQ(roster[0].owner_id, k_enemy);
  EXPECT_EQ(roster[0].troop_type, QStringLiteral("roman_veteran_consul"));
  EXPECT_EQ(roster[0].relationship, Game::Mission::CommanderRelationship::Enemy);

  EXPECT_EQ(roster[1].owner_id, k_ally);
  EXPECT_EQ(roster[1].troop_type, QStringLiteral("carthage_bow_commander"));
  EXPECT_EQ(roster[1].relationship, Game::Mission::CommanderRelationship::Ally);

  EXPECT_EQ(roster[2].owner_id, k_headless_enemy);
  EXPECT_EQ(roster[2].troop_type, QStringLiteral("roman_veteran_consul"))
      << "a Roman AI whose commander is dead falls back to Rome's default commander";
  EXPECT_EQ(roster[2].relationship, Game::Mission::CommanderRelationship::Enemy);
}

TEST_F(CommanderSpeakerRosterTest, AnAiWithNothingOnTheFieldIsStillGivenAVoice) {
  Engine::Core::World world;
  const auto roster = Game::Mission::build_commander_speaker_roster(
      world, Game::Systems::OwnerRegistry::instance(), m_nations, k_local);
  ASSERT_EQ(roster.size(), 3U);
  for (const auto& speaker : roster) {
    EXPECT_FALSE(speaker.troop_type.isEmpty()) << speaker.owner_id;
    EXPECT_NE(speaker.owner_id, k_local);
    EXPECT_NE(speaker.owner_id, k_neutral);
  }
  EXPECT_EQ(roster[1].troop_type, QStringLiteral("carthage_sword_commander"))
      << "an empty-handed AI speaks with its registered nation's default commander";
}

TEST_F(CommanderSpeakerRosterTest, AnUndeadOwnerWithNoUnitsYetStaysSilent) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(k_undead, Game::Systems::OwnerType::AI, "Sepulcher");
  owners.set_owner_team(k_undead, 3);
  m_nations.set_player_nation(k_undead, Game::Systems::NationID::IronSepulcher);

  Engine::Core::World world;
  const auto roster = Game::Mission::build_commander_speaker_roster(
      world, Game::Systems::OwnerRegistry::instance(), m_nations, k_local);
  for (const auto& speaker : roster) {
    EXPECT_NE(speaker.owner_id, k_undead)
        << "a nation without a commander must not borrow Rome's voice";
  }

  spawn(world,
        k_undead,
        Game::Units::SpawnType::SkeletonSwordsman,
        Game::Systems::NationID::IronSepulcher,
        false);
  const auto with_units = Game::Mission::build_commander_speaker_roster(
      world, Game::Systems::OwnerRegistry::instance(), m_nations, k_local);
  for (const auto& speaker : with_units) {
    EXPECT_NE(speaker.owner_id, k_undead);
  }
}

TEST_F(CommanderSpeakerRosterTest, AnAiWithNoNationAndNoUnitsIsNotGuessedAsRoman) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(k_unassigned, Game::Systems::OwnerType::AI, "Later");
  owners.set_owner_team(k_unassigned, 4);

  Engine::Core::World world;
  const auto roster = Game::Mission::build_commander_speaker_roster(
      world, Game::Systems::OwnerRegistry::instance(), m_nations, k_local);
  for (const auto& speaker : roster) {
    EXPECT_NE(speaker.owner_id, k_unassigned);
  }
}

} // namespace
