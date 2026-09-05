#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/map/campaign_definition.h"
#include "game/map/campaign_loader.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/resource_types.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "utils/resource_utils.h"

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
    for (const auto value : defeat_conditions) {
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

TEST(MissionAssetRulesTest, EveryMissionOpensAndClosesInACommandersVoice) {
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  ASSERT_TRUE(missions_dir.exists()) << missions_dir.path().toStdString();

  const QStringList files = missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  for (const QString& file_name : files) {
    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        missions_dir.absoluteFilePath(file_name), mission, &error))
        << file_name.toStdString() << ": " << error.toStdString();

    bool has_start = false;
    bool has_victory = false;
    bool has_defeat = false;
    for (const auto& message : mission.commander_messages) {
      switch (message.trigger) {
      case Game::Mission::CommanderMessageTrigger::MissionStart:
        has_start = true;
        break;
      case Game::Mission::CommanderMessageTrigger::MissionVictory:
        has_victory = true;
        break;
      case Game::Mission::CommanderMessageTrigger::MissionDefeat:
        has_defeat = true;
        break;
      default:
        break;
      }
    }

    EXPECT_TRUE(has_start) << file_name.toStdString() << ": no mission_start line";
    EXPECT_TRUE(has_victory) << file_name.toStdString() << ": no mission_victory line";
    EXPECT_TRUE(has_defeat) << file_name.toStdString() << ": no mission_defeat line";
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
    for (const auto value : defeat_conditions) {
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
    for (const auto value : spawns) {
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
  for (const auto value : spawns) {
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
    for (const auto value : root.value("structures").toArray()) {
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
  EXPECT_EQ(zama.size(), 2);
  EXPECT_EQ(zama.value(QStringLiteral("barracks")), 1);
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
      authored_yield.add(
          Game::Systems::ResourceType::Wood,
          Game::Systems::harvest_yield(Game::Systems::ResourceType::Wood));
    } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
      authored_yield.add(
          Game::Systems::ResourceType::Stone,
          Game::Systems::harvest_yield(Game::Systems::ResourceType::Stone));
    } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
      authored_yield.add(
          Game::Systems::ResourceType::Iron,
          Game::Systems::harvest_yield(Game::Systems::ResourceType::Iron));
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
    for (const auto value : map.value("structures").toArray()) {
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

TEST(MissionAssetRulesTest, DecapitationObjectivesHaveCommandersToKill) {
  Game::Campaign::CampaignDefinition campaign;
  QString campaign_error;
  ASSERT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("campaigns/second_punic_war.json")),
      campaign,
      &campaign_error))
      << campaign_error.toStdString();

  for (const auto& entry : campaign.missions) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        asset_dir_path(QStringLiteral("missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << mission_error.toStdString();

    const bool wants_decapitation =
        std::any_of(mission.victory_conditions.begin(),
                    mission.victory_conditions.end(),
                    [](const Game::Mission::Condition& condition) {
                      return condition.type == QStringLiteral("eliminate_commanders");
                    });
    if (!wants_decapitation) {
      continue;
    }

    ASSERT_FALSE(mission.ai_setups.empty())
        << entry.mission_id.toStdString()
        << " asks the player to kill commanders but ships no opponents";
    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(mission.map_path), map, &map_error))
        << mission.map_path.toStdString() << ": " << map_error.toStdString();

    for (std::size_t index = 0; index < mission.ai_setups.size(); ++index) {
      const int owner_id = static_cast<int>(index) + 2;
      const bool has_commander = std::any_of(
          map.spawns.begin(),
          map.spawns.end(),
          [owner_id](const Game::Map::UnitSpawn& spawn) {
            if (spawn.player_id != owner_id) {
              return false;
            }
            const auto troop = Game::Units::spawn_typeToTroopType(spawn.type);
            return troop.has_value() && Game::Units::is_commander_troop(*troop);
          });
      EXPECT_TRUE(has_commander)
          << entry.mission_id.toStdString() << " AI "
          << mission.ai_setups[index].id.toStdString()
          << " has no commander to kill, so eliminate_commanders can never arm";
    }
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

TEST(MissionAssetRulesTest, TutorialMissionIsFullyAuthored) {
  Game::Mission::MissionDefinition mission;
  QString mission_error;
  ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
      asset_dir_path(QStringLiteral("missions/tutorial.json")),
      mission,
      &mission_error))
      << mission_error.toStdString();

  EXPECT_TRUE(mission.tutorial) << "the tutorial mission must flag itself so the "
                                   "engine attaches the guided steps";
  ASSERT_EQ(mission.ai_setups.size(), 1U);
  EXPECT_FALSE(mission.ai_setups.front().waves.empty())
      << "the defend step waits for a raid that never comes";
  EXPECT_TRUE(std::any_of(mission.victory_conditions.begin(),
                          mission.victory_conditions.end(),
                          [](const Game::Mission::Condition& condition) {
                            return condition.type ==
                                   QStringLiteral("eliminate_commanders");
                          }));

  Game::Map::MapDefinition map;
  QString map_error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      Utils::Resources::resolve_resource_path(mission.map_path), map, &map_error))
      << map_error.toStdString();

  const QJsonObject raw_map =
      load_json_object(asset_dir_path(QStringLiteral("maps/map_tutorial.json")));
  EXPECT_TRUE(raw_map.value(QStringLiteral("skirmish_hidden")).toBool())
      << "the tutorial stage must not be offered as a skirmish map";

  int player_commanders = 0;
  int enemy_commanders = 0;
  int held_enemy_scouts = 0;
  for (const auto& spawn : map.spawns) {
    const auto troop = Game::Units::spawn_typeToTroopType(spawn.type);
    const bool commander = troop.has_value() && Game::Units::is_commander_troop(*troop);
    if (spawn.player_id == 1 && commander) {
      ++player_commanders;
    }
    if (spawn.player_id == 2 && commander) {
      ++enemy_commanders;
    }
    if (spawn.player_id == 2 && !commander &&
        spawn.behavior == QStringLiteral("hold") && spawn.z > 50.0F) {
      ++held_enemy_scouts;
    }
  }
  EXPECT_EQ(player_commanders, 1);
  EXPECT_EQ(enemy_commanders, 1);
  EXPECT_GE(held_enemy_scouts, 2) << "the attack step asks for two kills near the camp";

  int trees = 0;
  int boulders = 0;
  int ore = 0;
  for (const auto& prop : map.world_props) {
    trees += Game::Map::is_tree_world_prop_type(prop.type) ? 1 : 0;
    boulders += Game::Map::is_boulder_world_prop_type(prop.type) ? 1 : 0;
    ore += Game::Map::is_iron_ore_world_prop_type(prop.type) ? 1 : 0;
  }
  EXPECT_GE(trees, 6);
  EXPECT_GE(boulders, 3);
  EXPECT_GE(ore, 3);

  const bool player_barracks =
      std::any_of(map.structures.begin(),
                  map.structures.end(),
                  [](const Game::Map::StructureEntry& structure) {
                    return structure.player_id == 1 &&
                           structure.type == Game::Units::SpawnType::Barracks;
                  });
  EXPECT_TRUE(player_barracks) << "gathered loads need a yard to be dropped on";
}

TEST(MissionAssetRulesTest, GatherObjectivesFitTheHarvestActuallyOnTheMap) {
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  ASSERT_TRUE(missions_dir.exists()) << missions_dir.path().toStdString();

  const QStringList files = missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  int gather_missions = 0;
  for (const QString& file_name : files) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        missions_dir.absoluteFilePath(file_name), mission, &mission_error))
        << mission_error.toStdString();

    Game::Systems::ResourceAmounts wanted;
    bool has_gather_objective = false;
    const auto collect = [&wanted, &has_gather_objective](
                             const std::vector<Game::Mission::Condition>& conditions) {
      for (const auto& condition : conditions) {
        if (condition.type != QStringLiteral("accumulate_resources") ||
            !condition.resources.has_value()) {
          continue;
        }
        has_gather_objective = true;
        for (Game::Systems::ResourceType const type :
             Game::Systems::k_all_resource_types) {
          wanted.set(type, std::max(wanted.get(type), condition.resources->get(type)));
        }
      }
    };
    collect(mission.victory_conditions);
    collect(mission.optional_objectives);
    if (!has_gather_objective) {
      continue;
    }
    ++gather_missions;

    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(mission.map_path), map, &map_error))
        << file_name.toStdString() << ": " << map_error.toStdString();

    Game::Map::TerrainService::instance().initialize(map);
    Game::Systems::ResourceAmounts on_the_map;
    for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
      if (Game::Map::is_tree_world_prop_type(prop.type)) {
        on_the_map.add(Game::Systems::ResourceType::Wood,
                       Game::Systems::harvest_yield(Game::Systems::ResourceType::Wood));
      } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
        on_the_map.add(
            Game::Systems::ResourceType::Stone,
            Game::Systems::harvest_yield(Game::Systems::ResourceType::Stone));
      } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
        on_the_map.add(Game::Systems::ResourceType::Iron,
                       Game::Systems::harvest_yield(Game::Systems::ResourceType::Iron));
      }
    }
    Game::Map::TerrainService::instance().clear();

    Game::Systems::ResourceAmounts stock = map.starting_resources;
    mission.player_setup.starting_resources.apply_to(stock);

    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      const int required = wanted.get(type);
      if (required <= 0) {
        continue;
      }
      const int reachable = on_the_map.get(type) + stock.get(type);
      EXPECT_GE(reachable, required * 5 / 4)
          << file_name.toStdString() << " asks for " << required << " "
          << Game::Systems::resource_type_key(type)
          << " but the map and the starting stores only hold " << reachable;
    }
  }

  EXPECT_GT(gather_missions, 0);
}

TEST(MissionAssetRulesTest, EveryCampaignMissionSpeaksOnAtLeastSixBeats) {

  const QString campaign_path =
      asset_dir_path(QStringLiteral("campaigns/second_punic_war.json"));
  QFile campaign_file(campaign_path);
  ASSERT_TRUE(campaign_file.open(QIODevice::ReadOnly)) << campaign_path.toStdString();
  const QJsonArray campaign_missions =
      QJsonDocument::fromJson(campaign_file.readAll()).object()["missions"].toArray();
  ASSERT_FALSE(campaign_missions.isEmpty());

  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  for (const auto entry : campaign_missions) {
    const QString mission_id = entry.toObject()["mission_id"].toString();
    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        missions_dir.absoluteFilePath(mission_id + QStringLiteral(".json")),
        mission,
        &error))
        << mission_id.toStdString() << ": " << error.toStdString();

    QSet<int> triggers;
    bool has_placed_capture = false;
    bool has_wave_line = false;
    bool has_player_commander_fallen = false;
    bool has_last_stand = false;
    for (const auto& message : mission.commander_messages) {
      triggers.insert(static_cast<int>(message.trigger));
      using Game::Mission::CommanderMessageTrigger;
      switch (message.trigger) {
      case CommanderMessageTrigger::StructureCaptured:
        has_placed_capture = has_placed_capture ||
                             message.condition.owner_id.has_value() ||
                             message.condition.at.has_value();
        break;
      case CommanderMessageTrigger::WaveIncoming:
        has_wave_line = true;
        break;
      case CommanderMessageTrigger::CommanderDefeated:
        has_player_commander_fallen =
            has_player_commander_fallen || message.condition.owner_is_local;
        break;
      case CommanderMessageTrigger::NearDefeat:
        has_last_stand = true;
        break;
      default:
        break;
      }
    }

    bool takes_camps = false;
    for (const auto& condition : mission.victory_conditions) {
      takes_camps =
          takes_camps || condition.type == QStringLiteral("capture_structures");
    }
    bool has_waves = false;
    for (const auto& ai : mission.ai_setups) {
      has_waves = has_waves || !ai.waves.empty();
    }

    EXPECT_GE(triggers.size(), 6)
        << mission_id.toStdString() << " answers too few beats in a commander's voice";
    if (takes_camps) {
      EXPECT_TRUE(has_placed_capture)
          << mission_id.toStdString()
          << " takes camps but no commander speaks of losing one";
    }
    if (has_waves) {
      EXPECT_TRUE(has_wave_line)
          << mission_id.toStdString() << " marches waves nobody announces";
    }
    EXPECT_TRUE(has_player_commander_fallen)
        << mission_id.toStdString()
        << " has no line for the player's commander falling";
    EXPECT_TRUE(has_last_stand) << mission_id.toStdString()
                                << " has no commander speaking at his own last stand";
  }
}

TEST(MissionAssetRulesTest, EveryCommanderMessageIsSpokenBySomebodyOnTheField) {
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  ASSERT_TRUE(missions_dir.exists()) << missions_dir.path().toStdString();

  const QStringList files = missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  int checked = 0;
  for (const QString& file_name : files) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        missions_dir.absoluteFilePath(file_name), mission, &mission_error))
        << mission_error.toStdString();
    if (mission.commander_messages.empty()) {
      continue;
    }

    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(mission.map_path), map, &map_error))
        << file_name.toStdString() << ": " << map_error.toStdString();

    QSet<QString> fielded;
    for (const auto& spawn : map.spawns) {
      const auto troop_type = Game::Units::spawn_typeToTroopType(spawn.type);
      if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
        continue;
      }
      fielded.insert(Game::Units::troop_typeToQString(*troop_type));
    }

    for (const auto& message : mission.commander_messages) {
      if (message.speaker.isEmpty()) {
        continue;
      }
      ++checked;
      EXPECT_TRUE(fielded.contains(message.speaker))
          << file_name.toStdString() << ": message " << message.id.toStdString()
          << " is spoken by " << message.speaker.toStdString()
          << ", who never takes the field on " << mission.map_path.toStdString()
          << " -- the panel shows a portrait nobody is fighting";
    }
  }

  EXPECT_GT(checked, 0) << "no shipped mission speaks in a commander's voice any more";
}
