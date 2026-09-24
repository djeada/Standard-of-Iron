#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "game/map/terrain.h"
#include "render/gl/mesh.h"
#include "render/ground/linear_feature_geometry.h"

namespace {

// A plane rising `grade` metres per metre along +z, so every bank of a river
// running along x is a slope.
auto sloped_height_map(int size, float grade) -> Game::Map::TerrainHeightMap {
  const auto cells = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
  std::vector<float> heights(cells, 0.0F);
  for (int z = 0; z < size; ++z) {
    for (int x = 0; x < size; ++x) {
      heights[static_cast<std::size_t>(z * size + x)] =
          grade * (static_cast<float>(z) - static_cast<float>(size) * 0.5F);
    }
  }
  const std::vector<Game::Map::TerrainType> types(cells, Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(size, size, 1.0F);
  height_map.restore_from_data(heights, types, {}, {});
  return height_map;
}

// A plane falling `grade` metres per metre across x, so a bridge running
// along z has one deck edge higher than the other.
auto cross_sloped_height_map(int size, float grade) -> Game::Map::TerrainHeightMap {
  const auto cells = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
  std::vector<float> heights(cells, 0.0F);
  for (int z = 0; z < size; ++z) {
    for (int x = 0; x < size; ++x) {
      heights[static_cast<std::size_t>(z * size + x)] =
          grade * (static_cast<float>(x) - static_cast<float>(size) * 0.5F);
    }
  }
  const std::vector<Game::Map::TerrainType> types(cells, Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(size, size, 1.0F);
  height_map.restore_from_data(heights, types, {}, {});
  return height_map;
}

} // namespace

TEST(WaterTransitionTest, RiverbankStripDrapesOverASlopedBank) {
  auto const height_map = sloped_height_map(48, 0.35F);
  const std::vector<Game::Map::RiverSegment> river{
      {{-12.0F, 0.0F, 0.0F}, {12.0F, 0.0F, 0.0F}, 4.0F}};

  auto const bank = Render::Ground::build_riverbank_mesh(river, 0U, height_map);
  ASSERT_NE(bank.mesh, nullptr);

  int land_vertices = 0;
  for (const auto& vertex : bank.mesh->get_vertices()) {
    // tex_coord.x is the ring's distance from the water; ring 0 is the
    // waterline, tucked under the surface on purpose.
    if (vertex.tex_coord[0] <= 0.0F) {
      continue;
    }
    ++land_vertices;
    float const ground =
        height_map.get_base_height_at(vertex.position[0], vertex.position[2]);
    EXPECT_GE(vertex.position[1], ground - 1.0e-4F)
        << "bank strip sank under the slope at (" << vertex.position[0] << ", "
        << vertex.position[2] << ")";
  }
  EXPECT_GT(land_vertices, 50);
}

TEST(WaterTransitionTest, BendJunctionDiscsSitUnderTheRibbonsTheyJoin) {
  Render::Ground::LinearFeatureRibbonSettings settings =
      Render::Ground::make_river_ribbon_settings();
  settings.use_segment_elevation_profile = true;
  settings.y_offset = 0.02F;
  settings.shared_junction_drop = 0.008F;
  const std::vector<Render::Ground::LinearFeatureRibbonSegment> bend{
      {{-8.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F},
      {{0.0F, 0.0F, 0.0F}, {5.0F, 0.0F, 6.0F}, 4.0F}};

  auto const junctions =
      Render::Ground::build_linear_feature_junction_meshes(bend, 1.0F, settings);
  auto const shared = std::find_if(
      junctions.begin(), junctions.end(), [](auto& j) { return j.connections > 1; });
  ASSERT_NE(shared, junctions.end());
  for (const auto& vertex : shared->mesh->get_vertices()) {
    EXPECT_LT(vertex.position[1], settings.y_offset)
        << "a shared joint's disc must lose the depth test to the ribbons";
  }

  auto const open_end = std::find_if(
      junctions.begin(), junctions.end(), [](auto& j) { return j.connections == 1; });
  ASSERT_NE(open_end, junctions.end());
  EXPECT_FLOAT_EQ(open_end->mesh->get_vertices().front().position[1], settings.y_offset)
      << "an open river end is a real shore and stays at the water surface";
}

TEST(WaterTransitionTest, WalkedDeckHeightIsTheDrawnDeckIncludingItsLandings) {
  auto const height_map = cross_sloped_height_map(64, 0.12F);
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.0F, -6.0F);
  bridge.end = QVector3D(0.0F, 0.0F, 6.0F);
  bridge.width = 8.0F;
  bridge.height = 0.6F;

  float const landing =
      Game::Map::bridge_visual_landing_run(Game::Map::bridge_drawn_width(bridge));

  // Continuous from the landing end, over the span, to the far landing end.
  float previous = *height_map.bridge_deck_surface_y(bridge, -landing);
  for (float along = -landing + 0.05F; along <= 12.0F + landing; along += 0.05F) {
    auto const y = height_map.bridge_deck_surface_y(bridge, along);
    ASSERT_TRUE(y.has_value()) << along;
    EXPECT_LT(std::abs(*y - previous), 0.05F) << "deck steps at along=" << along;
    previous = *y;
  }

  // Beyond the drawn landings there is no deck: units walk on the ground.
  EXPECT_FALSE(height_map.bridge_deck_surface_y(bridge, -landing - 0.2F).has_value());
  EXPECT_FALSE(
      height_map.bridge_deck_surface_y(bridge, 12.0F + landing + 0.2F).has_value());

  // Where a landing touches down it clears the highest ground across the
  // flared deck by road height, so neither edge is buried on a cross-slope.
  auto const ground = height_map.bridge_station_ground(bridge, -landing);
  EXPECT_GT(ground.highest - ground.lowest, 0.5F) << "fixture must cross a slope";
  EXPECT_NEAR(*height_map.bridge_deck_surface_y(bridge, -landing),
              ground.highest + Game::Map::k_bridge_landing_end_lift,
              1.0e-4F);
}

TEST(WaterTransitionTest, BridgeLandingsReachTheGroundUnderBothDeckEdges) {
  auto const height_map = cross_sloped_height_map(64, 0.12F);
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.0F, -6.0F);
  bridge.end = QVector3D(0.0F, 0.0F, 6.0F);
  bridge.width = 8.0F;
  bridge.height = 0.6F;

  auto const mesh = Render::Ground::build_bridge_mesh(bridge, 1.0F, height_map);
  ASSERT_NE(mesh, nullptr);
  float const landing =
      Game::Map::bridge_visual_landing_run(Game::Map::bridge_drawn_width(bridge));

  // At the very end of each landing, the lowest vertex is at or below the
  // lowest ground across the deck: the abutment meets the downhill side.
  for (float end_z : {-6.0F - landing, 6.0F + landing}) {
    float lowest_vertex = std::numeric_limits<float>::max();
    for (const auto& vertex : mesh->get_vertices()) {
      if (std::abs(vertex.position[2] - end_z) < 0.02F) {
        lowest_vertex = std::min(lowest_vertex, vertex.position[1]);
      }
    }
    ASSERT_LT(lowest_vertex, std::numeric_limits<float>::max()) << end_z;
    auto const along = end_z < 0.0F ? -landing : 12.0F + landing;
    EXPECT_LE(lowest_vertex, height_map.bridge_station_ground(bridge, along).lowest)
        << "daylight under the deck end at z=" << end_z;
  }
}
