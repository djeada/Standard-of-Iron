#include "game/map/terrain.h"
#include "render/gl/mesh.h"
#include "render/ground/linear_feature_geometry.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace {

TEST(LinearFeatureGeometryTest, BuildsRoadRibbonMeshWithConfiguredYOffset) {
  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.edge_noise_frequencies = {1.5F, 4.0F, 0.0F};
  settings.edge_noise_weights = {0.6F, 0.4F, 0.0F};
  settings.width_variation_scale = 0.15F;
  settings.meander_length_scale = 0.1F;
  settings.y_offset = Game::Map::k_road_surface_y_offset;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  EXPECT_FALSE(mesh->get_vertices().empty());
  EXPECT_FALSE(mesh->get_indices().empty());
  EXPECT_FLOAT_EQ(mesh->get_vertices().front().position[1],
                  Game::Map::k_road_surface_y_offset);
}

TEST(LinearFeatureGeometryTest, BuildsRoadRibbonMeshAlongTerrainHeightMap) {
  std::vector<float> heights = {0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 0.0F, 1.0F,
                                2.0F, 3.0F, 4.0F, 0.0F, 1.0F, 2.0F, 3.0F,
                                4.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 0.0F,
                                1.0F, 2.0F, 3.0F, 4.0F};
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(5, 5, 1.0F);
  height_map.restore_from_data(heights, terrain_types, {}, {});

  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 1.0F;
  settings.min_length_steps = 2;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.height_map = &height_map;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  ASSERT_GE(mesh->get_vertices().size(), 4U);
  EXPECT_FLOAT_EQ(
      mesh->get_vertices().front().position[1],
      Game::Map::road_surface_world_y(height_map.get_height_at(-2.0F, 0.0F)));
  EXPECT_FLOAT_EQ(
      mesh->get_vertices().back().position[1],
      Game::Map::road_surface_world_y(height_map.get_height_at(2.0F, 0.0F)));
}

TEST(LinearFeatureGeometryTest, BuildsRoadRibbonMeshAboveCrossSlopeTerrain) {
  std::vector<float> heights = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F,
                                1.0F, 1.0F, 1.0F, 2.0F, 2.0F, 2.0F, 2.0F,
                                2.0F, 3.0F, 3.0F, 3.0F, 3.0F, 3.0F, 4.0F,
                                4.0F, 4.0F, 4.0F, 4.0F};
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(5, 5, 1.0F);
  height_map.restore_from_data(heights, terrain_types, {}, {});

  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 1.0F;
  settings.min_length_steps = 2;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.height_map = &height_map;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  ASSERT_GE(mesh->get_vertices().size(), 4U);
  EXPECT_FLOAT_EQ(
      mesh->get_vertices()[0].position[1],
      Game::Map::road_surface_world_y(height_map.get_height_at(-2.0F, -1.0F)));
  EXPECT_FLOAT_EQ(
      mesh->get_vertices()[1].position[1],
      Game::Map::road_surface_world_y(height_map.get_height_at(-2.0F, 1.0F)));
}

TEST(LinearFeatureGeometryTest, BuildsRiverRibbonMeshWithMeanderSupport) {
  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.edge_noise_frequencies = {2.0F, 5.0F, 10.0F};
  settings.edge_noise_weights = {0.5F, 0.3F, 0.2F};
  settings.width_variation_scale = 0.35F;
  settings.meander_frequency = 3.0F;
  settings.meander_length_scale = 0.1F;
  settings.meander_amplitude = 0.3F;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(0.0F, 0.0F, -2.0F), QVector3D(0.0F, 0.0F, 2.0F), 3.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  EXPECT_GT(mesh->get_vertices().size(), 8U);
  EXPECT_GT(mesh->get_indices().size(), 6U);
  EXPECT_FLOAT_EQ(mesh->get_vertices().front().position[1], 0.0F);
}

TEST(LinearFeatureGeometryTest, BuildsBridgeMeshFromSharedHelper) {
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(-2.0F, 0.5F, 0.0F);
  bridge.end = QVector3D(2.0F, 0.5F, 0.0F);
  bridge.width = 2.0F;
  bridge.height = 0.6F;

  auto mesh = Render::Ground::build_bridge_mesh(bridge, 1.0F);

  ASSERT_NE(mesh, nullptr);
  EXPECT_FALSE(mesh->get_vertices().empty());
  EXPECT_FALSE(mesh->get_indices().empty());

  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  for (const auto &vertex : mesh->get_vertices()) {
    min_x = std::min(min_x, vertex.position[0]);
    max_x = std::max(max_x, vertex.position[0]);
  }

  float start_width = 0.0F;
  bool found_start_band = false;
  float start_max_y = std::numeric_limits<float>::lowest();
  float mid_max_y = std::numeric_limits<float>::lowest();
  bool found_mid_band = false;
  auto update_band_width = [&](float center_x, float half_band,
                               float &width_out, bool &found) {
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();
    for (const auto &vertex : mesh->get_vertices()) {
      if (std::abs(vertex.position[0] - center_x) > half_band) {
        continue;
      }
      min_z = std::min(min_z, vertex.position[2]);
      max_z = std::max(max_z, vertex.position[2]);
      found = true;
    }
    if (found) {
      width_out = max_z - min_z;
    }
  };

  update_band_width(min_x, 0.03F, start_width, found_start_band);
  for (const auto &vertex : mesh->get_vertices()) {
    if (std::abs(vertex.position[0] - bridge.start.x()) <= 0.03F) {
      start_max_y = std::max(start_max_y, vertex.position[1]);
      found_start_band = true;
    }
    if (std::abs(vertex.position[0]) <= 0.10F) {
      mid_max_y = std::max(mid_max_y, vertex.position[1]);
      found_mid_band = true;
    }
  }

  EXPECT_NEAR(min_x, bridge.start.x(), 0.0001F);
  EXPECT_NEAR(max_x, bridge.end.x(), 0.0001F);
  ASSERT_TRUE(found_start_band);
  ASSERT_TRUE(found_mid_band);
  EXPECT_GT(start_width, bridge.width * 0.95F);
  EXPECT_GT(mid_max_y, start_max_y + 0.05F);
}

TEST(LinearFeatureGeometryTest, BuildsRiverbankMeshWithVisibilitySamples) {
  Game::Map::TerrainHeightMap height_map(16, 16, 1.0F);
  Game::Map::RiverSegment segment;
  segment.start = QVector3D(-3.0F, 0.0F, 0.0F);
  segment.end = QVector3D(3.0F, 0.0F, 0.0F);
  segment.width = 2.0F;

  auto result = Render::Ground::build_riverbank_mesh(segment, height_map);

  ASSERT_NE(result.mesh, nullptr);
  EXPECT_FALSE(result.mesh->get_vertices().empty());
  EXPECT_FALSE(result.mesh->get_indices().empty());
  EXPECT_FALSE(result.visibility_samples.empty());
}

} // namespace
