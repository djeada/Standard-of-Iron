

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <vector>

#include "core/world.h"
#include "game/map/campaign_definition.h"
#include "game/map/campaign_loader.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/undead_awakening_system.h"
#include "game/wildlife/wildlife_system.h"

namespace {

auto asset_path(const QString& relative) -> QString {
  QDir dir(QDir::currentPath());
  for (int hop = 0; hop < 6; ++hop) {
    const QString candidate = dir.filePath(QStringLiteral("assets/") + relative);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QStringLiteral("assets/") + relative;
}

auto load_campaign() -> Game::Campaign::CampaignDefinition {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  EXPECT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      asset_path(QStringLiteral("campaigns/second_punic_war.json")), campaign, &error))
      << error.toStdString();
  return campaign;
}

class CampaignContentIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.set_local_player_id(1);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.initialize_defaults();
    nations.set_player_nation(1, Game::Systems::NationID::Carthage);

    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

TEST_F(CampaignContentIntegrationTest, EveryMissionsGroundStandsUpAndMeansWhatItSays) {
  const auto campaign = load_campaign();
  ASSERT_FALSE(campaign.missions.empty());

  for (const auto& entry : campaign.missions) {
    const std::string where = entry.mission_id.toStdString();

    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        asset_path(QStringLiteral("missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << where << ": " << mission_error.toStdString();

    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        asset_path(
            QStringLiteral("maps/%1").arg(QFileInfo(mission.map_path).fileName())),
        map,
        &map_error))
        << where << ": " << map_error.toStdString();

    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().initialize(map);

    ASSERT_FALSE(map.undead_zones.empty()) << where << " ships no dead zone";
    for (const auto& zone : map.undead_zones) {
      EXPECT_FALSE(zone.clear_reward.empty())
          << where << " dead zone " << zone.id.toStdString() << " pays nothing";
    }

    ASSERT_FALSE(map.forests.empty()) << where << " ships no forest";

    Game::Systems::Pathfinding pathfinding(map.grid.width, map.grid.height);
    pathfinding.update_navigation_grid();

    int forest_cells = 0;
    for (int z = 0; z < map.grid.height; ++z) {
      for (int x = 0; x < map.grid.width; ++x) {
        if (pathfinding.is_forest(x, z)) {
          forest_cells += 1;
          EXPECT_TRUE(pathfinding.is_walkable(
              x, z, Game::Systems::Pathfinding::Passability::Light))
              << where << ": forest must stay open to foot troops";
          EXPECT_FALSE(pathfinding.is_walkable(
              x, z, Game::Systems::Pathfinding::Passability::Heavy))
              << where << ": forest must stay shut to horses and siege";
        }
      }
    }
    EXPECT_GT(forest_cells, 0) << where << ": authored woods produced no forest ground";

    Engine::Core::World world;
    Game::Systems::UndeadAwakeningSystem undead;
    undead.configure(map);
    undead.update(&world, 0.1F);
    for (const auto& zone : map.undead_zones) {
      EXPECT_TRUE(undead.has_zone(zone.id))
          << where << ": dead zone " << zone.id.toStdString() << " did not configure";
    }

    Game::Wildlife::WildlifeSystem wildlife;
    wildlife.configure(map);
    wildlife.update(&world, 0.1F);
    EXPECT_TRUE(wildlife.is_enabled()) << where << ": wildlife is switched off";
    EXPECT_TRUE(wildlife.settings().sheep.enabled)
        << where << ": no herd worth raiding";
    EXPECT_TRUE(wildlife.settings().wolves.enabled) << where << ": no wolves";

    Game::Map::TerrainService::instance().clear();
  }
}

TEST_F(CampaignContentIntegrationTest, ScheduledWolfPacksBuildTowardTheLateCampaign) {
  const auto campaign = load_campaign();

  int missions_with_packs = 0;
  for (const auto& entry : campaign.missions) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        asset_path(QStringLiteral("missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << mission_error.toStdString();

    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        asset_path(
            QStringLiteral("maps/%1").arg(QFileInfo(mission.map_path).fileName())),
        map,
        &map_error))
        << map_error.toStdString();

    const auto& waves = map.wildlife.wolves.waves;
    if (waves.empty()) {
      continue;
    }
    missions_with_packs += 1;

    EXPECT_TRUE(std::is_sorted(waves.begin(),
                               waves.end(),
                               [](const Game::Wildlife::WildlifeWave& lhs,
                                  const Game::Wildlife::WildlifeWave& rhs) {
                                 return lhs.timing < rhs.timing;
                               }))
        << entry.mission_id.toStdString() << ": wolf waves are out of order";
    for (const auto& wave : waves) {
      EXPECT_GT(wave.timing, 0.0F)
          << entry.mission_id.toStdString() << ": a wave at t=0 is a spawn area";
      EXPECT_GT(wave.pack_size, 0)
          << entry.mission_id.toStdString() << ": an empty pack";
    }
  }

  EXPECT_GE(missions_with_packs, 5)
      << "scheduled wolf packs are meant to be a campaign-wide pressure, not a one-off";
}

} // namespace

TEST_F(CampaignContentIntegrationTest, EveryAuthoredSettlementStandsOnGroundItCanHold) {
  const auto campaign = load_campaign();
  ASSERT_FALSE(campaign.missions.empty());

  int authored_maps = 0;
  for (const auto& entry : campaign.missions) {
    const std::string where = entry.mission_id.toStdString();

    Game::Mission::MissionDefinition mission;
    QString mission_error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        asset_path(QStringLiteral("missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << where << ": " << mission_error.toStdString();

    const QString map_file = QFileInfo(mission.map_path).fileName();
    Game::Map::MapDefinition map;
    QString map_error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        asset_path(QStringLiteral("maps/%1").arg(map_file)), map, &map_error))
        << where << ": " << map_error.toStdString();

    QFile source(asset_path(QStringLiteral("maps/%1").arg(map_file)));
    ASSERT_TRUE(source.open(QIODevice::ReadOnly)) << where;
    const QJsonObject root = QJsonDocument::fromJson(source.readAll()).object();

    std::map<int, std::vector<QPointF>> authored_by_player;
    for (const auto& value : root.value(QStringLiteral("structures")).toArray()) {
      const QJsonObject entry_obj = value.toObject();
      if (!entry_obj.value(QStringLiteral("authored")).toBool() ||
          !entry_obj.contains(QStringLiteral("x"))) {
        continue;
      }
      authored_by_player[entry_obj.value(QStringLiteral("player_id")).toInt()]
          .emplace_back(entry_obj.value(QStringLiteral("x")).toDouble(),
                        entry_obj.value(QStringLiteral("z")).toDouble());
    }
    if (authored_by_player.empty()) {
      continue;
    }
    authored_maps += 1;

    Game::Map::TerrainHeightMap height_map(
        map.grid.width, map.grid.height, map.grid.tile_size);
    height_map.build_from_features(map.terrain);

    for (const auto& [player, spots] : authored_by_player) {
      std::vector<float> on_hill;
      for (const QPointF& spot : spots) {
        const int grid_x = static_cast<int>(std::lround(spot.x()));
        const int grid_z = static_cast<int>(std::lround(spot.y()));
        const auto type = height_map.getTerrainType(grid_x, grid_z);
        EXPECT_TRUE(type == Game::Map::TerrainType::Flat ||
                    type == Game::Map::TerrainType::Hill)
            << where << " (player " << player << "): authored building at " << grid_x
            << "," << grid_z << " stands on " << Game::Map::terrain_type_to_string(type)
            << ", which nothing can be built on";
        if (type == Game::Map::TerrainType::Hill) {
          on_hill.push_back(height_map.get_height_at_grid(grid_x, grid_z));
        }
      }

      if (on_hill.size() < 2) {
        continue;
      }
      const float highest = *std::max_element(on_hill.begin(), on_hill.end());
      const float lowest = *std::min_element(on_hill.begin(), on_hill.end());
      if (highest < 1.0F) {
        continue;
      }
      EXPECT_GT(lowest, highest * 0.88F)
          << where << " (player " << player
          << "): a hill settlement has buildings at different heights, so some are "
             "down a slope: "
          << lowest << " vs " << highest;
    }
  }

  EXPECT_GT(authored_maps, 0) << "no campaign map ships an authored settlement";
}
