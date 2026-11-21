#include <QString>
#include <QStringList>

#include <gtest/gtest.h>
#include <vector>

#include "game/map/campaign_definition.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/campaign_mission_index.h"
#include "game/systems/persistence/save_campaign_catalog.h"

namespace {

using Game::Campaign::CampaignDefinition;
using Game::Campaign::CampaignMission;
using Game::Mission::build_campaign_mission_index;
using Game::Mission::load_campaign_mission_index;

auto mission(const QString& mission_id, int order_index) -> CampaignMission {
  CampaignMission out;
  out.mission_id = mission_id;
  out.order_index = order_index;
  return out;
}

TEST(CampaignMissionIndex, ListsEveryShippedCampaignMission) {
  const auto index = load_campaign_mission_index();
  ASSERT_TRUE(index.ok()) << index.problems.join(QLatin1Char('\n')).toStdString();

  const auto campaigns =
      Game::Systems::SaveCampaignCatalog::load_campaign_definitions();
  qsizetype expected = 0;
  for (const auto& campaign : campaigns) {
    expected += static_cast<qsizetype>(campaign.missions.size());
  }
  EXPECT_EQ(index.references.size(), expected);
  EXPECT_TRUE(
      index.references.contains(QStringLiteral("second_punic_war/crossing_the_alps")));
}

TEST(CampaignMissionIndex, EveryReferenceStartsThroughTheCampaignMissionPath) {
  const auto index = load_campaign_mission_index();
  ASSERT_TRUE(index.ok()) << index.problems.join(QLatin1Char('\n')).toStdString();

  for (const QString& reference : index.references) {
    const QStringList parts = reference.split(QLatin1Char('/'));
    ASSERT_EQ(parts.size(), 2) << reference.toStdString();

    CampaignManager manager;
    int player_id = 1;
    manager.start_campaign_mission(reference, player_id);
    EXPECT_TRUE(manager.current_mission_definition().has_value())
        << reference.toStdString() << " did not start";
    EXPECT_EQ(manager.current_campaign_id(), parts[0]) << reference.toStdString();
    EXPECT_EQ(manager.current_mission_id(), parts[1]) << reference.toStdString();
  }
}

TEST(CampaignMissionIndex, OrderIsStableAndFollowsCampaignOrder) {
  const auto first = load_campaign_mission_index();
  const auto second = load_campaign_mission_index();
  EXPECT_EQ(first.references, second.references);

  const QStringList shipped = first.references;
  ASSERT_GE(shipped.size(), 2);
  EXPECT_EQ(shipped.front(), QStringLiteral("second_punic_war/crossing_the_rhone"));
  EXPECT_EQ(shipped.back(), QStringLiteral("second_punic_war/battle_of_zama"));

  const QString first_id = shipped.front().section(QLatin1Char('/'), 1);
  const QString second_id = shipped.at(1).section(QLatin1Char('/'), 1);

  CampaignDefinition later;
  later.id = QStringLiteral("z_campaign");
  later.missions = {mission(second_id, 1), mission(first_id, 0)};
  CampaignDefinition earlier;
  earlier.id = QStringLiteral("a_campaign");
  earlier.missions = {mission(first_id, 0)};

  const auto index = build_campaign_mission_index({later, earlier});
  ASSERT_TRUE(index.ok()) << index.problems.join(QLatin1Char('\n')).toStdString();
  EXPECT_EQ(index.references,
            (QStringList{QStringLiteral("a_campaign/%1").arg(first_id),
                         QStringLiteral("z_campaign/%1").arg(first_id),
                         QStringLiteral("z_campaign/%1").arg(second_id)}));
}

TEST(CampaignMissionIndex, NoCampaignDataIsAFailure) {
  const auto index = build_campaign_mission_index({});
  EXPECT_FALSE(index.ok());
  EXPECT_TRUE(index.references.isEmpty());
  EXPECT_FALSE(index.problems.isEmpty());
}

TEST(CampaignMissionIndex, BrokenMissionEntriesAreReportedNotListed) {
  CampaignDefinition campaign;
  campaign.id = QStringLiteral("second_punic_war");
  campaign.missions = {mission(QStringLiteral("crossing_the_alps"), 0),
                       mission(QStringLiteral("no_such_mission_file"), 1),
                       mission(QStringLiteral("bad/id"), 2),
                       mission(QString(), 3)};

  const auto index = build_campaign_mission_index({campaign});
  EXPECT_FALSE(index.ok());
  EXPECT_EQ(index.references,
            QStringList{QStringLiteral("second_punic_war/crossing_the_alps")});
  EXPECT_EQ(index.problems.size(), 3);
}

TEST(CampaignMissionIndex, CampaignIdWithASlashIsRejected) {
  CampaignDefinition campaign;
  campaign.id = QStringLiteral("bad/campaign");
  campaign.missions = {mission(QStringLiteral("crossing_the_alps"), 0)};

  const auto index = build_campaign_mission_index({campaign});
  EXPECT_FALSE(index.ok());
  EXPECT_TRUE(index.references.isEmpty());
}

} // namespace
