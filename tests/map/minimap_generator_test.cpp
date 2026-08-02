#include <QColor>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <gtest/gtest.h>

#include "map/map_definition.h"
#include "map/map_loader.h"
#include "map/minimap/map_preview_generator.h"
#include "map/minimap/minimap_generator.h"
#include "map/minimap/minimap_utils.h"

using namespace Game::Map;
using namespace Game::Map::Minimap;

namespace {

auto color_distance(const QColor& lhs, const QColor& rhs) -> int {
  return std::abs(lhs.red() - rhs.red()) + std::abs(lhs.green() - rhs.green()) +
         std::abs(lhs.blue() - rhs.blue());
}

auto color_luminance(const QColor& color) -> int {
  return color.red() + color.green() + color.blue();
}

auto region_distance(const QImage& lhs,
                     const QImage& rhs,
                     const QPoint& center,
                     int radius) -> int {
  int total = 0;
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      const int x = center.x() + dx;
      const int y = center.y() + dy;
      total += color_distance(lhs.pixelColor(x, y), rhs.pixelColor(x, y));
    }
  }
  return total;
}

auto world_dimensions(const GridDefinition& grid) -> std::pair<float, float> {
  return {grid.width * grid.tile_size, grid.height * grid.tile_size};
}

auto image_dimensions(const GridDefinition& grid,
                      float pixels_per_tile) -> std::pair<float, float> {
  return {grid.width * pixels_per_tile, grid.height * pixels_per_tile};
}

auto point_structure(Game::Units::SpawnType type,
                     float world_x,
                     float world_z,
                     int player_id) -> StructureEntry {
  return StructureEntry{
      .type = type,
      .geometry = PointStructureGeometry{QVector3D(world_x, 0.0F, world_z)},
      .player_id = player_id,
  };
}

} // namespace

class MinimapGeneratorTest : public ::testing::Test {
protected:
  void SetUp() override {
    MinimapOrientation::instance().set_yaw_degrees(0.0F);

    test_map.name = "Test Map";
    test_map.grid.width = 50;
    test_map.grid.height = 50;
    test_map.grid.tile_size = 1.0F;

    test_map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);
    test_map.biome.grass_secondary = QVector3D(0.44F, 0.7F, 0.32F);
    test_map.biome.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
  }

  void TearDown() override {
    MinimapOrientation::instance().set_yaw_degrees(Constants::k_default_camera_yaw_deg);
  }

  MapDefinition test_map;
};

TEST_F(MinimapGeneratorTest, GeneratesValidImage) {
  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
  EXPECT_GT(result.width(), 0);
  EXPECT_GT(result.height(), 0);
  EXPECT_EQ(result.format(), QImage::Format_ARGB32);
}

TEST_F(MinimapGeneratorTest, ImageDimensionsMatchGrid) {
  MinimapGenerator::Config config;
  config.pixels_per_tile = 2.0F;
  MinimapGenerator generator(config);

  QImage const result = generator.generate(test_map);

  const int expected_width = test_map.grid.width * config.pixels_per_tile;
  const int expected_height = test_map.grid.height * config.pixels_per_tile;

  EXPECT_EQ(result.width(), expected_width);
  EXPECT_EQ(result.height(), expected_height);
}

TEST_F(MinimapGeneratorTest, LargeCampaignMapsAreCappedToHudUsefulResolution) {
  test_map.grid.width = 650;
  test_map.grid.height = 420;
  MinimapGenerator generator;

  QImage const result = generator.generate(test_map);

  EXPECT_EQ(result.width(), 512);
  EXPECT_EQ(result.height(), 331);
  EXPECT_LE(std::max(result.width(), result.height()), 512);
}

TEST_F(MinimapGeneratorTest, RendersRivers) {

  RiverSegment river;
  river.start = QVector3D(10.0F, 0.0F, 10.0F);
  river.end = QVector3D(40.0F, 0.0F, 40.0F);
  river.width = 3.0F;
  test_map.rivers.push_back(river);

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RiversNearBoundaryContinueToMapEdge) {
  RiverSegment river;
  river.start = QVector3D(0.0F, 0.0F, -19.0F);
  river.end = QVector3D(0.0F, 0.0F, 19.0F);
  river.width = 3.0F;
  test_map.rivers.push_back(river);

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  ASSERT_FALSE(result.isNull());

  const QColor near_top_edge = result.pixelColor(result.width() / 2, 6);
  EXPECT_GT(near_top_edge.blue(), near_top_edge.red());
  EXPECT_GT(near_top_edge.blue(), near_top_edge.green() - 10);
}

TEST_F(MinimapGeneratorTest, RendersTerrainFeatures) {

  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 25.0F;
  hill.center_z = 25.0F;
  hill.width = 10.0F;
  hill.depth = 10.0F;
  hill.height = 3.0F;
  test_map.terrain.push_back(hill);

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersForestFeatures) {

  TerrainFeature forest;
  forest.type = TerrainType::Forest;
  forest.center_x = 30.0F;
  forest.center_z = 30.0F;
  forest.width = 8.0F;
  forest.depth = 8.0F;
  forest.height = 2.0F;
  test_map.terrain.push_back(forest);

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersRoads) {

  RoadSegment road;
  road.start = QVector3D(5.0F, 0.0F, 5.0F);
  road.end = QVector3D(45.0F, 0.0F, 45.0F);
  road.width = 3.0F;
  road.style = "default";
  test_map.roads.push_back(road);

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersStructures) {
  test_map.structures.push_back(
      point_structure(Game::Units::SpawnType::Barracks, 0.5F, 0.5F, 1));

  MinimapGenerator generator;
  QImage const result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersWallStructures) {
  MapDefinition const empty_map = test_map;
  test_map.structures.push_back({
      .type = Game::Units::SpawnType::WallSegment,
      .geometry =
          LineStructureGeometry{
              .start = QVector3D(-10.0F, 0.0F, 0.0F),
              .end = QVector3D(10.0F, 0.0F, 0.0F),
              .width = 2.0F,
          },
      .player_id = 1,
  });

  MinimapGenerator generator;
  const QImage empty = generator.generate(empty_map);
  const QImage walls = generator.generate(test_map);
  const QPoint center(walls.width() / 2, walls.height() / 2);
  EXPECT_GT(color_distance(walls.pixelColor(center), empty.pixelColor(center)), 20);
}

TEST_F(MinimapGeneratorTest, LandmarkBakeKeepsBarracksOnTheStaticImage) {
  MapDefinition const empty_map = test_map;
  test_map.structures.push_back(
      point_structure(Game::Units::SpawnType::Barracks, 0.5F, 0.5F, 2));

  MinimapGenerator::Config config;
  config.structure_bake = MinimapGenerator::StructureBake::LandmarksOnly;
  MinimapGenerator generator(config);

  const QImage empty = generator.generate(empty_map);
  const QImage with_barracks = generator.generate(test_map);
  const QPoint center(with_barracks.width() / 2, with_barracks.height() / 2);

  EXPECT_GT(region_distance(empty, with_barracks, center, 4), 100)
      << "Barracks stay marked on the minimap even before they are scouted.";
}

TEST_F(MinimapGeneratorTest, LandmarkBakeHidesNonBarracksStructures) {
  MapDefinition const empty_map = test_map;
  test_map.structures.push_back(
      point_structure(Game::Units::SpawnType::DefenseTower, 0.5F, 0.5F, 2));
  test_map.structures.push_back(
      point_structure(Game::Units::SpawnType::Home, -8.5F, 6.5F, 2));
  test_map.structures.push_back({
      .type = Game::Units::SpawnType::WallSegment,
      .geometry =
          LineStructureGeometry{
              .start = QVector3D(-10.0F, 0.0F, 0.0F),
              .end = QVector3D(10.0F, 0.0F, 0.0F),
              .width = 2.0F,
          },
      .player_id = 2,
  });

  MinimapGenerator::Config config;
  config.structure_bake = MinimapGenerator::StructureBake::LandmarksOnly;
  MinimapGenerator generator(config);

  const QImage empty = generator.generate(empty_map);
  const QImage with_structures = generator.generate(test_map);

  EXPECT_EQ(empty, with_structures)
      << "Towers, homes and walls must not leak through fog via the static "
         "minimap image.";
}

TEST_F(MinimapGeneratorTest, DefaultBakeStillDrawsEveryStructure) {
  MapDefinition const empty_map = test_map;
  test_map.structures.push_back(
      point_structure(Game::Units::SpawnType::DefenseTower, 0.5F, 0.5F, 2));

  MinimapGenerator generator;
  const QImage empty = generator.generate(empty_map);
  const QImage with_tower = generator.generate(test_map);
  const QPoint center(with_tower.width() / 2, with_tower.height() / 2);

  EXPECT_GT(region_distance(empty, with_tower, center, 4), 100)
      << "The map-select preview has no fog and keeps showing everything.";
}

TEST_F(MinimapGeneratorTest, RoadsRenderBelowTerrainFeatures) {
  RoadSegment road;
  road.start = QVector3D(-15.0F, 0.0F, 0.0F);
  road.end = QVector3D(15.0F, 0.0F, 0.0F);
  road.width = 4.0F;
  test_map.roads.push_back(road);

  TerrainFeature forest;
  forest.type = TerrainType::Forest;
  forest.center_x = 0.0F;
  forest.center_z = 0.0F;
  forest.width = 12.0F;
  forest.depth = 12.0F;
  forest.height = 2.0F;

  MinimapGenerator generator;

  MapDefinition forest_only_map = test_map;
  forest_only_map.terrain.push_back(forest);

  MapDefinition road_only_map = test_map;
  road_only_map.terrain.clear();

  MapDefinition const combined_map = forest_only_map;

  const QImage road_only = generator.generate(road_only_map);
  const QImage forest_only = generator.generate(forest_only_map);
  const QImage combined = generator.generate(combined_map);

  const QPoint sample(road_only.width() / 2, road_only.height() / 2);
  EXPECT_LT(region_distance(combined, forest_only, sample, 5),
            region_distance(combined, road_only, sample, 5));
}

TEST_F(MinimapGeneratorTest, RoadIntersectionsDoNotDarkenAgainstSingleSegment) {
  RoadSegment horizontal;
  horizontal.start = QVector3D(-15.0F, 0.0F, 0.0F);
  horizontal.end = QVector3D(15.0F, 0.0F, 0.0F);
  horizontal.width = 4.0F;

  RoadSegment vertical;
  vertical.start = QVector3D(0.0F, 0.0F, -15.0F);
  vertical.end = QVector3D(0.0F, 0.0F, 15.0F);
  vertical.width = 4.0F;

  MinimapGenerator generator;

  MapDefinition single_road_map = test_map;
  single_road_map.roads.push_back(horizontal);

  MapDefinition crossing_road_map = test_map;
  crossing_road_map.roads.push_back(horizontal);
  crossing_road_map.roads.push_back(vertical);

  const QImage single = generator.generate(single_road_map);
  const QImage crossing = generator.generate(crossing_road_map);

  const QPoint sample(single.width() / 2, single.height() / 2);
  const QColor single_pixel = single.pixelColor(sample);
  const QColor crossing_pixel = crossing.pixelColor(sample);

  EXPECT_GE(color_luminance(crossing_pixel), color_luminance(single_pixel) - 12);
  EXPECT_LT(color_distance(crossing_pixel, single_pixel), 24);
}

TEST_F(MinimapGeneratorTest, BarracksAndHomesExtendFartherThanOtherBuildingIcons) {
  MinimapGenerator generator;

  const auto [world_width, world_height] = world_dimensions(test_map.grid);
  const auto [img_width, img_height] = image_dimensions(test_map.grid, 2.0F);

  const auto [home_world_x, home_world_z] =
      grid_to_world_coords(25.0F, 25.0F, test_map);
  const auto [px, py] = world_to_pixel(
      home_world_x, home_world_z, world_width, world_height, img_width, img_height);
  const QPoint sample(static_cast<int>(std::lround(px + 7.0F)),
                      static_cast<int>(std::lround(py - 5.0F)));

  MapDefinition const empty_map = test_map;

  MapDefinition barracks_map = test_map;
  barracks_map.structures.push_back(
      point_structure(Game::Units::SpawnType::Barracks, home_world_x, home_world_z, 1));

  MapDefinition home_map = test_map;
  home_map.structures.push_back(
      point_structure(Game::Units::SpawnType::Home, home_world_x, home_world_z, 1));

  MapDefinition tower_map = test_map;
  tower_map.structures.push_back(point_structure(
      Game::Units::SpawnType::DefenseTower, home_world_x, home_world_z, 1));

  const QColor empty_pixel = generator.generate(empty_map).pixelColor(sample);
  const QColor barracks_pixel = generator.generate(barracks_map).pixelColor(sample);
  const QColor home_pixel = generator.generate(home_map).pixelColor(sample);
  const QColor tower_pixel = generator.generate(tower_map).pixelColor(sample);

  EXPECT_GT(color_distance(barracks_pixel, empty_pixel),
            color_distance(tower_pixel, empty_pixel) + 20);
  EXPECT_GT(color_distance(home_pixel, empty_pixel),
            color_distance(tower_pixel, empty_pixel) + 20);
}

TEST(MapPreviewGeneratorTest, PlayerBasesLandOnTheCappedPreviewScale) {
  const QString map_path = QStringLiteral("assets/maps/map_battle_cannae.json");

  MapDefinition map_def;
  QString error;
  ASSERT_TRUE(MapLoader::load_from_json_file(map_path, map_def, &error))
      << error.toStdString();
  ASSERT_GT(map_def.grid.width, 256)
      << "This map has to be large enough for the generator to cap the image.";

  const PointStructureGeometry* player_one_base = nullptr;
  for (const auto& structure : map_def.structures) {
    if (structure.player_id != 1) {
      continue;
    }
    if (const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry)) {
      player_one_base = point;
      break;
    }
  }
  ASSERT_NE(player_one_base, nullptr);

  const QColor player_color(QStringLiteral("#ff00ff"));
  QVariantMap config;
  config["player_id"] = 1;
  config["colorHex"] = player_color.name();

  MapPreviewGenerator generator;
  const QImage preview = generator.generate_preview(map_path, QVariantList{config});
  ASSERT_FALSE(preview.isNull());
  EXPECT_LE(std::max(preview.width(), preview.height()), 512);

  const auto [world_width, world_height] = world_dimensions(map_def.grid);
  const auto [px, py] =
      Game::Map::Minimap::world_to_pixel(player_one_base->position.x(),
                                         player_one_base->position.z(),
                                         world_width,
                                         world_height,
                                         static_cast<float>(preview.width()),
                                         static_cast<float>(preview.height()));

  const QPoint marker(static_cast<int>(std::lround(px)),
                      static_cast<int>(std::lround(py)));
  ASSERT_TRUE(preview.rect().contains(marker))
      << "marker at " << marker.x() << "," << marker.y();
  EXPECT_LT(color_distance(preview.pixelColor(marker), player_color), 90)
      << "The start-position marker must be drawn at the same scale as the "
         "capped preview image.";
}

TEST_F(MinimapGeneratorTest, HandlesEmptyMap) {

  MapDefinition empty_map;
  empty_map.grid.width = 10;
  empty_map.grid.height = 10;
  empty_map.grid.tile_size = 1.0F;
  empty_map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);

  MinimapGenerator generator;
  QImage const result = generator.generate(empty_map);

  EXPECT_FALSE(result.isNull());
  EXPECT_GT(result.width(), 0);
  EXPECT_GT(result.height(), 0);
}
