#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <limits>
#include <string>

#include "game/map/map_catalog.h"
#include "game/map/mission_catalog.h"

namespace {

auto mission_ids(const QVariantList& missions) -> QStringList {
  QStringList ids;
  for (const QVariant& entry : missions) {
    ids.append(entry.toMap().value(QStringLiteral("mission_id")).toString());
  }
  return ids;
}

} // namespace

TEST(MissionCatalogTest, StandaloneMissionsExcludeTheCampaignAndTheTutorial) {
  const QStringList ids = mission_ids(Game::Map::MissionCatalog::standalone_missions());
  ASSERT_FALSE(ids.isEmpty());

  const QSet<QString> owned = Game::Map::MissionCatalog::campaign_mission_ids();
  ASSERT_FALSE(owned.isEmpty()) << "no campaign claims any mission";

  for (const QString& id : ids) {
    EXPECT_FALSE(owned.contains(id))
        << id.toStdString()
        << " is a link in a campaign chronicle; the Missions menu must not offer it "
           "out of order";
  }
  EXPECT_FALSE(ids.contains(QStringLiteral("tutorial")))
      << "the tutorial is reached from its own menu entry, not the Missions list";
}

TEST(MissionCatalogTest, StandaloneMissionsCarryWhatTheMenuNeeds) {
  const QVariantList missions = Game::Map::MissionCatalog::standalone_missions();
  ASSERT_FALSE(missions.isEmpty());

  for (const QVariant& entry : missions) {
    const QVariantMap mission = entry.toMap();
    const std::string id =
        mission.value(QStringLiteral("mission_id")).toString().toStdString();

    EXPECT_FALSE(mission.value(QStringLiteral("title")).toString().isEmpty()) << id;
    EXPECT_FALSE(mission.value(QStringLiteral("summary")).toString().isEmpty()) << id;
    EXPECT_FALSE(mission.value(QStringLiteral("file_path")).toString().isEmpty()) << id;
    EXPECT_FALSE(mission.value(QStringLiteral("map_name")).toString().isEmpty())
        << id << " names a map the catalogue could not open";
    EXPECT_GT(mission.value(QStringLiteral("map_width")).toInt(), 0) << id;
    EXPECT_FALSE(mission.value(QStringLiteral("objectives")).toList().isEmpty())
        << id << " lists no objective, so the menu can say nothing about it";
  }
}

TEST(MissionCatalogTest, StandaloneMissionsAreOrderedByTheirAuthoredMenuOrder) {
  const QVariantList missions = Game::Map::MissionCatalog::standalone_missions();
  ASSERT_GE(missions.size(), 2);

  int previous = std::numeric_limits<int>::min();
  for (const QVariant& entry : missions) {
    const int order = entry.toMap().value(QStringLiteral("menu_order")).toInt();
    EXPECT_GE(order, previous);
    previous = order;
  }
}

TEST(MissionCatalogTest, NoMissionMapIsOfferedAsASkirmishField) {
  const QSet<QString> mission_maps = Game::Map::MissionCatalog::mission_map_paths();
  ASSERT_FALSE(mission_maps.isEmpty());

  for (const QVariant& entry : Game::Map::MapCatalog::available_maps()) {
    const QString path = entry.toMap().value(QStringLiteral("path")).toString();
    EXPECT_FALSE(mission_maps.contains(path))
        << path.toStdString()
        << " is authored around one mission's objective; offering it as a free-play "
           "field puts the player on a board with nothing to do";
  }
}

TEST(MissionCatalogTest, TheSkirmishRosterIsStillWorthOpening) {
  const QVariantList maps = Game::Map::MapCatalog::available_maps();
  EXPECT_GE(maps.size(), 6)
      << "subtracting the mission maps has emptied the skirmish roster";
}
