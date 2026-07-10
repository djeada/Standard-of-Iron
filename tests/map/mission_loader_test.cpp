#include <QCoreApplication>
#include <QDir>
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
        "commander_troop": "roman_veteran_consul",
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
          "commander_troop": "carthage_elephant_master",
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
  ASSERT_TRUE(mission.player_setup.commander_troop.has_value());
  EXPECT_EQ(*mission.player_setup.commander_troop, "roman_veteran_consul");
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
  ASSERT_TRUE(mission.ai_setups[0].commander_troop.has_value());
  EXPECT_EQ(*mission.ai_setups[0].commander_troop, "carthage_elephant_master");
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
      "commander_troop": "roman_veteran_consul",
      "starting_units": [],
      "starting_buildings": []
    },
    "ai_setups": [
      {
        "id": "road_guard",
        "nation": "carthage",
        "faction": "carthaginian",
        "color": "blue",
        "commander_troop": "carthage_elephant_master",
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

TEST_F(MissionLoaderTest, ShippedMissionsAuthorCommandersForPlayerAndAiSetups) {
  const QDir mission_dir = assetMissionDirectory();
  const QStringList mission_files =
      mission_dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
  ASSERT_FALSE(mission_files.isEmpty());

  for (const QString& file_name : mission_files) {
    MissionDefinition mission;
    QString error;
    ASSERT_TRUE(MissionLoader::load_from_json_file(
        mission_dir.absoluteFilePath(file_name), mission, &error))
        << file_name.toStdString() << ": " << error.toStdString();

    ASSERT_TRUE(mission.player_setup.commander_troop.has_value())
        << file_name.toStdString();
    EXPECT_FALSE(mission.player_setup.commander_troop->trimmed().isEmpty())
        << file_name.toStdString();

    for (std::size_t index = 0; index < mission.ai_setups.size(); ++index) {
      ASSERT_TRUE(mission.ai_setups[index].commander_troop.has_value())
          << file_name.toStdString() << " ai index " << index;
      EXPECT_FALSE(mission.ai_setups[index].commander_troop->trimmed().isEmpty())
          << file_name.toStdString() << " ai index " << index;
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
