#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/systems/resource_types.h"

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
