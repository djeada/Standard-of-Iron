#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "game/map/base_options.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "render_bridge/minimap/map_preview_generator.h"

namespace {

auto map_path(const QString& file_name) -> QString {
  return QStringLiteral("assets/maps/%1").arg(file_name);
}

auto load_map_at(const QString& path) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition def;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(path, def, &error))
      << path.toStdString() << ": " << error.toStdString();
  return def;
}

auto load_map(const QString& file_name) -> Game::Map::MapDefinition {
  return load_map_at(map_path(file_name));
}

auto skirmish_map_paths() -> QStringList {
  QStringList paths;
  for (const QVariant& entry : Game::Map::MapCatalog::available_maps()) {
    const QString path = entry.toMap().value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) {
      paths.append(path);
    }
  }
  return paths;
}

} // namespace

TEST(BaseOptionsTest, ListsEveryBarracksIncludingTheNeutralOnes) {
  const auto def = load_map(QStringLiteral("map_rivers.json"));
  const auto options = Game::Map::collect_base_options(def);

  QSet<QString> keys;
  for (const auto& option : options) {
    keys.insert(option.key);
  }

  EXPECT_EQ(options.size(), 4U)
      << "The Two Fords authors two starts and two toll barracks";
  EXPECT_TRUE(keys.contains(QStringLiteral("p1_barracks")));
  EXPECT_TRUE(keys.contains(QStringLiteral("p2_barracks")));
  EXPECT_TRUE(keys.contains(QStringLiteral("north_toll_barracks")));
  EXPECT_TRUE(keys.contains(QStringLiteral("south_toll_barracks")));
}

TEST(BaseOptionsTest, TheDefaultSeatingIsWhateverTheMapAuthored) {
  const auto def = load_map(QStringLiteral("map_rivers.json"));
  const auto assignments = Game::Map::default_base_assignments(def);

  ASSERT_EQ(assignments.size(), 2U);
  EXPECT_EQ(assignments.at(1), QStringLiteral("p1_barracks"));
  EXPECT_EQ(assignments.at(2), QStringLiteral("p2_barracks"));
}

TEST(BaseOptionsTest, AnUnnamedStructureStillGetsAStableKey) {
  const auto def = load_map(QStringLiteral("map_tutorial.json"));
  const auto options = Game::Map::collect_base_options(def);

  ASSERT_FALSE(options.empty());
  for (const auto& option : options) {
    EXPECT_FALSE(option.key.isEmpty())
        << "a base with no key cannot be handed to a player";
  }
}

TEST(BaseOptionsTest, EverySkirmishMapCanSeatEveryPlayerItAdvertises) {
  const QStringList paths = skirmish_map_paths();
  ASSERT_FALSE(paths.isEmpty());

  for (const QString& path : paths) {
    const auto def = load_map_at(path);
    const auto options = Game::Map::collect_base_options(def);

    QSet<int> advertised_slots;
    for (const auto& structure : def.structures) {
      if (structure.player_id > 0) {
        advertised_slots.insert(structure.player_id);
      }
    }
    for (const auto& spawn : def.spawns) {
      if (spawn.player_id > 0) {
        advertised_slots.insert(spawn.player_id);
      }
    }

    EXPECT_GE(options.size(), static_cast<std::size_t>(advertised_slots.size()))
        << path.toStdString()
        << " advertises more player slots than it has bases to seat them at";
  }
}

TEST(BaseMarkersTest, EveryMarkerIsNamedAndSitsInsideThePreview) {
  const QVariantList markers = Game::Map::Minimap::MapPreviewGenerator::base_markers(
      map_path(QStringLiteral("map_rivers.json")));

  ASSERT_EQ(markers.size(), 4);

  QSet<QString> keys;
  QSet<QString> names;
  for (const QVariant& entry : markers) {
    const QVariantMap marker = entry.toMap();
    const QString key = marker.value(QStringLiteral("key")).toString();
    const QString name = marker.value(QStringLiteral("name")).toString();

    EXPECT_FALSE(key.isEmpty());
    EXPECT_FALSE(name.isEmpty()) << "an unnamed base cannot be picked from a list";
    keys.insert(key);
    names.insert(name);

    const double preview_x = marker.value(QStringLiteral("previewX")).toDouble();
    const double preview_y = marker.value(QStringLiteral("previewY")).toDouble();
    EXPECT_GE(preview_x, 0.0);
    EXPECT_LE(preview_x, 1.0);
    EXPECT_GE(preview_y, 0.0);
    EXPECT_LE(preview_y, 1.0);
  }

  EXPECT_EQ(keys.size(), markers.size());
  EXPECT_EQ(names.size(), markers.size())
      << "two bases sharing a name would make the picker ambiguous";
}

TEST(BaseMarkersTest, NoSkirmishMapOffersTwoBasesUnderTheSameName) {
  const QStringList paths = skirmish_map_paths();
  ASSERT_FALSE(paths.isEmpty());

  for (const QString& path : paths) {
    const QVariantList markers =
        Game::Map::Minimap::MapPreviewGenerator::base_markers(path);

    QSet<QString> names;
    for (const QVariant& entry : markers) {
      names.insert(entry.toMap().value(QStringLiteral("name")).toString());
    }

    EXPECT_EQ(names.size(), markers.size())
        << path.toStdString() << " names two of its bases the same way";
  }
}

TEST(BaseMarkersTest, AnAuthoredPlaceKeepsItsNameAndASlotBaseGetsItsBearing) {
  const QVariantList markers = Game::Map::Minimap::MapPreviewGenerator::base_markers(
      map_path(QStringLiteral("map_forest.json")));

  QVariantMap lodge;
  QVariantMap slot_base;
  for (const QVariant& entry : markers) {
    const QVariantMap marker = entry.toMap();
    const QString key = marker.value(QStringLiteral("key")).toString();
    if (key == QStringLiteral("east_lodge_barracks")) {
      lodge = marker;
    } else if (key == QStringLiteral("p1_barracks")) {
      slot_base = marker;
    }
  }

  ASSERT_FALSE(lodge.isEmpty());
  ASSERT_FALSE(slot_base.isEmpty());
  EXPECT_EQ(lodge.value(QStringLiteral("name")).toString(),
            QStringLiteral("East Lodge"));
  EXPECT_EQ(lodge.value(QStringLiteral("defaultPlayerId")).toInt(), 0);
  EXPECT_NE(slot_base.value(QStringLiteral("name")).toString(), QStringLiteral("P1"))
      << "a bookkeeping id is not a name a player can navigate by";
  EXPECT_EQ(slot_base.value(QStringLiteral("defaultPlayerId")).toInt(), 1);
}
