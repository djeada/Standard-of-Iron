#include <QDir>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "app/core/mission_definition_view.h"
#include "game/map/campaign_loader.h"

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
            QStringLiteral("carthage_sword_commander"));
  EXPECT_EQ(commander.value("display_name").toString(),
            QStringLiteral("Hannibal Barca"));
  EXPECT_EQ(commander.value("battlefield_role").toString(),
            QStringLiteral("Elite sword commander with an iconic standard and "
                           "sacred-band armor."));
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
  EXPECT_EQ(first_force.value("commander").toMap().value("display_name").toString(),
            QStringLiteral("Quintus Fabius Maximus"));

  EXPECT_EQ(second_force.value("id").toString(), QStringLiteral("south"));
  EXPECT_EQ(second_force.value("commander").toMap().value("display_name").toString(),
            QStringLiteral("Publius Cornelius Scipio"));
}

TEST(MissionDefinitionViewTest, ExposesWoodInStartingResources) {
  Game::Mission::MissionDefinition mission;
  mission.player_setup.starting_resources.set(Game::Systems::ResourceType::Gold, 100);
  mission.player_setup.starting_resources.set(Game::Systems::ResourceType::Food, 80);
  mission.player_setup.starting_resources.set(Game::Systems::ResourceType::Wood, 45);

  const QVariantMap view_model = build_mission_definition_map(mission);
  const QVariantMap resources =
      view_model.value("player_setup").toMap().value("starting_resources").toMap();

  EXPECT_EQ(resources.value("gold").toInt(), 100);
  EXPECT_EQ(resources.value("food").toInt(), 80);
  EXPECT_EQ(resources.value("wood").toInt(), 45);
}

// Issue #1079: the briefing a player is shown is built from the mission file,
// so every shipped campaign mission has to produce a complete one. A mission
// with no objectives is a mission that cannot be won or explained.
namespace {

auto campaign_mission_ids() -> QStringList {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/campaigns/second_punic_war.json"))) {
      break;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  EXPECT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      dir.filePath(QStringLiteral("assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();

  QStringList ids;
  for (const auto& mission : campaign.missions) {
    ids.append(mission.mission_id);
  }
  return ids;
}

} // namespace

TEST(MissionObjectivesTest, EveryCampaignMissionBriefsThePlayer) {
  const QStringList ids = campaign_mission_ids();
  ASSERT_FALSE(ids.isEmpty());

  for (const QString& mission_id : ids) {
    const QVariantMap definition = load_mission_definition_map(mission_id);
    ASSERT_FALSE(definition.isEmpty()) << mission_id.toStdString() << " did not load";

    EXPECT_FALSE(definition.value(QStringLiteral("title")).toString().isEmpty())
        << mission_id.toStdString() << " has no title";
    EXPECT_FALSE(definition.value(QStringLiteral("summary")).toString().isEmpty())
        << mission_id.toStdString() << " has no summary";

    const QVariantList victory =
        definition.value(QStringLiteral("victory_conditions")).toList();
    const QVariantList defeat =
        definition.value(QStringLiteral("defeat_conditions")).toList();
    EXPECT_FALSE(victory.isEmpty())
        << mission_id.toStdString() << " cannot be won: no victory condition";
    EXPECT_FALSE(defeat.isEmpty())
        << mission_id.toStdString() << " cannot be lost: no defeat condition";

    for (const QVariant& entry : victory + defeat) {
      const QVariantMap condition = entry.toMap();
      EXPECT_FALSE(condition.value(QStringLiteral("type")).toString().isEmpty())
          << mission_id.toStdString() << " has an untyped condition";
      EXPECT_FALSE(condition.value(QStringLiteral("description")).toString().isEmpty())
          << mission_id.toStdString()
          << " has a condition the briefing cannot describe";
    }
  }
}

TEST(MissionObjectivesTest, OptionalObjectivesReachTheBriefingSeparately) {
  bool saw_optional = false;
  for (const QString& mission_id : campaign_mission_ids()) {
    const QVariantMap definition = load_mission_definition_map(mission_id);
    const QVariantList optional =
        definition.value(QStringLiteral("optional_objectives")).toList();
    if (optional.isEmpty()) {
      continue;
    }
    saw_optional = true;

    // An optional objective must never be duplicated into the mandatory set:
    // victory would then wait on something the player was told was a bonus.
    const QVariantList victory =
        definition.value(QStringLiteral("victory_conditions")).toList();
    for (const QVariant& entry : optional) {
      const QString description =
          entry.toMap().value(QStringLiteral("description")).toString();
      EXPECT_FALSE(description.isEmpty())
          << mission_id.toStdString() << " has an undescribed optional objective";
      for (const QVariant& required : victory) {
        EXPECT_NE(required.toMap().value(QStringLiteral("description")).toString(),
                  description)
            << mission_id.toStdString()
            << " lists the same objective as both required and optional";
      }
    }
  }
  EXPECT_TRUE(saw_optional)
      << "no shipped mission carries an optional objective; the bonus path is dead";
}

TEST(MissionObjectivesTest, AMissionRequiringEveryConditionSaysSo) {
  // "all" is the mode that makes victory wait on every mandatory objective, so
  // a mission that ships more than one victory condition must declare it --
  // otherwise the first one alone would end the mission.
  for (const QString& mission_id : campaign_mission_ids()) {
    const QVariantMap definition = load_mission_definition_map(mission_id);
    const QVariantList victory =
        definition.value(QStringLiteral("victory_conditions")).toList();
    if (victory.size() <= 1) {
      continue;
    }
    EXPECT_EQ(definition.value(QStringLiteral("victory_mode")).toString(),
              QStringLiteral("all"))
        << mission_id.toStdString()
        << " has several victory conditions but does not require all of them";
  }
}

TEST(MissionObjectivesTest, AnUnknownMissionYieldsNothingRatherThanAHalfBriefing) {
  EXPECT_TRUE(load_mission_definition_map(QStringLiteral("no_such_mission")).isEmpty());
  EXPECT_TRUE(load_mission_definition_map(QString()).isEmpty());
}
