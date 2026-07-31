#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain.h"
#include "game/systems/building_collision_registry.h"

namespace {

auto load_json_object(const QString& file_path) -> QJsonObject {
  QFile file(file_path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << file_path.toStdString();
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
  EXPECT_EQ(error.error, QJsonParseError::NoError)
      << file_path.toStdString() << ": " << error.errorString().toStdString();
  EXPECT_TRUE(document.isObject()) << file_path.toStdString();
  return document.object();
}

auto asset_dir_path(const QString& relative_path) -> QString {
  return QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QStringLiteral("../../assets/%1").arg(relative_path));
}

} // namespace

TEST(MissionAssetRulesTest, CurrentMissionsDeclareCommanderDefeatRules) {
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  ASSERT_TRUE(missions_dir.exists()) << missions_dir.path().toStdString();

  const QStringList files = missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  for (const QString& file_name : files) {
    const QJsonObject root = load_json_object(missions_dir.absoluteFilePath(file_name));
    const QJsonArray defeat_conditions = root.value("defeat_conditions").toArray();

    bool has_lose_commander = false;
    bool has_only_commander_remaining = false;
    for (const auto& value : defeat_conditions) {
      const QString type = value.toObject().value("type").toString();
      has_lose_commander =
          has_lose_commander || type == QStringLiteral("lose_commander");
      has_only_commander_remaining = has_only_commander_remaining ||
                                     type == QStringLiteral("only_commander_remaining");
    }

    EXPECT_TRUE(has_lose_commander) << file_name.toStdString();
    EXPECT_TRUE(has_only_commander_remaining) << file_name.toStdString();
  }
}

TEST(MissionAssetRulesTest, CurrentMapsDeclareCommanderDefeatRules) {
  QDir const maps_dir(asset_dir_path(QStringLiteral("maps")));
  ASSERT_TRUE(maps_dir.exists()) << maps_dir.path().toStdString();

  const QStringList files = maps_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  for (const QString& file_name : files) {
    const QJsonObject root = load_json_object(maps_dir.absoluteFilePath(file_name));
    const QJsonObject victory = root.value("victory").toObject();
    ASSERT_FALSE(victory.isEmpty()) << file_name.toStdString();

    const QJsonArray defeat_conditions = victory.value("defeat_conditions").toArray();
    bool has_no_commander = false;
    bool has_only_commander_remaining = false;
    for (const auto& value : defeat_conditions) {
      const QString type = value.toString();
      has_no_commander = has_no_commander || type == QStringLiteral("no_commander");
      has_only_commander_remaining = has_only_commander_remaining ||
                                     type == QStringLiteral("only_commander_remaining");
    }

    EXPECT_TRUE(has_no_commander) << file_name.toStdString();
    EXPECT_TRUE(has_only_commander_remaining) << file_name.toStdString();
  }
}

TEST(MissionAssetRulesTest, CrossingRhoneUsesAuthoredLocalAiRoles) {
  const QJsonObject root =
      load_json_object(asset_dir_path(QStringLiteral("maps/map_crossing_rhone.json")));
  const QJsonArray spawns = root.value("spawns").toArray();
  ASSERT_FALSE(spawns.isEmpty());

  auto find_spawn = [&](const QString& id) -> QJsonObject {
    for (const auto& value : spawns) {
      const QJsonObject spawn = value.toObject();
      if (spawn.value("id").toString() == id) {
        return spawn;
      }
    }
    return {};
  };

  const QJsonObject hill_archer = find_spawn(QStringLiteral("rome_north_archer_01"));
  ASSERT_FALSE(hill_archer.isEmpty());
  EXPECT_EQ(hill_archer.value("behavior").toString(), QStringLiteral("hold"));

  const QJsonObject north_spearman =
      find_spawn(QStringLiteral("rome_north_spearman_01"));
  ASSERT_FALSE(north_spearman.isEmpty());
  EXPECT_EQ(north_spearman.value("behavior").toString(), QStringLiteral("guard"));
  EXPECT_DOUBLE_EQ(north_spearman.value("guard_radius").toDouble(), 14.0);

  const QJsonObject signal_guard =
      find_spawn(QStringLiteral("rome_north_signal_guard_01"));
  ASSERT_FALSE(signal_guard.isEmpty());
  EXPECT_EQ(signal_guard.value("behavior").toString(), QStringLiteral("patrol"));
  EXPECT_GE(signal_guard.value("patrol_waypoints").toArray().size(), 2);

  int local_role_count = 0;
  for (const auto& value : spawns) {
    const QJsonObject spawn = value.toObject();
    if (spawn.contains("behavior")) {
      ++local_role_count;
    }
  }
  EXPECT_GE(local_role_count, 20);
}

TEST(MissionAssetRulesTest, OffensivePlayerCampsUseAuthoredMinimalStructures) {
  auto player_structure_counts = [](const QString& map_name) {
    const QJsonObject root =
        load_json_object(asset_dir_path(QStringLiteral("maps/%1").arg(map_name)));
    QHash<QString, int> counts;
    for (const auto& value : root.value("structures").toArray()) {
      const QJsonObject structure = value.toObject();
      if (structure.value("player_id").toInt() == 1) {
        ++counts[structure.value("type").toString()];
      }
    }
    return counts;
  };

  const auto ticino = player_structure_counts(QStringLiteral("map_battle_ticino.json"));
  EXPECT_EQ(ticino.size(), 1);
  EXPECT_EQ(ticino.value(QStringLiteral("barracks")), 1);

  const auto cannae = player_structure_counts(QStringLiteral("map_battle_cannae.json"));
  EXPECT_EQ(cannae.size(), 1);
  EXPECT_EQ(cannae.value(QStringLiteral("barracks")), 1);

  const auto zama = player_structure_counts(QStringLiteral("map_battle_zama.json"));
  EXPECT_EQ(zama.size(), 3);
  EXPECT_EQ(zama.value(QStringLiteral("barracks")), 1);
  EXPECT_EQ(zama.value(QStringLiteral("defense_tower")), 2);
  EXPECT_EQ(zama.value(QStringLiteral("home")), 1);
}

TEST(MissionAssetRulesTest, AlpsMapYieldsEnoughHarvestForItsGatherObjective) {
  Game::Map::MapDefinition map;
  QString map_error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("maps/map_crossing_alps.json")), map, &map_error))
      << map_error.toStdString();

  Game::Mission::MissionDefinition mission;
  QString mission_error;
  ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("missions/crossing_the_alps.json")),
      mission,
      &mission_error))
      << mission_error.toStdString();

  const Game::Mission::Condition* gather_condition = nullptr;
  for (const auto& condition : mission.victory_conditions) {
    if (condition.type == QStringLiteral("accumulate_resources")) {
      gather_condition = &condition;
      break;
    }
  }
  ASSERT_NE(gather_condition, nullptr)
      << "crossing_the_alps is the campaign's economic mission";
  ASSERT_TRUE(gather_condition->resources.has_value());

  Game::Systems::ResourceAmounts authored_yield;
  for (const auto& prop : map.world_props) {
    if (Game::Map::is_tree_world_prop_type(prop.type)) {
      authored_yield.add(Game::Systems::ResourceType::Wood, 40);
    } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
      authored_yield.add(Game::Systems::ResourceType::Stone, 35);
    } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
      authored_yield.add(Game::Systems::ResourceType::Iron, 30);
    }
  }

  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    const int required = gather_condition->resources->get(type);
    if (required <= 0) {
      continue;
    }

    EXPECT_GE(authored_yield.get(type), required * 5 / 4)
        << "not enough authored " << Game::Systems::resource_type_key(type)
        << " on map_crossing_alps for a target of " << required;
  }

  ASSERT_FALSE(mission.player_setup.starting_units.empty());
  bool has_builder = false;
  for (const auto& unit : mission.player_setup.starting_units) {
    has_builder = has_builder || unit.type == QStringLiteral("builder");
  }
  EXPECT_TRUE(has_builder) << "a gather objective needs builders to gather with";
}

TEST(MissionAssetRulesTest, AlpsRiverHasAFlatRuntimeSurface) {
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("maps/map_crossing_alps.json")), map, &error))
      << error.toStdString();

  Game::Map::TerrainHeightMap height_map(
      map.grid.width, map.grid.height, map.grid.tile_size);
  height_map.apply_biome_variation(map.biome);
  height_map.build_from_features(map.terrain);
  height_map.add_lakes(map.lakes);
  height_map.add_river_segments(map.rivers);

  const auto& rivers = height_map.get_river_segments();
  ASSERT_FALSE(rivers.empty());
  const float surface_height = rivers.front().start.y();
  for (const auto& river : rivers) {
    EXPECT_FLOAT_EQ(river.start.y(), surface_height);
    EXPECT_FLOAT_EQ(river.end.y(), surface_height);

    const QVector3D midpoint = (river.start + river.end) * 0.5F;
    EXPECT_NEAR(height_map.get_height_at(midpoint.x(), midpoint.z()),
                surface_height - 0.10F,
                0.0001F);
  }
}

TEST(MissionAssetRulesTest, CaptureObjectivesMatchEnemyOwnedBarracks) {
  struct Expectation {
    const char* mission_id;
    int ai_count;
    int player_barracks;
    int enemy_barracks;
    int capture_target;
  };

  const Expectation expectations[] = {
      {"battle_of_ticino", 2, 1, 2, 2},
      {"battle_of_trasimene", 2, 1, 2, 2},
      {"battle_of_cannae", 3, 1, 3, 3},
      {"battle_of_zama", 4, 1, 4, 4},
  };

  for (const auto& expectation : expectations) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        asset_dir_path(QStringLiteral("missions/%1.json").arg(expectation.mission_id)),
        mission,
        &mission_error))
        << mission_error.toStdString();

    EXPECT_EQ(static_cast<int>(mission.ai_setups.size()), expectation.ai_count)
        << expectation.mission_id;

    const QJsonObject map = load_json_object(asset_dir_path(
        QStringLiteral("maps/%1").arg(QFileInfo(mission.map_path).fileName())));

    const int last_ai_owner = 1 + static_cast<int>(mission.ai_setups.size());
    int player_barracks = 0;
    int enemy_barracks = 0;
    for (const auto& ai_setup : mission.ai_setups) {
      for (const auto& building : ai_setup.starting_buildings) {
        if (building.type == QStringLiteral("barracks")) {
          enemy_barracks += 1;
        }
      }
    }
    for (const auto& value : map.value("structures").toArray()) {
      const QJsonObject structure = value.toObject();
      if (structure.value("type").toString() != QStringLiteral("barracks")) {
        continue;
      }
      const int player_id = structure.value("player_id").toInt(0);
      if (player_id == 1) {
        player_barracks += 1;
      } else if (player_id >= 2) {
        EXPECT_LE(player_id, last_ai_owner)
            << expectation.mission_id << " has an unowned barracks camp";
        if (player_id <= last_ai_owner) {
          enemy_barracks += 1;
        }
      }
    }

    EXPECT_EQ(player_barracks, expectation.player_barracks) << expectation.mission_id;
    EXPECT_EQ(enemy_barracks, expectation.enemy_barracks) << expectation.mission_id;

    int capture_target = 0;
    for (const auto& condition : mission.victory_conditions) {
      if (condition.type == QStringLiteral("capture_structures")) {
        capture_target = condition.min_count.value_or(1);
      }
    }
    EXPECT_EQ(capture_target, expectation.capture_target) << expectation.mission_id;
    EXPECT_LE(capture_target, enemy_barracks)
        << expectation.mission_id << " cannot be won";
  }
}

TEST(MissionAssetRulesTest, TrasimeneIsTheTimeBoundOffensiveMission) {
  Game::Mission::MissionDefinition mission;
  QString error;
  ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("missions/battle_of_trasimene.json")),
      mission,
      &error))
      << error.toStdString();

  const Game::Mission::Condition* time_limit = nullptr;
  for (const auto& condition : mission.defeat_conditions) {
    if (condition.type == QStringLiteral("time_limit")) {
      time_limit = &condition;
    }
  }
  ASSERT_NE(time_limit, nullptr);
  ASSERT_TRUE(time_limit->duration.has_value());
  EXPECT_FLOAT_EQ(*time_limit->duration, 1200.0F);
}
