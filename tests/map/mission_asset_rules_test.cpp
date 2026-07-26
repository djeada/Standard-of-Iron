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

TEST(MissionAssetRulesTest, CrossingRhoneUsesFortifiedSettlements) {
  const QJsonObject root =
      load_json_object(asset_dir_path(QStringLiteral("maps/map_crossing_rhone.json")));
  const QJsonArray structures = root.value("structures").toArray();
  const QJsonArray props = root.value("world_props").toArray();

  struct Holding {
    int barracks = 0;
    int towers = 0;
    int homes = 0;
    int marketplaces = 0;
    int wall_lines = 0;
  };

  QHash<int, Holding> holdings;
  for (const auto& value : structures) {
    const QJsonObject structure = value.toObject();
    Holding& holding = holdings[structure.value("player_id").toInt()];
    const QString type = structure.value("type").toString();
    holding.barracks += type == QStringLiteral("barracks") ? 1 : 0;
    holding.towers += type == QStringLiteral("defense_tower") ? 1 : 0;
    holding.homes += type == QStringLiteral("home") ? 1 : 0;
    holding.marketplaces += type == QStringLiteral("marketplace") ? 1 : 0;
    holding.wall_lines += type == QStringLiteral("wall_segment") ? 1 : 0;
  }

  // The player's camp used to be walls and props with no buildings at all, so
  // the player began the campaign unable to produce anything.
  const Holding& player = holdings[1];
  EXPECT_EQ(player.barracks, 1) << "the player must start with a working barracks";
  EXPECT_GE(player.homes, 4);
  EXPECT_GE(player.wall_lines, 3) << "the marching camp keeps its palisade";

  // Every settlement of every tier is garrisoned, inhabited and fortified. A
  // marketplace belongs to the fortified-camp and town tiers, not to a marching
  // camp, so it is checked on the town below rather than on all of them.
  for (const int bot_id : {2, 3}) {
    const Holding& holding = holdings[bot_id];
    EXPECT_EQ(holding.barracks, 1) << "player " << bot_id;
    EXPECT_GE(holding.towers, 2) << "player " << bot_id;
    EXPECT_GE(holding.homes, 5)
        << "player " << bot_id << " should hold an inhabited settlement";
    EXPECT_GE(holding.wall_lines, 4) << "player " << bot_id;
  }

  // One settlement on the map is a full town: walled, with an inner citadel and
  // streets of housing rather than a single row.
  int densest = 0;
  int town_marketplaces = 0;
  for (const auto& holding : holdings) {
    if (holding.homes > densest) {
      densest = holding.homes;
      town_marketplaces = holding.marketplaces;
    }
  }
  EXPECT_GE(densest, 40) << "no settlement on the map reads as a town";
  EXPECT_GE(town_marketplaces, 1) << "the town has no market";

  bool has_tent = false;
  bool has_firecamp = false;
  bool has_weapon_rack = false;
  bool has_supply_cart = false;
  for (const auto& value : props) {
    const QJsonObject prop = value.toObject();
    const double x = prop.value("x").toDouble();
    const double z = prop.value("z").toDouble();
    if (x < 8.0 || x > 54.0 || z < 136.0 || z > 212.0) {
      continue;
    }
    const QString type = prop.value("type").toString();
    has_tent = has_tent || type == QStringLiteral("tent");
    has_firecamp = has_firecamp || type == QStringLiteral("firecamp");
    has_weapon_rack = has_weapon_rack || type == QStringLiteral("weapon_rack");
    has_supply_cart = has_supply_cart || type == QStringLiteral("supply_cart");
  }

  EXPECT_TRUE(has_tent);
  EXPECT_TRUE(has_firecamp);
  EXPECT_TRUE(has_weapon_rack);
  EXPECT_TRUE(has_supply_cart);
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

  // Authored yields only. Procedural scatter adds more on top, so the objective
  // must be reachable without depending on it.
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
    // Slack so a few unreachable or contested nodes cannot soft-lock the mission.
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

TEST(MissionAssetRulesTest, CaptureObjectivesMatchEnemyOwnedBarracks) {
  struct Expectation {
    const char* mission_id;
    int ai_count;
    int player_barracks;
    int enemy_barracks;
    int capture_target;
  };

  // AI setups take owner ids 2, 3, 4..., so a map barracks tagged past the last AI
  // would belong to nobody and could never satisfy a capture objective.
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

namespace {

constexpr const char* k_campaign_maps[] = {
    "map_crossing_rhone.json",
    "map_crossing_alps.json",
    "map_battle_ticino.json",
    "map_battle_trebia.json",
    "map_battle_trasimene.json",
    "map_battle_cannae.json",
    "map_campania_campaign.json",
    "map_battle_zama.json",
};

struct PlacementFailure {
  QString map_name;
  QString structure_type;
  int player_id;
  float world_x;
  float world_z;
};

// A building whose footprint overlaps unwalkable ground is stranded: units cannot
// reach it, and on a hill it means the structure hangs off the crown onto a slope.
auto find_stranded_structures(const Game::Map::MapDefinition& map,
                              const QString& map_name)
    -> std::vector<PlacementFailure> {
  std::vector<PlacementFailure> failures;

  Game::Map::TerrainHeightMap height_map(
      map.grid.width, map.grid.height, map.grid.tile_size);
  height_map.build_from_features(map.terrain);

  const float half_width = map.grid.width * 0.5F - 0.5F;
  const float half_height = map.grid.height * 0.5F - 0.5F;

  for (const auto& structure : map.structures) {
    const auto* point =
        std::get_if<Game::Map::PointStructureGeometry>(&structure.geometry);
    if (point == nullptr) {
      continue;
    }

    const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(
        Game::Units::spawn_typeToString(structure.type));

    const float grid_x = point->position.x() / map.grid.tile_size + half_width;
    const float grid_z = point->position.z() / map.grid.tile_size + half_height;
    const int half_cells_x =
        std::max(0, static_cast<int>(size.width * 0.5F / map.grid.tile_size));
    const int half_cells_z =
        std::max(0, static_cast<int>(size.depth * 0.5F / map.grid.tile_size));

    bool stranded = false;
    for (int dz = -half_cells_z; dz <= half_cells_z && !stranded; ++dz) {
      for (int dx = -half_cells_x; dx <= half_cells_x && !stranded; ++dx) {
        const int cell_x = static_cast<int>(std::lround(grid_x)) + dx;
        const int cell_z = static_cast<int>(std::lround(grid_z)) + dz;
        if (!height_map.is_walkable(cell_x, cell_z)) {
          stranded = true;
        }
      }
    }

    if (stranded) {
      failures.push_back(
          {map_name,
           QString::fromStdString(Game::Units::spawn_typeToString(structure.type)),
           structure.player_id,
           point->position.x(),
           point->position.z()});
    }
  }

  return failures;
}

} // namespace

TEST(MissionAssetRulesTest, CampaignStructuresStandOnWalkableGround) {
  std::vector<PlacementFailure> all_failures;

  for (const char* map_file : k_campaign_maps) {
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        asset_dir_path(QStringLiteral("maps/%1").arg(map_file)), map, &error))
        << error.toStdString();

    auto failures = find_stranded_structures(map, QString::fromUtf8(map_file));
    all_failures.insert(all_failures.end(), failures.begin(), failures.end());
  }

  QStringList report;
  for (const auto& failure : all_failures) {
    report.append(QStringLiteral("%1: %2 (player %3) at %4,%5")
                      .arg(failure.map_name)
                      .arg(failure.structure_type)
                      .arg(failure.player_id)
                      .arg(failure.world_x)
                      .arg(failure.world_z));
  }

  EXPECT_TRUE(all_failures.empty())
      << all_failures.size() << " stranded structure(s):\n"
      << report.join('\n').toStdString();
}
