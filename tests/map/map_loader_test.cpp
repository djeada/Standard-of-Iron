#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <cmath>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/systems/resource_types.h"

TEST(MapLoaderTest, ExpandsRiverWaypointsIntoAContinuousRuntimeChain) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonArray waypoints{QJsonArray{0, 4},
                             QJsonArray{8, 7},
                             QJsonArray{12, 12},
                             QJsonArray{20, 16}};
  const QJsonObject river{{"start", QJsonArray{0, 4}},
                          {"end", QJsonArray{20, 16}},
                          {"width", 4.0},
                          {"waypoints", waypoints}};
  const QJsonObject root{
      {"name", "River Chain Test"},
      {"grid", QJsonObject{{"width", 21}, {"height", 21}, {"tile_size", 1.0}}},
      {"rivers", QJsonArray{river}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      temp_file.fileName(), map, &error))
      << error.toStdString();
  ASSERT_EQ(map.rivers.size(), 3U);
  EXPECT_EQ(map.rivers[0].end, map.rivers[1].start);
  EXPECT_EQ(map.rivers[1].end, map.rivers[2].start);
  EXPECT_FLOAT_EQ(map.rivers[0].width, 4.0F);
}

TEST(MapLoaderTest, ParsesUndeadZonesAndWaveSpawns) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Undead Zone Test"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"world_props", QJsonArray{QJsonObject{{"type", "ruins"}, {"x", 16}, {"z", 16}}}},
      {"undead_zones",
       QJsonArray{QJsonObject{
           {"id", "sepulcher_ruin"},
           {"anchor_type", "ruins"},
           {"x", 16},
           {"z", 16},
           {"radius", 9.0},
           {"leash_radius", 14.0},
           {"owner_id", 99},
           {"team_id", 99},
           {"awaken_on", QJsonArray{"unit_enters_radius"}},
           {"waves",
            QJsonArray{
                QJsonObject{
                    {"trigger", "initial"},
                    {"units",
                     QJsonObject{{"skeleton_swordsman", 2}, {"grave_priest", 1}}}},
                QJsonObject{{"trigger", "after_clear"}, {"skeleton_archer", 3}}}}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      temp_file.fileName(), map_definition, &error))
      << error.toStdString();

  ASSERT_EQ(map_definition.undead_zones.size(), 1);
  const auto& zone = map_definition.undead_zones.front();
  EXPECT_EQ(zone.id, QStringLiteral("sepulcher_ruin"));
  EXPECT_EQ(zone.anchor_type, Game::Map::WorldProp::Type::Ruins);
  EXPECT_EQ(zone.owner_id, 99);
  EXPECT_EQ(zone.team_id, 99);
  ASSERT_EQ(zone.awaken_on.size(), 1);
  EXPECT_EQ(zone.awaken_on.front(), QStringLiteral("unit_enters_radius"));
  ASSERT_EQ(zone.waves.size(), 2);
  EXPECT_EQ(zone.waves[0].trigger, QStringLiteral("initial"));
  ASSERT_EQ(zone.waves[0].units.size(), 2);
  EXPECT_EQ(zone.waves[0].units[0].type, Game::Units::SpawnType::SkeletonSwordsman);
  EXPECT_EQ(zone.waves[0].units[0].count, 2);
  EXPECT_EQ(zone.waves[1].trigger, QStringLiteral("after_clear"));
  ASSERT_EQ(zone.waves[1].units.size(), 1);
  EXPECT_EQ(zone.waves[1].units[0].type, Game::Units::SpawnType::SkeletonArcher);
  EXPECT_EQ(zone.waves[1].units[0].count, 3);
}

TEST(MapLoaderTest, ParsesStartingResources) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Resource Test Map"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"starting_resources",
       QJsonObject{
           {"gold", 300}, {"food", 200}, {"wood", 150}, {"stone", 100}, {"iron", 75}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Gold), 300);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Food), 200);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Wood), 150);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Stone), 100);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Iron), 75);
}

TEST(MapLoaderTest, StartingResourcesDefaultToZeroWhenAbsent) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "No Resources Map"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  EXPECT_TRUE(map_def.starting_resources.empty());
}

TEST(MapLoaderTest, ExpandsRoadWaypointsIntoConnectedRuntimeSegments) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Waypoint Road Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"roads",
       QJsonArray{QJsonObject{
           {"start", QJsonArray{0.0, 0.0}},
           {"end", QJsonArray{10.0, 5.0}},
           {"waypoints",
            QJsonArray{QJsonArray{0.0, 0.0},
                       QJsonArray{5.0, 0.0},
                       QJsonArray{5.0, 5.0},
                       QJsonArray{10.0, 5.0}}},
           {"width", 2.5},
           {"style", "rough"}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.roads.size(), 3U);
  EXPECT_EQ(map_def.roads[0].start, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(map_def.roads[0].end, QVector3D(5.0F, 0.0F, 0.0F));
  EXPECT_EQ(map_def.roads[1].end, QVector3D(5.0F, 0.0F, 5.0F));
  EXPECT_EQ(map_def.roads[2].end, QVector3D(10.0F, 0.0F, 5.0F));
  for (const auto& segment : map_def.roads) {
    EXPECT_FLOAT_EQ(segment.width, 2.5F);
    EXPECT_EQ(segment.style, QStringLiteral("rough"));
  }
}

TEST(MapLoaderTest, ParsesLakesAsFirstClassWaterBodies) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Lake Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"lakes",
       QJsonArray{QJsonObject{{"x", 4.0},
                              {"z", -3.0},
                              {"width", 18.0},
                              {"depth", 10.0},
                              {"rotation", 27.0}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.lakes.size(), 1U);
  EXPECT_EQ(map_def.lakes.front().center, QVector3D(4.0F, 0.0F, -3.0F));
  EXPECT_FLOAT_EQ(map_def.lakes.front().width, 18.0F);
  EXPECT_FLOAT_EQ(map_def.lakes.front().depth, 10.0F);
  EXPECT_FLOAT_EQ(map_def.lakes.front().rotation_deg, 27.0F);
}

TEST(MapLoaderTest, TrimsFeedingRiverAtIrregularLakeBoundary) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "River Lake Join Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{-24.0, 0.0}},
                              {"end", QJsonArray{0.0, 0.0}},
                              {"width", 4.0}}}},
      {"lakes",
       QJsonArray{QJsonObject{{"x", 0.0},
                              {"z", 0.0},
                              {"width", 20.0},
                              {"depth", 20.0}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.rivers.size(), 1U);
  ASSERT_EQ(map_def.lakes.size(), 1U);
  const auto& endpoint = map_def.rivers.front().end;
  EXPECT_TRUE(Game::Map::point_on_lake_boundary(
      map_def.lakes.front(), endpoint.x(), endpoint.z(), 0.01F));
  EXPECT_GT(std::abs(endpoint.x()), 8.0F);
}

TEST(MapLoaderTest, ParsesAuthoredSpawnBehavior) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Spawn Behavior Test"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"spawns",
       QJsonArray{QJsonObject{{"type", "spearman"},
                              {"x", 12},
                              {"z", 14},
                              {"player_id", 2},
                              {"behavior", "guard"},
                              {"guard_radius", 18.0},
                              {"patrol_waypoints",
                               QJsonArray{QJsonObject{{"x", 16}, {"z", 14}},
                                          QJsonObject{{"x", 16}, {"z", 20}}}}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.spawns.size(), 1U);
  const auto& spawn = map_def.spawns.front();
  EXPECT_EQ(spawn.behavior, QStringLiteral("guard"));
  EXPECT_FLOAT_EQ(spawn.guard_radius, 18.0F);
  ASSERT_EQ(spawn.patrol_waypoints.size(), 2U);
  EXPECT_FLOAT_EQ(spawn.patrol_waypoints[0].x(), 16.0F);
  EXPECT_FLOAT_EQ(spawn.patrol_waypoints[1].z(), 20.0F);
}

TEST(MapLoaderTest, StartingResourcesPartialKeysDefaultMissingToZero) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Partial Resources Map"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"starting_resources", QJsonObject{{"gold", 500}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Gold), 500);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Food), 0);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Wood), 0);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Stone), 0);
  EXPECT_EQ(map_def.starting_resources.get(Game::Systems::ResourceType::Iron), 0);
}

TEST(MapLoaderTest, ExtendsBridgeEndpointsToSpanRiverbanks) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Bridge Span Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{0.0, -10.0}},
                              {"end", QJsonArray{0.0, 10.0}},
                              {"width", 10.0}}}},
      {"bridges",
       QJsonArray{QJsonObject{{"start", QJsonArray{-3.0, 0.0}},
                              {"end", QJsonArray{3.0, 0.0}},
                              {"width", 4.0},
                              {"height", 0.5}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.bridges.size(), 1U);
  const auto& bridge = map_def.bridges.front();
  EXPECT_LE(bridge.start.x(), -6.0F);
  EXPECT_GE(bridge.end.x(), 6.0F);
}

TEST(MapLoaderTest, ParsesHillEntranceRadiusIntoExpandedEntrancePoints) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Terrain Entrance Radius Test"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"terrain",
       QJsonArray{QJsonObject{
           {"type", "hill"},
           {"x", 16},
           {"z", 16},
           {"width", 10.0},
           {"depth", 10.0},
           {"height", 4.0},
           {"entrances",
            QJsonArray{QJsonObject{{"x", 16}, {"z", 10}, {"radius", 1.5}}}},
       }}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.terrain.size(), 1U);
  const auto& hill = map_def.terrain.front();
  EXPECT_GT(hill.entrances.size(), 1U);

  bool has_center = false;
  for (const QVector3D& entrance : hill.entrances) {
    if (entrance.x() == 0.5F && entrance.z() == -5.5F) {
      has_center = true;
      break;
    }
  }
  EXPECT_TRUE(has_center);
}

TEST(MapLoaderTest, ParsesBuildingsArrayWithOwnership) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Buildings Test"},
      {"grid", QJsonObject{{"width", 50}, {"height", 50}, {"tile_size", 1.0}}},
      {"buildings",
       QJsonArray{
           QJsonObject{{"type", "defense_tower"},
                       {"x", 10},
                       {"z", 15},
                       {"player_id", 1},
                       {"nation", "rome"}},
           QJsonObject{{"type", "home"}, {"x", 20}, {"z", 25}, {"player_id", 2}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.buildings.size(), 2U);
  const auto& tower = map_def.buildings[0];
  EXPECT_EQ(tower.type, QStringLiteral("defense_tower"));
  EXPECT_EQ(tower.player_id, 1);
  EXPECT_EQ(tower.nation, QStringLiteral("rome"));

  const auto& home = map_def.buildings[1];
  EXPECT_EQ(home.type, QStringLiteral("home"));
  EXPECT_EQ(home.player_id, 2);
}

TEST(MapLoaderTest, ParsesWallsArrayWithOwnership) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Walls Test"},
      {"grid", QJsonObject{{"width", 50}, {"height", 50}, {"tile_size", 1.0}}},
      {"walls",
       QJsonArray{QJsonObject{{"start", QJsonArray{5, 10}},
                              {"end", QJsonArray{30, 10}},
                              {"player_id", 1},
                              {"nation", "rome"}},
                  QJsonObject{{"start", QJsonArray{5, 40}},
                              {"end", QJsonArray{30, 40}},
                              {"player_id", 2}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.wall_lines.size(), 2U);
  const auto& wall0 = map_def.wall_lines[0];
  EXPECT_EQ(wall0.player_id, 1);
  EXPECT_EQ(wall0.nation, QStringLiteral("rome"));

  const auto& wall1 = map_def.wall_lines[1];
  EXPECT_EQ(wall1.player_id, 2);
}

TEST(MapLoaderTest, EmptyBuildingsAndWallsWhenArraysAbsent) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "No Buildings Map"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  EXPECT_TRUE(map_def.buildings.empty());
  EXPECT_TRUE(map_def.wall_lines.empty());
}
