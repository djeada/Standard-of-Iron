#include "game/map/campaign_definition.h"
#include "game/map/campaign_loader.h"
#include <QTemporaryFile>
#include <gtest/gtest.h>

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
  bool result =
      CampaignLoader::loadFromJsonFile(temp_file.fileName(), campaign, &error);

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
      CampaignLoader::loadFromJsonFile(temp_file.fileName(), campaign, &error));

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
  bool result =
      CampaignLoader::loadFromJsonFile(temp_file.fileName(), campaign, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(CampaignLoaderTest, FailsOnNonexistentFile) {
  CampaignDefinition campaign;
  QString error;
  bool result = CampaignLoader::loadFromJsonFile("/nonexistent/file.json",
                                                 campaign, &error);

  EXPECT_FALSE(result);
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(CampaignLoaderTest, HandlesEmptyMissions) {
  QString json = R"({
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
      CampaignLoader::loadFromJsonFile(temp_file.fileName(), campaign, &error));

  EXPECT_EQ(campaign.missions.size(), 0);
}

TEST_F(CampaignLoaderTest, HandlesOptionalFields) {
  QString json = R"({
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
      CampaignLoader::loadFromJsonFile(temp_file.fileName(), campaign, &error));

  ASSERT_EQ(campaign.missions.size(), 1);
  EXPECT_EQ(campaign.missions[0].mission_id, "mission_1");
  EXPECT_FALSE(campaign.missions[0].intro_text.has_value());
  EXPECT_FALSE(campaign.missions[0].outro_text.has_value());
  EXPECT_FALSE(campaign.missions[0].difficulty_modifier.has_value());
}
