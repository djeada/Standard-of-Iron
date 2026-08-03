#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <cmath>
#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/undead_shrine_placement.h"
#include "game/systems/building_collision_registry.h"

namespace {

auto maps_dir() -> QDir {
  return QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QStringLiteral("../../assets/maps"));
}

auto make_open_field_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = 32;
  map_definition.grid.height = 32;
  map_definition.grid.tile_size = 1.0F;

  Game::Map::UndeadZone zone;
  zone.id = QStringLiteral("zone");
  zone.x = 16.0F;
  zone.z = 16.0F;
  zone.radius = 6.0F;
  map_definition.undead_zones.push_back(zone);
  return map_definition;
}

class UndeadShrinePlacementTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

  static auto plan(const Game::Map::MapDefinition& map_definition)
      -> std::vector<Game::Map::UndeadShrinePlacement> {
    auto& terrain = Game::Map::TerrainService::instance();
    terrain.initialize(map_definition);
    return Game::Map::plan_undead_zone_shrines(terrain, map_definition);
  }
};

} // namespace

TEST_F(UndeadShrinePlacementTest, AnEmptyZoneCentreTakesTheShrineItself) {
  const auto placements = plan(make_open_field_map());

  ASSERT_EQ(placements.size(), 1U);
  EXPECT_TRUE(placements.front().placed);
  EXPECT_FALSE(placements.front().moved_off_center);
  EXPECT_NEAR(placements.front().world_position.x(), 0.5F, 0.01F);
  EXPECT_NEAR(placements.front().world_position.z(), 0.5F, 0.01F);
}

TEST_F(UndeadShrinePlacementTest,
       AnAuthoredShrineInTheZoneIsAdoptedRatherThanDuplicated) {
  Game::Map::MapDefinition map_definition = make_open_field_map();
  Game::Map::WorldProp shrine;
  shrine.type = Game::Map::WorldProp::Type::MagicShrine;
  shrine.x = 18.0F;
  shrine.z = 16.0F;
  map_definition.world_props.push_back(shrine);

  const auto placements = plan(map_definition);

  ASSERT_EQ(placements.size(), 1U);
  EXPECT_TRUE(placements.front().placed);
  EXPECT_TRUE(placements.front().adopted_existing_prop);
  EXPECT_NE(placements.front().prop_id, 0U);
  EXPECT_NEAR(placements.front().world_position.x(), 2.5F, 0.01F);
}

TEST_F(UndeadShrinePlacementTest, ABlockedCentreSendsTheShrineToTheNearestClearGround) {
  Game::Map::MapDefinition map_definition = make_open_field_map();
  Game::Map::WorldProp boulder;
  boulder.type = Game::Map::WorldProp::Type::Boulder;
  boulder.x = 16.0F;
  boulder.z = 16.0F;
  map_definition.world_props.push_back(boulder);

  const auto placements = plan(map_definition);

  ASSERT_EQ(placements.size(), 1U);
  ASSERT_TRUE(placements.front().placed);
  EXPECT_TRUE(placements.front().moved_off_center);

  const float dx = placements.front().world_position.x() - 0.5F;
  const float dz = placements.front().world_position.z() - 0.5F;
  const float distance = std::sqrt(dx * dx + dz * dz);
  EXPECT_GT(distance, Game::Map::k_undead_shrine_clearance);
  EXPECT_LT(distance, map_definition.undead_zones.front().radius);
}

TEST_F(UndeadShrinePlacementTest, TwoZonesSharingGroundStillGetOneShrineEach) {
  Game::Map::MapDefinition map_definition = make_open_field_map();
  auto second_zone = map_definition.undead_zones.front();
  second_zone.id = QStringLiteral("zone_2");
  second_zone.x = 17.0F;
  map_definition.undead_zones.push_back(second_zone);

  const auto placements = plan(map_definition);

  ASSERT_EQ(placements.size(), 2U);
  EXPECT_TRUE(placements[0].placed);
  EXPECT_TRUE(placements[1].placed);

  const float dx = placements[0].world_position.x() - placements[1].world_position.x();
  const float dz = placements[0].world_position.z() - placements[1].world_position.z();
  EXPECT_GT(std::sqrt(dx * dx + dz * dz), Game::Map::k_undead_shrine_clearance * 2.0F)
      << "neighbouring zones must not stack their shrines";
}

TEST_F(UndeadShrinePlacementTest, AZoneWithNoDryGroundReportsAPlacementFailure) {
  Game::Map::MapDefinition map_definition = make_open_field_map();
  map_definition.lakes.push_back(Game::Map::Lake{
      .center = QVector3D(0.5F, 0.0F, 0.5F), .width = 40.0F, .depth = 40.0F});

  const auto placements = plan(map_definition);

  ASSERT_EQ(placements.size(), 1U);
  EXPECT_FALSE(placements.front().placed);
}

TEST_F(UndeadShrinePlacementTest, EveryShippedUndeadZoneCanPlaceItsShrine) {
  const QDir dir = maps_dir();
  ASSERT_TRUE(dir.exists()) << dir.path().toStdString();

  const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  int checked_zones = 0;
  for (const QString& file_name : files) {
    Game::Map::MapDefinition map_definition;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        dir.absoluteFilePath(file_name), map_definition, &error))
        << file_name.toStdString() << ": " << error.toStdString();

    if (map_definition.undead_zones.empty()) {
      continue;
    }

    Game::Systems::BuildingCollisionRegistry::instance().clear();
    const auto placements = plan(map_definition);
    ASSERT_EQ(placements.size(), map_definition.undead_zones.size());

    for (const auto& placement : placements) {
      EXPECT_TRUE(placement.placed)
          << file_name.toStdString() << ": zone '" << placement.zone_id.toStdString()
          << "' has nowhere to put a shrine";
      ++checked_zones;
    }
  }

  EXPECT_GT(checked_zones, 0) << "no shipped map declares an undead zone";
}
