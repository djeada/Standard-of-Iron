#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include <QTemporaryFile>
#include <gtest/gtest.h>

using namespace Game::Mission;

class MissionLoaderTest : public ::testing::Test {
protected:
  void SetUp() override {}

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
            "position": {"x": 60, "z": 60}
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
          "food": 500
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
  bool result =
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error);

  EXPECT_TRUE(result) << "Error: " << error.toStdString();
  EXPECT_EQ(mission.id, "test_mission");
  EXPECT_EQ(mission.title, "Test Mission");
  EXPECT_EQ(mission.summary, "A test mission for unit testing");
  EXPECT_EQ(mission.map_path, ":/assets/maps/map_forest.json");
}

TEST_F(MissionLoaderTest, ParsesPlayerSetup) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error));

  EXPECT_EQ(mission.player_setup.nation, "roman_republic");
  EXPECT_EQ(mission.player_setup.faction, "roman");
  EXPECT_EQ(mission.player_setup.color, "red");
  EXPECT_EQ(mission.player_setup.starting_units.size(), 1);
  EXPECT_EQ(mission.player_setup.starting_buildings.size(), 1);
  EXPECT_EQ(mission.player_setup.starting_resources.gold, 1000);
  EXPECT_EQ(mission.player_setup.starting_resources.food, 500);
}

TEST_F(MissionLoaderTest, ParsesAISetups) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestMission().toUtf8());
  temp_file.flush();

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error));

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
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error));

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
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error));

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
  bool result =
      MissionLoader::loadFromJsonFile(temp_file.fileName(), mission, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(MissionLoaderTest, FailsOnNonexistentFile) {
  MissionDefinition mission;
  QString error;
  bool result = MissionLoader::loadFromJsonFile("/nonexistent/file.json",
                                                mission, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}
