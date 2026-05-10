#include "app/core/mission_definition_view.h"

#include <gtest/gtest.h>

TEST(MissionDefinitionViewTest, ResolvesFallbackPlayerCommanderForCarthage) {
  Game::Mission::MissionDefinition mission;
  mission.id = "campania_campaign";
  mission.title = "Campania Campaign";
  mission.player_setup.nation = "carthage";
  mission.player_setup.faction = "carthaginian";
  mission.player_setup.color = "brown";

  const QVariantMap view_model = build_mission_definition_map(mission);
  const QVariantMap player_setup = view_model.value("player_setup").toMap();
  const QVariantMap commander = player_setup.value("commander").toMap();

  EXPECT_EQ(player_setup.value("commander_troop").toString(),
            QStringLiteral("carthage_elephant_master"));
  EXPECT_EQ(commander.value("display_name").toString(),
            QStringLiteral("Hannibal Barca"));
  EXPECT_EQ(
      commander.value("battlefield_role").toString(),
      QStringLiteral("Elite infantry commander with sacred-band bodyguard "
                     "and iconic command standard."));
}

TEST(MissionDefinitionViewTest, IncludesEveryEnemySetupAndCommanderDetails) {
  Game::Mission::MissionDefinition mission;
  mission.player_setup.nation = "roman_republic";
  mission.player_setup.faction = "roman";
  mission.player_setup.color = "red";

  Game::Mission::AISetup northern_force;
  northern_force.id = "north";
  northern_force.nation = "roman_republic";
  northern_force.faction = "roman";
  northern_force.color = "red";
  northern_force.difficulty = "hard";
  northern_force.commander_troop = QStringLiteral("roman_legion_organizer");

  Game::Mission::AISetup southern_force;
  southern_force.id = "south";
  southern_force.nation = "roman_republic";
  southern_force.faction = "roman";
  southern_force.color = "orange";
  southern_force.difficulty = "hard";
  southern_force.commander_troop = QStringLiteral("roman_veteran_consul");

  mission.ai_setups = {northern_force, southern_force};

  const QVariantMap view_model = build_mission_definition_map(mission);
  const QVariantList ai_setups = view_model.value("ai_setups").toList();

  ASSERT_EQ(ai_setups.size(), 2);

  const QVariantMap first_force = ai_setups[0].toMap();
  const QVariantMap second_force = ai_setups[1].toMap();

  EXPECT_EQ(first_force.value("id").toString(), QStringLiteral("north"));
  EXPECT_EQ(
      first_force.value("commander").toMap().value("display_name").toString(),
      QStringLiteral("Quintus Fabius Maximus"));

  EXPECT_EQ(second_force.value("id").toString(), QStringLiteral("south"));
  EXPECT_EQ(
      second_force.value("commander").toMap().value("display_name").toString(),
      QStringLiteral("Publius Cornelius Scipio"));
}
