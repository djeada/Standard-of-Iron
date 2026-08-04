#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/wildlife/wildlife_config.h"

namespace {

auto load_map(const QJsonObject& root, Game::Map::MapDefinition& out) -> bool {
  QTemporaryFile temp_file;
  if (!temp_file.open()) {
    return false;
  }
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();
  QString error;
  bool const loaded =
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), out, &error);
  if (!loaded) {
    ADD_FAILURE() << error.toStdString();
  }
  return loaded;
}

auto base_root() -> QJsonObject {
  return QJsonObject{
      {"name", "Wildlife Test"},
      {"grid", QJsonObject{{"width", 41}, {"height", 41}, {"tile_size", 1.0}}}};
}

} // namespace

TEST(MapWildlifeConfigTest, MapsWithoutWildlifeKeepTheFeatureOff) {
  Game::Map::MapDefinition map;
  ASSERT_TRUE(load_map(base_root(), map));
  EXPECT_FALSE(map.wildlife.enabled);
}

TEST(MapWildlifeConfigTest, ParsesPerSpeciesPopulationAndBehaviourKnobs) {
  QJsonObject root = base_root();
  root["wildlife"] = QJsonObject{
      {"enabled", true},
      {"seed", 1234},
      {"sheep",
       QJsonObject{{"enabled", true},
                   {"groups", 3},
                   {"group_size_min", 4},
                   {"group_size_max", 7},
                   {"roam_radius", 11.0},
                   {"move_speed", 0.9},
                   {"flee_speed", 3.6},
                   {"alert_radius", 12.0},
                   {"respawn", false},
                   {"respawn_delay", 33.0}}},
      {"wolves",
       QJsonObject{{"enabled", true},
                   {"groups", 2},
                   {"group_size_min", 3},
                   {"group_size_max", 3},
                   {"aggression", 0.8}}},
      {"birds", QJsonObject{{"enabled", true}, {"groups", 4}, {"flight_height", 9.5}}}};

  Game::Map::MapDefinition map;
  ASSERT_TRUE(load_map(root, map));

  EXPECT_TRUE(map.wildlife.enabled);
  EXPECT_EQ(map.wildlife.seed, 1234U);

  EXPECT_TRUE(map.wildlife.sheep.enabled);
  EXPECT_EQ(map.wildlife.sheep.group_count, 3);
  EXPECT_EQ(map.wildlife.sheep.group_size_min, 4);
  EXPECT_EQ(map.wildlife.sheep.group_size_max, 7);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.roam_radius, 11.0F);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.alert_radius, 12.0F);
  EXPECT_FALSE(map.wildlife.sheep.respawn);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.respawn_delay, 33.0F);

  EXPECT_TRUE(map.wildlife.wolves.enabled);
  EXPECT_EQ(map.wildlife.wolves.group_count, 2);
  EXPECT_FLOAT_EQ(map.wildlife.wolves.aggression, 0.8F);

  EXPECT_TRUE(map.wildlife.birds.enabled);
  EXPECT_EQ(map.wildlife.birds.group_count, 4);
  EXPECT_FLOAT_EQ(map.wildlife.birds.flight_height, 9.5F);
}

TEST(MapWildlifeConfigTest, SpeciesOmittedFromTheBlockStayDisabled) {
  QJsonObject root = base_root();
  root["wildlife"] = QJsonObject{
      {"enabled", true}, {"sheep", QJsonObject{{"enabled", true}, {"groups", 1}}}};

  Game::Map::MapDefinition map;
  ASSERT_TRUE(load_map(root, map));

  EXPECT_TRUE(map.wildlife.sheep.enabled);
  EXPECT_FALSE(map.wildlife.wolves.enabled);
  EXPECT_FALSE(map.wildlife.birds.enabled);
}

TEST(MapWildlifeConfigTest, GridSpawnAreasBecomeWorldCoordinates) {
  QJsonObject root = base_root();
  root["wildlife"] = QJsonObject{
      {"enabled", true},
      {"sheep",
       QJsonObject{{"enabled", true},
                   {"groups", 1},
                   {"spawn_areas",
                    QJsonArray{QJsonObject{{"x", 20}, {"z", 20}, {"radius", 6}},
                               QJsonObject{{"x", 0}, {"z", 0}, {"radius", 4}}}}}}};

  Game::Map::MapDefinition map;
  ASSERT_TRUE(load_map(root, map));

  ASSERT_EQ(map.wildlife.sheep.spawn_areas.size(), 2U);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.spawn_areas[0].x, 0.0F);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.spawn_areas[0].z, 0.0F);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.spawn_areas[0].radius, 6.0F);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.spawn_areas[1].x, -20.0F);
  EXPECT_FLOAT_EQ(map.wildlife.sheep.spawn_areas[1].z, -20.0F);
}

TEST(MapWildlifeConfigTest, OutOfRangeValuesAreClamped) {
  QJsonObject root = base_root();
  root["wildlife"] = QJsonObject{{"enabled", true},
                                 {"wolves",
                                  QJsonObject{{"enabled", true},
                                              {"groups", 5000},
                                              {"group_size_min", 9},
                                              {"group_size_max", 2},
                                              {"aggression", 12.0},
                                              {"roam_radius", -4.0}}}};

  Game::Map::MapDefinition map;
  ASSERT_TRUE(load_map(root, map));

  EXPECT_LE(map.wildlife.wolves.group_count, 64);
  EXPECT_GE(map.wildlife.wolves.group_size_max, map.wildlife.wolves.group_size_min);
  EXPECT_FLOAT_EQ(map.wildlife.wolves.aggression, 1.0F);
  EXPECT_GT(map.wildlife.wolves.roam_radius, 0.0F);
}
