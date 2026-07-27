#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QStringList>
#include <QTemporaryFile>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/map/campaign_definition.h"
#include "game/map/campaign_loader.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"

using namespace Game::Campaign;

class CampaignLoaderTest : public ::testing::Test {
protected:
  void SetUp() override {}

  auto createTestCampaign() -> QString {
    return R"({
      "id": "test_campaign",
      "title": "Test Campaign",
      "description": "A test campaign for unit testing",
      "missions": [
        {
          "mission_id": "mission_1",
          "order_index": 0,
          "intro_text": "Welcome to mission 1",
          "outro_text": "Mission 1 completed"
        },
        {
          "mission_id": "mission_2",
          "order_index": 1,
          "intro_text": "Welcome to mission 2",
          "outro_text": "Mission 2 completed",
          "difficulty_modifier": 1.2
        }
      ]
    })";
  }
};

TEST_F(CampaignLoaderTest, LoadsValidCampaign) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestCampaign().toUtf8());
  temp_file.flush();

  CampaignDefinition campaign;
  QString error;
  bool const result =
      CampaignLoader::load_from_json_file(temp_file.fileName(), campaign, &error);

  EXPECT_TRUE(result) << "Error: " << error.toStdString();
  EXPECT_EQ(campaign.id, "test_campaign");
  EXPECT_EQ(campaign.title, "Test Campaign");
  EXPECT_EQ(campaign.description, "A test campaign for unit testing");
}

TEST_F(CampaignLoaderTest, ParsesMissions) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(createTestCampaign().toUtf8());
  temp_file.flush();

  CampaignDefinition campaign;
  QString error;
  ASSERT_TRUE(
      CampaignLoader::load_from_json_file(temp_file.fileName(), campaign, &error));

  ASSERT_EQ(campaign.missions.size(), 2);

  EXPECT_EQ(campaign.missions[0].mission_id, "mission_1");
  EXPECT_EQ(campaign.missions[0].order_index, 0);
  EXPECT_TRUE(campaign.missions[0].intro_text.has_value());
  EXPECT_EQ(*campaign.missions[0].intro_text, "Welcome to mission 1");
  EXPECT_TRUE(campaign.missions[0].outro_text.has_value());
  EXPECT_EQ(*campaign.missions[0].outro_text, "Mission 1 completed");

  EXPECT_EQ(campaign.missions[1].mission_id, "mission_2");
  EXPECT_EQ(campaign.missions[1].order_index, 1);
  EXPECT_TRUE(campaign.missions[1].difficulty_modifier.has_value());
  EXPECT_FLOAT_EQ(*campaign.missions[1].difficulty_modifier, 1.2F);
}

TEST_F(CampaignLoaderTest, FailsOnInvalidJSON) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write("{ invalid json }");
  temp_file.flush();

  CampaignDefinition campaign;
  QString error;
  bool const result =
      CampaignLoader::load_from_json_file(temp_file.fileName(), campaign, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(CampaignLoaderTest, FailsOnNonexistentFile) {
  CampaignDefinition campaign;
  QString error;
  bool const result =
      CampaignLoader::load_from_json_file("/nonexistent/file.json", campaign, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(CampaignLoaderTest, HandlesEmptyMissions) {
  QString const json = R"({
    "id": "empty_campaign",
    "title": "Empty Campaign",
    "description": "Campaign with no missions",
    "missions": []
  })";

  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(json.toUtf8());
  temp_file.flush();

  CampaignDefinition campaign;
  QString error;
  ASSERT_TRUE(
      CampaignLoader::load_from_json_file(temp_file.fileName(), campaign, &error));

  EXPECT_EQ(campaign.missions.size(), 0);
}

TEST_F(CampaignLoaderTest, HandlesOptionalFields) {
  QString const json = R"({
    "id": "minimal_campaign",
    "title": "Minimal Campaign",
    "description": "Campaign with minimal mission data",
    "missions": [
      {
        "mission_id": "mission_1",
        "order_index": 0
      }
    ]
  })";

  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  temp_file.write(json.toUtf8());
  temp_file.flush();

  CampaignDefinition campaign;
  QString error;
  ASSERT_TRUE(
      CampaignLoader::load_from_json_file(temp_file.fileName(), campaign, &error));

  ASSERT_EQ(campaign.missions.size(), 1);
  EXPECT_EQ(campaign.missions[0].mission_id, "mission_1");
  EXPECT_FALSE(campaign.missions[0].intro_text.has_value());
  EXPECT_FALSE(campaign.missions[0].outro_text.has_value());
  EXPECT_FALSE(campaign.missions[0].difficulty_modifier.has_value());
}

TEST(SecondPunicWarCampaignShapeTest, CoversEveryObjectiveCategory) {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  ASSERT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(
              QStringLiteral("../../assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();

  ASSERT_EQ(campaign.missions.size(), 8U);

  QHash<QString, QStringList> victory_types_by_mission;
  QHash<QString, QStringList> defeat_types_by_mission;
  QHash<QString, QString> victory_mode_by_mission;

  for (const auto& entry : campaign.missions) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(
                QStringLiteral("../../assets/missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << mission_error.toStdString();

    QStringList victory_types;
    for (const auto& condition : mission.victory_conditions) {
      victory_types.append(condition.type);
    }
    QStringList defeat_types;
    for (const auto& condition : mission.defeat_conditions) {
      defeat_types.append(condition.type);
    }
    victory_types_by_mission.insert(entry.mission_id, victory_types);
    defeat_types_by_mission.insert(entry.mission_id, defeat_types);
    victory_mode_by_mission.insert(entry.mission_id, mission.victory_mode);
  }

  EXPECT_TRUE(victory_types_by_mission.value(QStringLiteral("crossing_the_alps"))
                  .contains(QStringLiteral("accumulate_resources")));

  EXPECT_TRUE(victory_types_by_mission.value(QStringLiteral("battle_of_trebia"))
                  .contains(QStringLiteral("survive_waves")));
  EXPECT_TRUE(victory_types_by_mission.value(QStringLiteral("campania_campaign"))
                  .contains(QStringLiteral("survive_waves")));

  EXPECT_TRUE(defeat_types_by_mission.value(QStringLiteral("battle_of_trasimene"))
                  .contains(QStringLiteral("time_limit")));

  for (const auto& mission_id : {QStringLiteral("crossing_the_rhone"),
                                 QStringLiteral("battle_of_ticino"),
                                 QStringLiteral("battle_of_cannae")}) {
    EXPECT_TRUE(victory_types_by_mission.value(mission_id)
                    .contains(QStringLiteral("capture_structures")))
        << mission_id.toStdString();
    EXPECT_FALSE(defeat_types_by_mission.value(mission_id)
                     .contains(QStringLiteral("time_limit")))
        << mission_id.toStdString();
  }

  EXPECT_EQ(victory_mode_by_mission.value(QStringLiteral("battle_of_zama")),
            QStringLiteral("all"));
  EXPECT_GE(victory_types_by_mission.value(QStringLiteral("battle_of_zama")).size(), 2);
  EXPECT_FALSE(defeat_types_by_mission.value(QStringLiteral("battle_of_zama"))
                   .contains(QStringLiteral("time_limit")));
}

TEST(SecondPunicWarCampaignShapeTest, DifficultyModifiersIncreaseMonotonically) {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  ASSERT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(
              QStringLiteral("../../assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();

  auto missions = campaign.missions;
  std::sort(missions.begin(), missions.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.order_index < rhs.order_index;
  });

  for (std::size_t i = 1; i < missions.size(); ++i) {
    EXPECT_GT(missions[i].difficulty_modifier, missions[i - 1].difficulty_modifier)
        << missions[i].mission_id.toStdString() << " is not harder than "
        << missions[i - 1].mission_id.toStdString();
  }
}
