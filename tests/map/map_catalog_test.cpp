#include <QFileInfo>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "game/map/map_catalog.h"

namespace {

auto load_map_entry(const QString& relative_path) -> QVariantMap {
  const QString path = QStringLiteral("assets/maps/%1").arg(relative_path);
  EXPECT_TRUE(QFileInfo::exists(path)) << path.toStdString();
  return Game::Map::MapCatalog::load_single_map(path);
}

} // namespace

TEST(MapCatalogTest, MapsWithUndeadZonesAreSoloPlayable) {
  const QVariantMap entry =
      load_map_entry(QStringLiteral("map_iron_sepulcher_watch.json"));

  ASSERT_FALSE(entry.isEmpty());
  EXPECT_TRUE(entry.value(QStringLiteral("soloPlayable")).toBool());
  EXPECT_EQ(entry.value(QStringLiteral("playerCount")).toInt(), 1);
}

TEST(MapCatalogTest, ConventionalSkirmishMapsAreNotSoloPlayable) {
  const QVariantMap entry = load_map_entry(QStringLiteral("map_forest.json"));

  ASSERT_FALSE(entry.isEmpty());
  EXPECT_FALSE(entry.value(QStringLiteral("soloPlayable")).toBool());
  EXPECT_GE(entry.value(QStringLiteral("playerCount")).toInt(), 2);
}
