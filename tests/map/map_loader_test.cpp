#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/procedural_tree_generation.h"
#include "game/map/terrain.h"
#include "game/systems/resource_types.h"

TEST(MapLoaderTest, ExpandsRiverWaypointsIntoAContinuousRuntimeChain) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonArray waypoints{
      QJsonArray{0, 4}, QJsonArray{8, 7}, QJsonArray{12, 12}, QJsonArray{20, 16}};
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
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
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

TEST(MapLoaderTest, ParsesUndeadZoneClearReward) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Undead Reward Test"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"undead_zones",
       QJsonArray{
           QJsonObject{{"id", "hoard"},
                       {"x", 16},
                       {"z", 16},
                       {"clear_reward",
                        QJsonObject{{"gold", 150}, {"stone", 80}, {"iron", 40}}}},
           QJsonObject{{"id", "no_hoard"}, {"x", 8}, {"z", 8}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      temp_file.fileName(), map_definition, &error))
      << error.toStdString();

  ASSERT_EQ(map_definition.undead_zones.size(), 2);
  const auto& rewarded = map_definition.undead_zones[0];
  EXPECT_EQ(rewarded.clear_reward.get(Game::Systems::ResourceType::Gold), 150);
  EXPECT_EQ(rewarded.clear_reward.get(Game::Systems::ResourceType::Stone), 80);
  EXPECT_EQ(rewarded.clear_reward.get(Game::Systems::ResourceType::Iron), 40);
  EXPECT_EQ(rewarded.clear_reward.get(Game::Systems::ResourceType::Wood), 0);
  EXPECT_TRUE(map_definition.undead_zones[1].clear_reward.empty());
}

TEST(MapLoaderTest, ParsesProceduralScatterOptOuts) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Scatter Opt Out Test"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"biome",
       QJsonObject{{"procedural_boulders_enabled", false},
                   {"procedural_iron_ore_enabled", false}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      temp_file.fileName(), map_definition, &error))
      << error.toStdString();

  EXPECT_FALSE(map_definition.biome.procedural_boulders_enabled);
  EXPECT_FALSE(map_definition.biome.procedural_iron_ore_enabled);
  EXPECT_TRUE(map_definition.biome.procedural_trees_enabled);
}

TEST(MapLoaderTest, ParsesUndeadVictoryObjectives) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Undead Victory Test"},
      {"grid", QJsonObject{{"width", 32}, {"height", 32}, {"tile_size", 1.0}}},
      {"victory",
       QJsonObject{
           {"type", "undead_zones"},
           {"undead_objectives",
            QJsonArray{
                QJsonObject{{"type", "clear_undead_zone"}, {"zone_id", "ruins_guard"}},
                QJsonObject{{"type", "purify_shrine"}, {"zone_id", "shrine"}},
                QJsonObject{{"type", "survive_undead_wave"},
                            {"zone_id", "ruins_guard"},
                            {"wave_count", 3}},
                QJsonObject{{"type", "clear_undead_zone"}}}},
           {"defeat_conditions", QJsonArray{"no_commander"}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      temp_file.fileName(), map_definition, &error))
      << error.toStdString();

  EXPECT_EQ(map_definition.victory.victory_type, QStringLiteral("undead_zones"));
  ASSERT_EQ(map_definition.victory.undead_objectives.size(), 3U);
  EXPECT_EQ(map_definition.victory.undead_objectives[0].type,
            QStringLiteral("clear_undead_zone"));
  EXPECT_EQ(map_definition.victory.undead_objectives[0].zone_id,
            QStringLiteral("ruins_guard"));
  EXPECT_EQ(map_definition.victory.undead_objectives[1].type,
            QStringLiteral("purify_shrine"));
  EXPECT_EQ(map_definition.victory.undead_objectives[2].wave_count, 3);
}

TEST(MapLoaderTest, IronSepulcherWatchMapDrivesVictoryFromItsUndeadZones) {
  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_iron_sepulcher_watch.json"),
      map_definition,
      &error))
      << error.toStdString();

  ASSERT_EQ(map_definition.undead_zones.size(), 2U);
  EXPECT_EQ(map_definition.victory.victory_type, QStringLiteral("undead_zones"));
  ASSERT_EQ(map_definition.victory.undead_objectives.size(), 2U);

  for (const auto& objective : map_definition.victory.undead_objectives) {
    EXPECT_NE(std::find_if(map_definition.undead_zones.begin(),
                           map_definition.undead_zones.end(),
                           [&objective](const Game::Map::UndeadZone& zone) {
                             return zone.id == objective.zone_id;
                           }),
              map_definition.undead_zones.end())
        << objective.zone_id.toStdString();
  }
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
       QJsonArray{QJsonObject{{"start", QJsonArray{0.0, 0.0}},
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
       QJsonArray{
           QJsonObject{{"x", 0.0}, {"z", 0.0}, {"width", 20.0}, {"depth", 20.0}}}}};
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

namespace {

struct BridgeSpanBudget {
  float shortest_half = 0.0F;
  float longest_half = 0.0F;
};

auto bridge_span_budget(float bridge_width, float river_width) -> BridgeSpanBudget {
  const float shortest = Game::Map::river_bank_standing_half_width(river_width) +
                         Game::Map::bridge_bank_landing(bridge_width, river_width);
  return {shortest,
          shortest + Game::Map::bridge_bank_overhang(bridge_width, river_width)};
}

} // namespace

TEST(MapLoaderTest, LandsBridgeEndpointsOnTheBankPastTheDrawnWaterline) {
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

  const auto budget = bridge_span_budget(bridge.width, 10.0F);
  EXPECT_GT(budget.shortest_half, 5.0F);
  EXPECT_NEAR(bridge.start.x(), -budget.shortest_half, 0.0001F);
  EXPECT_NEAR(bridge.end.x(), budget.shortest_half, 0.0001F);
}

TEST(MapLoaderTest, SquaresBridgeDecksToTheRiverTheyCross) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Skew Bridge Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{0.0, -20.0}},
                              {"end", QJsonArray{0.0, 20.0}},
                              {"width", 6.0}}}},
      {"bridges",
       QJsonArray{QJsonObject{{"start", QJsonArray{-6.0, -6.0}},
                              {"end", QJsonArray{6.0, 6.0}},
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

  EXPECT_NEAR(bridge.start.z(), 0.0F, 0.0001F);
  EXPECT_NEAR(bridge.end.z(), 0.0F, 0.0001F);

  const auto budget = bridge_span_budget(bridge.width, 6.0F);
  EXPECT_GE(-bridge.start.x(), budget.shortest_half - 0.0001F);
  EXPECT_LE(-bridge.start.x(), budget.longest_half + 0.0001F);
  EXPECT_GE(bridge.end.x(), budget.shortest_half - 0.0001F);
  EXPECT_LE(bridge.end.x(), budget.longest_half + 0.0001F);
}

TEST(MapLoaderTest, TrimsOverlongBridgesBackToTheRiverbanks) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Overlong Bridge Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{0.0, -20.0}},
                              {"end", QJsonArray{0.0, 20.0}},
                              {"width", 4.0}}}},
      {"bridges",
       QJsonArray{QJsonObject{{"start", QJsonArray{-12.0, 0.0}},
                              {"end", QJsonArray{12.0, 0.0}},
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

  const auto budget = bridge_span_budget(bridge.width, 4.0F);

  EXPECT_NEAR(bridge.start.x(), -budget.longest_half, 0.0001F);
  EXPECT_NEAR(bridge.end.x(), budget.longest_half, 0.0001F);

  EXPECT_LE(bridge.start.x(), -2.0F);
  EXPECT_GE(bridge.end.x(), 2.0F);
}

TEST(MapLoaderTest, KeepsAuthoredBridgeAsymmetryInsideTheOverhangBudget) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const auto authored_budget = bridge_span_budget(Game::Map::k_min_bridge_width, 4.0F);
  const double start_reach = authored_budget.shortest_half + 0.2F;
  const double end_reach = authored_budget.longest_half - 0.2F;

  const QJsonObject root{
      {"name", "Asymmetric Bridge Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"rivers",
       QJsonArray{QJsonObject{{"start", QJsonArray{0.0, -20.0}},
                              {"end", QJsonArray{0.0, 20.0}},
                              {"width", 4.0}}}},
      {"bridges",
       QJsonArray{QJsonObject{{"start", QJsonArray{-start_reach, 0.0}},
                              {"end", QJsonArray{end_reach, 0.0}},
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

  const auto budget = bridge_span_budget(bridge.width, 4.0F);
  ASSERT_GE(start_reach, budget.shortest_half);
  ASSERT_LE(end_reach, budget.longest_half);

  EXPECT_NEAR(bridge.start.x(), -static_cast<float>(start_reach), 0.0001F);
  EXPECT_NEAR(bridge.end.x(), static_cast<float>(end_reach), 0.0001F);
}

TEST(MapLoaderTest, LeavesBridgesThatCrossNoRiverUntouched) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Landlocked Bridge Test"},
      {"coord_system", "world"},
      {"grid", QJsonObject{{"width", 64}, {"height", 64}, {"tile_size", 1.0}}},
      {"bridges",
       QJsonArray{QJsonObject{{"start", QJsonArray{-9.0, 0.0}},
                              {"end", QJsonArray{9.0, 0.0}},
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
  EXPECT_NEAR(map_def.bridges.front().start.x(), -9.0F, 0.0001F);
  EXPECT_NEAR(map_def.bridges.front().end.x(), 9.0F, 0.0001F);
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

TEST(MapLoaderTest, ParsesPointAndLineStructuresWithOwnership) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Buildings Test"},
      {"grid", QJsonObject{{"width", 50}, {"height", 50}, {"tile_size", 1.0}}},
      {"structures",
       QJsonArray{QJsonObject{{"type", "defense_tower"},
                              {"x", 10},
                              {"z", 15},
                              {"player_id", 1},
                              {"nation", "rome"}},
                  QJsonObject{{"type", "home"}, {"x", 20}, {"z", 25}, {"player_id", 2}},
                  QJsonObject{{"type", "wall_segment"},
                              {"start", QJsonArray{5, 10}},
                              {"end", QJsonArray{30, 10}},
                              {"player_id", 1},
                              {"nation", "rome"}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map_def, &error))
      << error.toStdString();

  ASSERT_EQ(map_def.structures.size(), 3U);
  const auto& tower = map_def.structures[0];
  EXPECT_EQ(tower.type, Game::Units::SpawnType::DefenseTower);
  EXPECT_EQ(tower.player_id, 1);
  EXPECT_EQ(tower.nation, QStringLiteral("rome"));
  ASSERT_TRUE(
      std::holds_alternative<Game::Map::PointStructureGeometry>(tower.geometry));

  const auto& home = map_def.structures[1];
  EXPECT_EQ(home.type, Game::Units::SpawnType::Home);
  EXPECT_EQ(home.player_id, 2);
  const auto& wall = map_def.structures[2];
  EXPECT_EQ(wall.type, Game::Units::SpawnType::WallSegment);
  EXPECT_EQ(wall.player_id, 1);
  ASSERT_TRUE(std::holds_alternative<Game::Map::LineStructureGeometry>(wall.geometry));
  const auto& line = std::get<Game::Map::LineStructureGeometry>(wall.geometry);
  EXPECT_LT(line.start.x(), line.end.x());
}

TEST(MapLoaderTest, EmptyStructuresWhenArrayAbsent) {
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

  EXPECT_TRUE(map_def.structures.empty());
}

TEST(MapLoaderTest, ParsesContinuousEnvironmentAndOverridesLegacyAlias) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());
  const QJsonObject root{
      {"name", "Continuous Sunset"},
      {"grid", QJsonObject{{"width", 16}, {"height", 16}, {"tile_size", 1.0}}},
      {"time_of_day", "morning"},
      {"environment",
       QJsonObject{{"start_time", 17.5},
                   {"time_mode", "continuous"},
                   {"day_length_seconds", 900.0},
                   {"lighting_profile", "iron_sepulcher"},
                   {"fog_density", 0.012},
                   {"exposure", 0.86}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
      << error.toStdString();
  EXPECT_FLOAT_EQ(map.environment.start_time, 17.5F);
  EXPECT_EQ(map.environment.time_mode, Game::Map::TimeMode::Continuous);
  EXPECT_FLOAT_EQ(map.environment.day_length_seconds, 900.0F);
  EXPECT_EQ(map.environment.lighting_profile, QStringLiteral("iron_sepulcher"));
  EXPECT_FLOAT_EQ(map.environment.fog_density_override, 0.012F);
  EXPECT_FLOAT_EQ(map.environment.exposure_override, 0.86F);
  EXPECT_EQ(map.time_of_day, Game::Map::TimeOfDay::Afternoon);
}

TEST(MapLoaderTest, ReusingDefinitionCannotRetainPreviousEnvironment) {
  QTemporaryFile first;
  QTemporaryFile second;
  ASSERT_TRUE(first.open());
  ASSERT_TRUE(second.open());
  const QJsonObject grid{{"width", 8}, {"height", 8}, {"tile_size", 1.0}};
  first.write(
      QJsonDocument(QJsonObject{{"grid", grid},
                                {"environment",
                                 QJsonObject{{"start_time", 22.0},
                                             {"lighting_profile", "iron_sepulcher"}}}})
          .toJson(QJsonDocument::Compact));
  second.write(
      QJsonDocument(QJsonObject{{"grid", grid}}).toJson(QJsonDocument::Compact));
  first.flush();
  second.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(first.fileName(), map, &error));
  ASSERT_EQ(map.environment.lighting_profile, QStringLiteral("iron_sepulcher"));
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(second.fileName(), map, &error));
  EXPECT_FLOAT_EQ(map.environment.start_time, 13.0F);
  EXPECT_EQ(map.environment.lighting_profile, QStringLiteral("mediterranean_summer"));
  EXPECT_EQ(map.environment.time_mode, Game::Map::TimeMode::Locked);
}

namespace {

auto load_weather(const QJsonObject& rain) -> Game::Map::RainSettings {
  QTemporaryFile temp_file;
  EXPECT_TRUE(temp_file.open());
  const QJsonObject root{
      {"name", "Weather Test"},
      {"grid", QJsonObject{{"width", 16}, {"height", 16}, {"tile_size", 1.0}}},
      {"rain", rain}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
      << error.toStdString();
  return map.rain;
}

} // namespace

TEST(MapLoaderTest, ReadsNamedPrecipitationIntensities) {
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", "light"}}).intensity,
                  Game::Map::k_weather_intensity_light);
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", "Medium"}}).intensity,
                  Game::Map::k_weather_intensity_medium);
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", "HEAVY"}}).intensity,
                  Game::Map::k_weather_intensity_heavy);
}

TEST(MapLoaderTest, NumericPrecipitationIntensityStillLoadsAndIsClamped) {
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", 0.42}}).intensity,
                  0.42F);
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", 4.0}}).intensity,
                  1.0F);
  EXPECT_FLOAT_EQ(load_weather({{"enabled", true}, {"intensity", -1.0}}).intensity,
                  0.0F);
}

TEST(MapLoaderTest, UnknownIntensityWordKeepsTheDefault) {
  const auto settings = load_weather({{"enabled", true}, {"intensity", "torrential"}});
  EXPECT_FLOAT_EQ(settings.intensity, Game::Map::RainSettings{}.intensity);
}

TEST(MapLoaderTest, ReadsWindDirectionAndStrength) {
  const auto settings = load_weather({{"enabled", true},
                                      {"type", "snow"},
                                      {"wind_strength", 0.65},
                                      {"wind_direction", 330.0}});
  EXPECT_EQ(settings.type, Game::Map::WeatherType::Snow);
  EXPECT_FLOAT_EQ(settings.wind_strength, 0.65F);
  EXPECT_FLOAT_EQ(settings.wind_direction_deg, 330.0F);
}

TEST(MapLoaderTest, FailedLoadClearsPreviouslyLoadedDefinition) {
  Game::Map::MapDefinition map;
  map.name = QStringLiteral("Previous map");
  map.environment.start_time = 2.0F;
  map.rain.enabled = true;

  QString error;
  EXPECT_FALSE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("/definitely/not/a/map.json"), map, &error));
  EXPECT_TRUE(map.name.isEmpty());
  EXPECT_FLOAT_EQ(map.environment.start_time, 13.0F);
  EXPECT_FALSE(map.rain.enabled);
  EXPECT_FALSE(error.isEmpty());
}

TEST(MapLoaderTest, RejectsRetiredStructureCollectionsAndBuildingSpawns) {
  const auto expect_rejected = [](const QJsonObject& root) {
    QTemporaryFile temp_file;
    EXPECT_TRUE(temp_file.open());
    temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    temp_file.flush();
    Game::Map::MapDefinition map_def;
    QString error;
    EXPECT_FALSE(Game::Map::MapLoader::load_from_json_file(
        temp_file.fileName(), map_def, &error));
    EXPECT_FALSE(error.isEmpty());
  };

  const QJsonObject grid{{"width", 16}, {"height", 16}, {"tile_size", 1.0}};
  expect_rejected(QJsonObject{{"grid", grid}, {"buildings", QJsonArray{}}});
  expect_rejected(QJsonObject{{"grid", grid}, {"walls", QJsonArray{}}});
  expect_rejected(QJsonObject{
      {"grid", grid},
      {"spawns", QJsonArray{QJsonObject{{"type", "barracks"}, {"x", 2}, {"z", 2}}}}});
}

TEST(MapLoaderTest, LoadsEveryShippedMapWithTheCanonicalStructureSchema) {
  QDir repo_root = QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir();
  ASSERT_TRUE(repo_root.cdUp());
  ASSERT_TRUE(repo_root.cdUp());
  QDir const maps_dir(repo_root.filePath(QStringLiteral("assets/maps")));
  const QStringList maps =
      maps_dir.entryList(QStringList{QStringLiteral("*.json")}, QDir::Files);
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& map_name : maps) {
    Game::Map::MapDefinition map_def;
    QString error;
    EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
        maps_dir.filePath(map_name), map_def, &error))
        << map_name.toStdString() << ": " << error.toStdString();
  }
}

TEST(MapLoaderTest, AnAuthoredForestBecomesBothNavigationDataAndForestGround) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Forest Ground Test"},
      {"grid", QJsonObject{{"width", 41}, {"height", 41}, {"tile_size", 1.0}}},
      {"forests",
       QJsonArray{
           QJsonObject{{"id", "screen"}, {"x", 20}, {"z", 20}, {"radius", 6.0}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
      << error.toStdString();

  ASSERT_EQ(map.forests.size(), 1U);
  EXPECT_EQ(map.forests[0].id, QStringLiteral("screen"));

  const auto forest_features =
      std::count_if(map.terrain.begin(), map.terrain.end(), [](const auto& feature) {
        return feature.type == Game::Map::TerrainType::Forest;
      });
  ASSERT_EQ(forest_features, 1)
      << "the loader must raise a Forest terrain feature over every authored "
         "forest, or the trees never thicken over it";

  Game::Map::TerrainHeightMap height_map(map.grid.width, map.grid.height, 1.0F);
  height_map.build_from_features(map.terrain);
  EXPECT_EQ(height_map.getTerrainType(20, 20), Game::Map::TerrainType::Forest);
  EXPECT_EQ(height_map.getTerrainType(0, 0), Game::Map::TerrainType::Flat);
}

TEST(MapLoaderTest, AForestOverAHillIsClippedRatherThanFlatteningIt) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject hill{
      {"type", "hill"}, {"x", 20}, {"z", 20}, {"radius", 6.0}, {"height", 3.0}};
  const QJsonObject root{
      {"name", "Forest Over Hill Test"},
      {"grid", QJsonObject{{"width", 41}, {"height", 41}, {"tile_size", 1.0}}},
      {"terrain", QJsonArray{hill}},
      {"forests",
       QJsonArray{QJsonObject{
           {"id", "over_the_hill"}, {"x", 20}, {"z", 20}, {"radius", 14.0}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
      << error.toStdString();

  Game::Map::TerrainHeightMap height_map(map.grid.width, map.grid.height, 1.0F);
  height_map.build_from_features(map.terrain);

  EXPECT_EQ(height_map.getTerrainType(20, 20), Game::Map::TerrainType::Hill)
      << "the forest is appended after the terrain, so the hill keeps its crown";
  EXPECT_GT(height_map.get_height_at_grid(20, 20), 0.5F);
  EXPECT_EQ(height_map.getTerrainType(20, 33), Game::Map::TerrainType::Forest)
      << "and the forest still claims the flat ground beyond the hill";
}

TEST(MapLoaderTest, AForestIsWhereTheTreesActuallyThicken) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  const QJsonObject root{
      {"name", "Tree Density Test"},
      {"grid", QJsonObject{{"width", 161}, {"height", 161}, {"tile_size", 1.0}}},
      {"forests",
       QJsonArray{QJsonObject{{"id", "big"}, {"x", 80}, {"z", 80}, {"radius", 50.0}}}}};
  temp_file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  temp_file.flush();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(
      Game::Map::MapLoader::load_from_json_file(temp_file.fileName(), map, &error))
      << error.toStdString();

  const auto count_trees = [&](const std::vector<Game::Map::TerrainFeature>& features) {
    Game::Map::TerrainHeightMap height_map(
        map.grid.width, map.grid.height, map.grid.tile_size);
    height_map.build_from_features(features);
    const auto props = Game::Map::generate_procedural_world_props(
        height_map, map.biome, map.coordSystem, {});
    return std::count_if(props.begin(), props.end(), [](const Game::Map::WorldProp& p) {
      return Game::Map::is_tree_world_prop_type(p.type);
    });
  };

  const auto with_forest = count_trees(map.terrain);
  const auto bare_ground = count_trees({});

  EXPECT_GT(with_forest, bare_ground)
      << "an authored forest has to raise the tree scatter over its ground; "
         "with_forest="
      << with_forest << " bare=" << bare_ground;
}
