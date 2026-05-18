#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

#include "game/map/terrain.h"
#include "render/gl/mesh.h"
#include "render/ground/linear_feature_geometry.h"

namespace {

auto interpolate_triangle_height_at(const Render::GL::Vertex& a,
                                    const Render::GL::Vertex& b,
                                    const Render::GL::Vertex& c,
                                    float x,
                                    float z) -> std::optional<float> {
  float const ax = a.position[0];
  float const az = a.position[2];
  float const bx = b.position[0];
  float const bz = b.position[2];
  float const cx = c.position[0];
  float const cz = c.position[2];

  float const denom = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz);
  if (std::abs(denom) < 1e-6F) {
    return std::nullopt;
  }

  float const w0 = ((bz - cz) * (x - cx) + (cx - bx) * (z - cz)) / denom;
  float const w1 = ((cz - az) * (x - cx) + (ax - cx) * (z - cz)) / denom;
  float const w2 = 1.0F - w0 - w1;
  constexpr float k_epsilon = 1e-4F;
  if (w0 < -k_epsilon || w1 < -k_epsilon || w2 < -k_epsilon) {
    return std::nullopt;
  }

  return w0 * a.position[1] + w1 * b.position[1] + w2 * c.position[1];
}

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
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F},
      1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  EXPECT_FALSE(mesh->get_vertices().empty());
  EXPECT_FALSE(mesh->get_indices().empty());
  EXPECT_FLOAT_EQ(mesh->get_vertices().front().position[1],
                  Game::Map::k_road_surface_y_offset);
}

TEST(LinearFeatureGeometryTest, BuildsRoadRibbonMeshAlongTerrainHeightMap) {
  std::vector<float> const heights = {
      0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 0.0F, 1.0F, 2.0F,
      3.0F, 4.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F};
  std::vector<Game::Map::TerrainType> const terrain_types(heights.size(),
                                                          Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(5, 5, 1.0F);
  height_map.restore_from_data(heights, terrain_types, {}, {});

  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 1.0F;
  settings.min_length_steps = 2;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.height_map = &height_map;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F},
      1.0F,
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
  std::vector<float> const heights = {
      0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 2.0F, 2.0F, 2.0F,
      2.0F, 2.0F, 3.0F, 3.0F, 3.0F, 3.0F, 3.0F, 4.0F, 4.0F, 4.0F, 4.0F, 4.0F};
  std::vector<Game::Map::TerrainType> const terrain_types(heights.size(),
                                                          Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(5, 5, 1.0F);
  height_map.restore_from_data(heights, terrain_types, {}, {});

  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 1.0F;
  settings.min_length_steps = 2;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.height_map = &height_map;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F},
      1.0F,
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

TEST(LinearFeatureGeometryTest, SamplesRoadInteriorAboveLocalizedTerrainHump) {
  std::vector<float> const heights = {
      0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.1F, 0.2F, 0.1F, 0.0F, 0.0F, 0.2F, 1.0F,
      0.2F, 0.0F, 0.0F, 0.1F, 0.2F, 0.1F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
  std::vector<Game::Map::TerrainType> const terrain_types(heights.size(),
                                                          Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(5, 5, 1.0F);
  height_map.restore_from_data(heights, terrain_types, {}, {});

  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 1.0F;
  settings.min_length_steps = 2;
  settings.cross_section_segments = 4;
  settings.y_offset = Game::Map::k_road_surface_y_offset;
  settings.sample_terrain_envelope = true;
  settings.height_map = &height_map;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F},
      1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  auto const& vertices = mesh->get_vertices();
  auto const& indices = mesh->get_indices();
  ASSERT_FALSE(vertices.empty());
  ASSERT_FALSE(indices.empty());

  for (float world_x = -1.75F; world_x <= 1.75F; world_x += 0.25F) {
    for (float world_z = -0.90F; world_z <= 0.90F; world_z += 0.15F) {
      std::optional<float> road_height;
      for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        auto const maybe_height =
            interpolate_triangle_height_at(vertices[indices[i]],
                                           vertices[indices[i + 1]],
                                           vertices[indices[i + 2]],
                                           world_x,
                                           world_z);
        if (maybe_height.has_value()) {
          if (!road_height.has_value() || *maybe_height > *road_height) {
            road_height = *maybe_height;
          }
        }
      }

      ASSERT_TRUE(road_height.has_value());
      EXPECT_GE(
          *road_height + 1e-4F,
          Game::Map::road_surface_world_y(height_map.get_height_at(world_x, world_z)));
    }
  }
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
      {QVector3D(0.0F, 0.0F, -2.0F), QVector3D(0.0F, 0.0F, 2.0F), 3.0F},
      1.0F,
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
  for (const auto& vertex : mesh->get_vertices()) {
    min_x = std::min(min_x, vertex.position[0]);
    max_x = std::max(max_x, vertex.position[0]);
  }

  float start_width = 0.0F;
  bool found_start_band = false;
  float start_max_y = std::numeric_limits<float>::lowest();
  float mid_max_y = std::numeric_limits<float>::lowest();
  bool found_mid_band = false;
  auto update_band_width =
      [&](float center_x, float half_band, float& width_out, bool& found) {
        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();
        for (const auto& vertex : mesh->get_vertices()) {
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
  for (const auto& vertex : mesh->get_vertices()) {
    if (std::abs(vertex.position[0] - bridge.start.x()) <= 0.03F) {
      start_max_y = std::max(start_max_y, vertex.position[1]);
      found_start_band = true;
    }
    if (std::abs(vertex.position[0]) <= 0.15F) {
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
  Game::Map::TerrainHeightMap const height_map(16, 16, 1.0F);
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
