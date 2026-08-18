#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"

using namespace Game::Mission;

class MissionLoaderTest : public ::testing::Test {
protected:
  void SetUp() override {}

  auto assetMissionPath(const QString& file_name) -> QString {
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../assets/missions/%1").arg(file_name));
  }

  auto assetMissionDirectory() -> QDir {
    return QDir(QDir(QCoreApplication::applicationDirPath())
                    .absoluteFilePath(QStringLiteral("../../assets/missions")));
  }

  auto createTestMission() -> QString {
    return R"({
      "id": "test_mission",
      "title": "Test Mission",
      "summary": "A test mission for unit testing",
      "map_path": ":/assets/maps/map_forest.json",
      "player_setup": {
        "nation": "roman_republic",
        "faction": "roman",
        "color": "red",
        "starting_units": [
          {
            "type": "spearman",
            "count": 10,
            "position": {"x": 60, "z": 60},
            "behavior": "guard",
            "guard_radius": 14
          }
        ],
        "starting_buildings": [
          {
            "type": "barracks",
            "position": {"x": 60, "z": 60},
            "max_population": 200
          }
        ],
        "starting_resources": {
          "gold": 1000,
          "food": 500,
          "wood": 125
        }
      },
      "ai_setups": [
        {
          "id": "enemy_1",
          "nation": "carthage",
          "faction": "carthaginian",
          "color": "blue",
          "difficulty": "medium",
          "personality": {
            "aggression": 0.7,
            "defense": 0.3,
            "harassment": 0.5
          },
          "starting_units": [],
          "starting_buildings": [],
          "waves": [
            {
              "timing": 120.0,
              "composition": [
                {"type": "swordsman", "count": 8}
              ],
              "entry_point": {"x": 190, "z": 190}
            }
          ]
        }
      ],
      "victory_conditions": [
        {
          "type": "survive_duration",
          "duration": 600.0,
          "description": "Survive for 10 minutes"
        }
      ],
      "defeat_conditions": [
        {
          "type": "lose_structure",
          "structure_type": "barracks",
          "description": "Do not lose your barracks"
        }
      ],
      "events": []
    })";
  }
};

TEST_F(MissionLoaderTest, LoadsValidMission) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  bool const result =
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error);

  EXPECT_TRUE(result) << "Error: " << error.toStdString();
  EXPECT_EQ(mission.id, "test_mission");
  EXPECT_EQ(mission.title, "Test Mission");
  EXPECT_EQ(mission.summary, "A test mission for unit testing");
  EXPECT_EQ(mission.map_path, ":/assets/maps/map_forest.json");
  ASSERT_EQ(mission.player_setup.starting_units.size(), 1U);
  EXPECT_EQ(mission.player_setup.starting_units[0].behavior, UnitBehavior::Guard);
  EXPECT_FLOAT_EQ(mission.player_setup.starting_units[0].guard_radius, 14.0F);
}

TEST_F(MissionLoaderTest, ParsesPlayerSetup) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  EXPECT_EQ(mission.player_setup.nation, "roman_republic");
  EXPECT_EQ(mission.player_setup.faction, "roman");
  EXPECT_EQ(mission.player_setup.color, "red");
  EXPECT_EQ(mission.player_setup.starting_units.size(), 1);
  EXPECT_EQ(mission.player_setup.starting_buildings.size(), 1);
  EXPECT_EQ(
      mission.player_setup.starting_resources.get(Game::Systems::ResourceType::Gold),
      1000);
  EXPECT_EQ(
      mission.player_setup.starting_resources.get(Game::Systems::ResourceType::Food),
      500);
  EXPECT_EQ(
      mission.player_setup.starting_resources.get(Game::Systems::ResourceType::Wood),
      125);
}

TEST_F(MissionLoaderTest, ParsesAISetups) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  ASSERT_EQ(mission.ai_setups.size(), 1);
  EXPECT_EQ(mission.ai_setups[0].id, "enemy_1");
  EXPECT_EQ(mission.ai_setups[0].nation, "carthage");
  EXPECT_EQ(mission.ai_setups[0].difficulty, "medium");
  EXPECT_FLOAT_EQ(mission.ai_setups[0].personality.aggression, 0.7F);
  EXPECT_EQ(mission.ai_setups[0].waves.size(), 1);
}

TEST_F(MissionLoaderTest, ParsesVictoryConditions) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  ASSERT_EQ(mission.victory_conditions.size(), 1);
  EXPECT_EQ(mission.victory_conditions[0].type, "survive_duration");
  EXPECT_TRUE(mission.victory_conditions[0].duration.has_value());
  EXPECT_FLOAT_EQ(*mission.victory_conditions[0].duration, 600.0F);
}

TEST_F(MissionLoaderTest, ParsesDefeatConditions) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  ASSERT_EQ(mission.defeat_conditions.size(), 1);
  EXPECT_EQ(mission.defeat_conditions[0].type, "lose_structure");
  EXPECT_TRUE(mission.defeat_conditions[0].structure_type.has_value());
  EXPECT_EQ(*mission.defeat_conditions[0].structure_type, "barracks");
}

TEST_F(MissionLoaderTest, ParsesAccumulateResourcesAndVictoryMode) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(R"({
      "id": "gather_mission",
      "title": "Gather Mission",
      "map_path": ":/assets/maps/map_forest.json",
      "victory_mode": "all",
      "victory_conditions": [
        {
          "type": "accumulate_resources",
          "resources": {"wood": 600, "stone": 350, "iron": 300},
          "description": "Provision the column"
        },
        {
          "type": "survive_waves",
          "wave_count": 3,
          "description": "Break all three assaults"
        }
      ],
      "defeat_conditions": [
        {"type": "time_limit", "duration": 420.0, "description": "Beat the clock"}
      ]
    })");
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error))
      << error.toStdString();

  EXPECT_EQ(mission.victory_mode, "all");

  ASSERT_EQ(mission.victory_conditions.size(), 2);
  ASSERT_TRUE(mission.victory_conditions[0].resources.has_value());
  EXPECT_EQ(
      mission.victory_conditions[0].resources->get(Game::Systems::ResourceType::Wood),
      600);
  EXPECT_EQ(
      mission.victory_conditions[0].resources->get(Game::Systems::ResourceType::Stone),
      350);
  EXPECT_EQ(
      mission.victory_conditions[0].resources->get(Game::Systems::ResourceType::Iron),
      300);
  EXPECT_EQ(
      mission.victory_conditions[0].resources->get(Game::Systems::ResourceType::Gold),
      0);

  EXPECT_EQ(mission.victory_conditions[1].type, "survive_waves");
  ASSERT_TRUE(mission.victory_conditions[1].wave_count.has_value());
  EXPECT_EQ(*mission.victory_conditions[1].wave_count, 3);

  ASSERT_EQ(mission.defeat_conditions.size(), 1);
  EXPECT_EQ(mission.defeat_conditions[0].type, "time_limit");
  ASSERT_TRUE(mission.defeat_conditions[0].duration.has_value());
  EXPECT_FLOAT_EQ(*mission.defeat_conditions[0].duration, 420.0F);
}

TEST_F(MissionLoaderTest, ParsesCommanderMessages) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(R"({
      "id": "taunt_mission",
      "title": "Taunt Mission",
      "map_path": ":/assets/maps/map_forest.json",
      "commander_messages": [
        {
          "id": "open",
          "speaker": "roman_veteran_consul",
          "pose": "dismissive",
          "trigger": {"type": "mission_start", "delay": 2.5},
          "text": "Cross where you like.",
          "voice_cue": "alert.commander_message",
          "duration": 13.0,
          "priority": 100
        },
        {
          "id": "town_lost",
          "speaker": "roman_veteran_consul",
          "trigger": {
            "type": "structure_captured",
            "owner_id": 3,
            "by_owner_id": "player",
            "structure_type": "barracks",
            "at": {"x": 566, "z": 545},
            "radius": 24.0
          },
          "text": "You have the river town.",
          "once": false
        },
        {
          "id": "consul_down",
          "speaker": "roman_field_commander",
          "trigger": {
            "type": "commander_defeated",
            "nation": "roman_republic",
            "by_owner_id": "player"
          },
          "text": "You killed a consul."
        }
      ]
    })");
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error))
      << error.toStdString();

  ASSERT_EQ(mission.commander_messages.size(), 3);

  const auto& open = mission.commander_messages[0];
  EXPECT_EQ(open.id, "open");
  EXPECT_EQ(open.speaker, "roman_veteran_consul");
  EXPECT_EQ(open.pose, "dismissive");
  EXPECT_EQ(open.trigger, CommanderMessageTrigger::MissionStart);
  EXPECT_FLOAT_EQ(open.delay, 2.5F);
  EXPECT_FLOAT_EQ(open.duration, 13.0F);
  EXPECT_EQ(open.priority, 100);
  EXPECT_TRUE(open.once);

  const auto& capture = mission.commander_messages[1];
  EXPECT_EQ(capture.trigger, CommanderMessageTrigger::StructureCaptured);
  ASSERT_TRUE(capture.condition.owner_id.has_value());
  EXPECT_EQ(*capture.condition.owner_id, 3);
  EXPECT_FALSE(capture.condition.owner_is_local);
  EXPECT_TRUE(capture.condition.by_owner_is_local);
  EXPECT_FALSE(capture.condition.by_owner_id.has_value());
  ASSERT_TRUE(capture.condition.subject_type.has_value());
  EXPECT_EQ(*capture.condition.subject_type, "barracks");
  ASSERT_TRUE(capture.condition.at.has_value());
  EXPECT_FLOAT_EQ(capture.condition.at->x, 566.0F);
  ASSERT_TRUE(capture.condition.radius.has_value());
  EXPECT_FLOAT_EQ(*capture.condition.radius, 24.0F);
  EXPECT_FALSE(capture.once);

  const auto& death = mission.commander_messages[2];
  EXPECT_EQ(death.trigger, CommanderMessageTrigger::CommanderDefeated);
  ASSERT_TRUE(death.condition.nation.has_value());
  EXPECT_EQ(*death.condition.nation, "roman_republic");
  EXPECT_FLOAT_EQ(death.duration, k_default_commander_message_seconds);
}

TEST_F(MissionLoaderTest, CommanderMessagesAreAbsentWhenTheMissionAuthorsNone) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  EXPECT_TRUE(mission.commander_messages.empty());
}

TEST_F(MissionLoaderTest, VictoryModeDefaultsToAnyWhenOmitted) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error));

  EXPECT_EQ(mission.victory_mode, "any");
}

TEST_F(MissionLoaderTest, FailsOnInvalidJSON) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write("{ invalid json }");
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  bool const result =
      MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(MissionLoaderTest, FailsOnNonexistentFile) {
  MissionDefinition mission;
  QString error;
  bool const result =
      MissionLoader::load_from_json_file("/nonexistent/file.json", mission, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(MissionLoaderTest, CrossingTheRhonePatrolForcesStayDefensive) {
  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(
      assetMissionPath(QStringLiteral("crossing_the_rhone.json")), mission, &error))
      << error.toStdString();

  const auto patrol_it = std::find_if(
      mission.ai_setups.begin(), mission.ai_setups.end(), [](const AISetup& setup) {
        return setup.id == "roman_patrol_forces";
      });
  ASSERT_NE(patrol_it, mission.ai_setups.end());
  ASSERT_TRUE(patrol_it->strategy.has_value());
  EXPECT_EQ(*patrol_it->strategy, "defensive");
  EXPECT_LT(patrol_it->personality.aggression, 0.5F);
  EXPECT_GT(patrol_it->personality.defense, 0.7F);
}

TEST_F(MissionLoaderTest, ParsesMissionStages) {
  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(
      assetMissionPath(QStringLiteral("crossing_the_rhone.json")), mission, &error))
      << error.toStdString();

  ASSERT_FALSE(mission.stages.empty());

  const auto& first = mission.stages.front();
  EXPECT_EQ(first.type, QStringLiteral("reach_position"));
  EXPECT_FALSE(first.title.isEmpty());
  ASSERT_TRUE(first.target.has_value());
  ASSERT_TRUE(first.target_radius.has_value());
  EXPECT_GT(*first.target_radius, 0.0F);

  const auto capture_it = std::find_if(
      mission.stages.begin(), mission.stages.end(), [](const MissionStage& stage) {
        return stage.type == QStringLiteral("capture_structures");
      });
  ASSERT_NE(capture_it, mission.stages.end());
  ASSERT_EQ(capture_it->structure_types.size(), 1U);
  EXPECT_EQ(capture_it->structure_types.front(), QStringLiteral("barracks"));
  EXPECT_GE(capture_it->required_count, 1);
}

TEST_F(MissionLoaderTest,
       EveryCampaignMissionAuthorsStagesThatEndOnItsVictoryCondition) {
  for (const QString& mission_file : {QStringLiteral("crossing_the_rhone.json"),
                                      QStringLiteral("crossing_the_alps.json"),
                                      QStringLiteral("battle_of_ticino.json"),
                                      QStringLiteral("battle_of_trebia.json"),
                                      QStringLiteral("battle_of_trasimene.json"),
                                      QStringLiteral("battle_of_cannae.json"),
                                      QStringLiteral("campania_campaign.json"),
                                      QStringLiteral("battle_of_zama.json")}) {
    MissionDefinition mission;
    QString error;
    ASSERT_TRUE(MissionLoader::load_from_json_file(
        assetMissionPath(mission_file), mission, &error))
        << mission_file.toStdString() << ": " << error.toStdString();

    ASSERT_FALSE(mission.stages.empty())
        << mission_file.toStdString() << " has no stages";

    const bool ends_on_a_victory_condition =
        std::any_of(mission.victory_conditions.begin(),
                    mission.victory_conditions.end(),
                    [&mission](const Condition& condition) {
                      return condition.type == mission.stages.back().type;
                    });
    EXPECT_TRUE(ends_on_a_victory_condition)
        << mission_file.toStdString() << " ends on stage type "
        << mission.stages.back().type.toStdString()
        << ", which is not one of its victory conditions";

    for (const auto& stage : mission.stages) {
      EXPECT_FALSE(stage.id.isEmpty()) << mission_file.toStdString();
      EXPECT_FALSE(stage.title.isEmpty()) << mission_file.toStdString();
      EXPECT_FALSE(stage.type.isEmpty()) << mission_file.toStdString();
    }
  }
}

TEST_F(MissionLoaderTest, ParsesAuthoredUnitPatrolBehavior) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QString json = R"({
    "id": "patrol_mission",
    "title": "Patrol Mission",
    "summary": "Patrol parsing",
    "map_path": ":/assets/maps/map_forest.json",
    "player_setup": {
      "nation": "roman_republic",
      "faction": "roman",
      "color": "red",
      "starting_units": [],
      "starting_buildings": []
    },
    "ai_setups": [
      {
        "id": "road_guard",
        "nation": "carthage",
        "faction": "carthaginian",
        "color": "blue",
        "difficulty": "medium",
        "starting_units": [
          {
            "type": "archer",
            "count": 2,
            "position": {"x": 50, "z": 40},
            "behavior": "patrol",
            "patrol_waypoints": [
              {"x": 56, "z": 40},
              {"x": 56, "z": 48}
            ]
          }
        ],
        "starting_buildings": []
      }
    ],
    "victory_conditions": [],
    "defeat_conditions": []
  })";
  temp_file.write(json.toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error))
      << error.toStdString();

  ASSERT_EQ(mission.ai_setups.size(), 1U);
  ASSERT_EQ(mission.ai_setups[0].starting_units.size(), 1U);
  const UnitSetup& unit = mission.ai_setups[0].starting_units[0];
  EXPECT_EQ(unit.behavior, UnitBehavior::Patrol);
  ASSERT_EQ(unit.patrol_waypoints.size(), 2U);
  EXPECT_FLOAT_EQ(unit.patrol_waypoints[0].x, 56.0F);
  EXPECT_FLOAT_EQ(unit.patrol_waypoints[1].z, 48.0F);
}

TEST_F(MissionLoaderTest, ShippedMissionsDoNotAuthorCommanders) {
  const QDir mission_dir = assetMissionDirectory();
  const QStringList mission_files =
      mission_dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
  ASSERT_FALSE(mission_files.isEmpty());

  for (const QString& file_name : mission_files) {
    QFile file(mission_dir.absoluteFilePath(file_name));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << file_name.toStdString();
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    EXPECT_FALSE(root.value("player_setup").toObject().contains("commander_troop"))
        << file_name.toStdString()
        << ": player_setup must not author a commander; use the map's spawns[]";

    const QJsonArray ai_setups = root.value("ai_setups").toArray();
    for (qsizetype index = 0; index < ai_setups.size(); ++index) {
      EXPECT_FALSE(ai_setups[index].toObject().contains("commander_troop"))
          << file_name.toStdString() << " ai index " << index
          << ": must not author a commander; use the map's spawns[]";
    }
  }
}

TEST_F(MissionLoaderTest, ParsesUndeadMissionFields) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(R"({
    "id": "sepulcher_objective",
    "title": "Sepulcher Objective",
    "summary": "Test undead objective parsing",
    "map_path": ":/assets/maps/map_forest.json",
    "include_ambient_undead": true,
    "player_setup": {"nation": "roman_republic", "faction": "roman", "color": "red"},
    "victory_conditions": [
      {"type": "clear_undead_zone", "zone_id": "sepulcher_ruin"},
      {"type": "survive_undead_wave", "zone_id": "sepulcher_ruin", "wave_count": 2}
    ],
    "defeat_conditions": [],
    "events": []
  })");
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(temp_file.fileName(), mission, &error))
      << error.toStdString();

  EXPECT_TRUE(mission.include_ambient_undead);
  ASSERT_EQ(mission.victory_conditions.size(), 2);
  ASSERT_TRUE(mission.victory_conditions[0].zone_id.has_value());
  EXPECT_EQ(*mission.victory_conditions[0].zone_id, QStringLiteral("sepulcher_ruin"));
  EXPECT_EQ(mission.victory_conditions[1].wave_count.value_or(0), 2);
}
