#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>

#include "game/mission/campaign_manager.h"
#include "game/systems/save_load_service.h"

namespace {

constexpr const char* k_campaign_id = "second_punic_war";
constexpr const char* k_first_mission = "crossing_the_rhone";
constexpr const char* k_second_mission = "crossing_the_alps";
constexpr const char* k_final_mission = "battle_of_zama";

class CampaignManagerTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    QStandardPaths::setTestModeEnabled(true);
    s_saved_application_name = QCoreApplication::applicationName();
    const QString base = s_saved_application_name.isEmpty()
                             ? QStringLiteral("StandardOfIron")
                             : s_saved_application_name;
    QCoreApplication::setApplicationName(QStringLiteral("%1-campaign-tests-%2")
                                             .arg(base)
                                             .arg(QCoreApplication::applicationPid()));
  }

  static void TearDownTestSuite() {
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .removeRecursively();
    QCoreApplication::setApplicationName(s_saved_application_name);
    QStandardPaths::setTestModeEnabled(false);
  }

  void SetUp() override {

    QDir(Game::Systems::SaveLoadService::saves_directory()).removeRecursively();
    service = std::make_unique<Game::Systems::SaveLoadService>();

    const QVariantList campaigns = service->list_campaigns();
    ASSERT_FALSE(campaigns.isEmpty()) << "no campaigns were discovered";

    manager = std::make_unique<CampaignManager>();
    manager->set_save_service(service.get());
    manager->set_available_campaigns(campaigns);
  }

  void TearDown() override {
    manager.reset();
    if (service) {
      service->shutdown();
      service.reset();
    }
    QDir(Game::Systems::SaveLoadService::saves_directory()).removeRecursively();
  }

  void win(const char* mission_id) {
    int player_id = 1;
    manager->start_campaign_mission(
        QStringLiteral("%1/%2").arg(QLatin1String(k_campaign_id),
                                    QLatin1String(mission_id)),
        player_id);
    ASSERT_EQ(manager->current_mission_id(), QLatin1String(mission_id))
        << "mission " << mission_id << " did not load";
    manager->mark_current_mission_completed();
  }

  [[nodiscard]] auto mission_ids() const -> QStringList {
    QStringList ids;
    for (const QVariant& entry : service->list_campaigns()) {
      const QVariantMap campaign = entry.toMap();
      if (campaign.value(QStringLiteral("id")).toString() !=
          QLatin1String(k_campaign_id)) {
        continue;
      }
      for (const QVariant& mission :
           campaign.value(QStringLiteral("missions")).toList()) {
        ids.append(mission.toMap().value(QStringLiteral("mission_id")).toString());
      }
    }
    return ids;
  }

  [[nodiscard]] auto mission_state(const QString& mission_id) const -> QVariantMap {
    for (const QVariant& entry :
         service->get_campaign_mission_progress(QLatin1String(k_campaign_id))) {
      const QVariantMap row = entry.toMap();
      if (row.value(QStringLiteral("mission_id")).toString() == mission_id) {
        return row;
      }
    }
    return {};
  }

  std::unique_ptr<Game::Systems::SaveLoadService> service;
  std::unique_ptr<CampaignManager> manager;
  static QString s_saved_application_name;
};

QString CampaignManagerTest::s_saved_application_name;

TEST_F(CampaignManagerTest, WinningTheOpeningMissionDoesNotFinishTheCampaign) {
  win(k_first_mission);

  EXPECT_FALSE(manager->campaign_completed())
      << "mission one of the war ended the whole campaign";
  EXPECT_TRUE(mission_state(QLatin1String(k_first_mission))
                  .value(QStringLiteral("completed"))
                  .toBool());
  EXPECT_TRUE(mission_state(QLatin1String(k_second_mission))
                  .value(QStringLiteral("unlocked"))
                  .toBool());
  EXPECT_FALSE(mission_state(QLatin1String(k_second_mission))
                   .value(QStringLiteral("completed"))
                   .toBool());
}

TEST_F(CampaignManagerTest, TheCampaignOnlyCompletesAfterTheFinalMission) {
  const QStringList ids = mission_ids();
  ASSERT_GT(ids.size(), 1);
  ASSERT_EQ(ids.last(), QLatin1String(k_final_mission));

  for (int i = 0; i < ids.size() - 1; ++i) {
    win(ids[i].toUtf8().constData());
    EXPECT_FALSE(manager->campaign_completed())
        << "the campaign ended early at " << ids[i].toStdString();
  }

  win(k_final_mission);
  EXPECT_TRUE(manager->campaign_completed())
      << "clearing the last mission did not finish the campaign";
  EXPECT_TRUE(service->get_campaign_progress(QLatin1String(k_campaign_id))
                  .value(QStringLiteral("completed"))
                  .toBool());
}

TEST_F(CampaignManagerTest, StartingAnotherMissionClearsTheCompletionBanner) {
  for (const QString& id : mission_ids()) {
    win(id.toUtf8().constData());
  }
  ASSERT_TRUE(manager->campaign_completed());

  int player_id = 1;
  manager->start_campaign_mission(
      QStringLiteral("%1/%2").arg(QLatin1String(k_campaign_id),
                                  QLatin1String(k_first_mission)),
      player_id);
  EXPECT_FALSE(manager->campaign_completed());
}

TEST_F(CampaignManagerTest, ReplayingAFinishedCampaignKeepsItFinished) {
  for (const QString& id : mission_ids()) {
    win(id.toUtf8().constData());
  }
  ASSERT_TRUE(manager->campaign_completed());

  win(k_first_mission);

  EXPECT_TRUE(manager->campaign_completed())
      << "replaying mission one un-finished the campaign";
  for (const QString& id : mission_ids()) {
    EXPECT_TRUE(mission_state(id).value(QStringLiteral("completed")).toBool())
        << id.toStdString() << " lost its completion";
  }
}

TEST_F(CampaignManagerTest, DuplicateVictoriesDoNotAdvanceTwice) {
  win(k_first_mission);
  const QStringList ids = mission_ids();
  ASSERT_GT(ids.size(), 2);

  manager->mark_current_mission_completed();

  EXPECT_FALSE(mission_state(ids[2]).value(QStringLiteral("unlocked")).toBool())
      << "a repeated win unlocked two missions";
  EXPECT_FALSE(manager->campaign_completed());
}

TEST_F(CampaignManagerTest, ASkirmishNeverTouchesCampaignProgress) {
  manager->set_skirmish_context(QStringLiteral("assets/maps/map_forest.json"));
  manager->mark_current_mission_completed();

  EXPECT_FALSE(manager->campaign_completed());
  for (const QString& id : mission_ids()) {
    EXPECT_FALSE(mission_state(id).value(QStringLiteral("completed")).toBool())
        << id.toStdString() << " was completed by a skirmish";
  }
}

TEST_F(CampaignManagerTest, AMissionPathWithoutACampaignIsRejected) {
  int player_id = 1;
  manager->start_campaign_mission(QLatin1String(k_first_mission), player_id);

  EXPECT_TRUE(manager->current_mission_id().isEmpty())
      << "a bare mission id was accepted as a campaign path";
  EXPECT_TRUE(manager->current_campaign_id().isEmpty());
}

} // namespace
