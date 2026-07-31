#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "tools/map_editor/commander_preview.h"
#include "tools/map_editor/map_data.h"

namespace {

auto repo_root() -> QString {
  QDir dir = QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir();
  EXPECT_TRUE(dir.cdUp());
  EXPECT_TRUE(dir.cdUp());
  return dir.absolutePath();
}

auto read_json(const QString& path) -> QJsonObject {
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly));
  const auto document = QJsonDocument::fromJson(file.readAll());
  EXPECT_TRUE(document.isObject());
  return document.object();
}

auto commander_for_owner(const QVector<MapEditor::DerivedCommander>& commanders,
                         int owner_id) -> const MapEditor::DerivedCommander* {
  for (const auto& commander : commanders) {
    if (commander.owner_id == owner_id) {
      return &commander;
    }
  }
  return nullptr;
}

} // namespace

TEST(MapEditorCommanderPreviewTest, ConfiguredCommandersWinOverNationDefaults) {
  EXPECT_EQ(MapEditor::resolve_commander_troop("carthage", ""),
            QStringLiteral("carthage_elephant_master"));
  EXPECT_EQ(MapEditor::resolve_commander_troop("roman_republic", ""),
            QStringLiteral("roman_veteran_consul"));
  EXPECT_EQ(
      MapEditor::resolve_commander_troop("roman_republic", "roman_legion_organizer"),
      QStringLiteral("roman_legion_organizer"));
  EXPECT_EQ(MapEditor::resolve_commander_troop("roman_republic", "archer"),
            QStringLiteral("roman_veteran_consul"));
  EXPECT_EQ(
      MapEditor::resolve_commander_troop("roman_republic", "carthage_elephant_master"),
      QStringLiteral("roman_veteran_consul"));
}

TEST(MapEditorCommanderPreviewTest, RhoneMissionExposesOneCommanderPerOwner) {
  const QString root = repo_root();
  MapEditor::MapData map;
  QString error;
  ASSERT_TRUE(map.load_from_json(root + "/assets/maps/map_crossing_rhone.json", &error))
      << error.toStdString();

  const QJsonObject mission =
      read_json(root + "/assets/missions/crossing_the_rhone.json");
  const QVector<MapEditor::DerivedCommander> commanders =
      MapEditor::derive_mission_commanders(map, mission);

  ASSERT_EQ(commanders.size(), 3);

  const auto* player = commander_for_owner(commanders, 1);
  ASSERT_NE(player, nullptr);
  EXPECT_EQ(player->troop_type, QStringLiteral("carthage_elephant_master"));
  EXPECT_TRUE(player->authored_in_map);

  const auto* first_ai = commander_for_owner(commanders, 2);
  ASSERT_NE(first_ai, nullptr);
  EXPECT_EQ(first_ai->troop_type, QStringLiteral("roman_legion_organizer"));
  EXPECT_FALSE(first_ai->authored_in_map);

  const auto* second_ai = commander_for_owner(commanders, 3);
  ASSERT_NE(second_ai, nullptr);
  EXPECT_EQ(second_ai->troop_type, QStringLiteral("roman_veteran_consul"));
  EXPECT_FALSE(second_ai->authored_in_map);

  for (const auto& commander : commanders) {
    if (commander.authored_in_map) {
      continue;
    }
    bool near_owner_spawn = false;
    for (const auto& spawn : map.troop_spawns()) {
      if (spawn.player_id != commander.owner_id) {
        continue;
      }
      const double dx = spawn.x - commander.position.x();
      const double dz = spawn.z - commander.position.y();
      if ((dx * dx) + (dz * dz) <= 12.0 * 12.0) {
        near_owner_spawn = true;
        break;
      }
    }
    EXPECT_TRUE(near_owner_spawn)
        << "owner " << commander.owner_id << " commander is placed away from its army";
  }
}

TEST(MapEditorCommanderPreviewTest, OnlyOneCommanderPerOwnerIsTracked) {
  MapEditor::MapData map;

  MapEditor::TroopSpawnElement commander;
  commander.type = QStringLiteral("carthage_elephant_master");
  commander.player_id = 1;
  commander.x = 10.0F;
  commander.z = 10.0F;
  map.add_troop_spawn(commander);

  MapEditor::TroopSpawnElement archer;
  archer.type = QStringLiteral("archer");
  archer.player_id = 2;
  map.add_troop_spawn(archer);

  EXPECT_TRUE(MapEditor::MapData::is_commander_troop_type(
      QStringLiteral("roman_field_commander")));
  EXPECT_FALSE(MapEditor::MapData::is_commander_troop_type(QStringLiteral("archer")));
  EXPECT_EQ(map.commander_spawn_index_for_player(1), 0);
  EXPECT_EQ(map.commander_spawn_index_for_player(2), -1);
}
