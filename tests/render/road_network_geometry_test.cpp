#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <vector>

#include "game/map/terrain.h"
#include "render/gl/mesh.h"
#include "render/ground/road_network_geometry.h"

namespace {

using Render::Ground::RoadNetworkSettings;
using Render::Ground::RoadNetworkSurface;

auto flat_height_map(int size, float height) -> Game::Map::TerrainHeightMap {
  const auto cells = static_cast<std::size_t>(size * size);
  const std::vector<float> heights(cells, height);
  const std::vector<Game::Map::TerrainType> types(cells, Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(size, size, 1.0F);
  height_map.restore_from_data(heights, types, {}, {});
  return height_map;
}

auto segment(QVector3D start, QVector3D end, float width) -> Game::Map::RoadSegment {
  return {start, end, width, QStringLiteral("default")};
}

constexpr float k_sample_jitter_x = 0.0731F;
constexpr float k_sample_jitter_z = 0.0413F;

auto coverage_at(const std::vector<RoadNetworkSurface>& surfaces,
                 float sample_x,
                 float sample_z) -> int {
  const float x = sample_x + k_sample_jitter_x;
  const float z = sample_z + k_sample_jitter_z;
  int hits = 0;
  for (const auto& surface : surfaces) {
    if (surface.mesh == nullptr) {
      continue;
    }
    const auto& vertices = surface.mesh->get_vertices();
    const auto& indices = surface.mesh->get_indices();
    for (std::size_t index = 0; index + 2 < indices.size(); index += 3) {
      const auto& a = vertices[indices[index]];
      const auto& b = vertices[indices[index + 1]];
      const auto& c = vertices[indices[index + 2]];
      const float area =
          (b.position[0] - a.position[0]) * (c.position[2] - a.position[2]) -
          (c.position[0] - a.position[0]) * (b.position[2] - a.position[2]);
      if (std::abs(area) < 1e-9F) {
        continue;
      }
      const float w0 = ((b.position[0] - a.position[0]) * (z - a.position[2]) -
                        (x - a.position[0]) * (b.position[2] - a.position[2])) /
                       area;
      const float w1 = ((x - a.position[0]) * (c.position[2] - a.position[2]) -
                        (c.position[0] - a.position[0]) * (z - a.position[2])) /
                       area;
      constexpr float k_interior_margin = 1e-5F;
      if (w0 > k_interior_margin && w1 > k_interior_margin &&
          w0 + w1 < 1.0F - k_interior_margin) {
        ++hits;
      }
    }
  }
  return hits;
}

auto surface_bounds_contains(const RoadNetworkSurface& surface,
                             float x,
                             float z) -> bool {
  const auto& vertices = surface.mesh->get_vertices();
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (const auto& vertex : vertices) {
    min_x = std::min(min_x, vertex.position[0]);
    max_x = std::max(max_x, vertex.position[0]);
    min_z = std::min(min_z, vertex.position[2]);
    max_z = std::max(max_z, vertex.position[2]);
  }
  return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
}

TEST(RoadNetworkGeometryTest, MergesWaypointRunIntoOneContinuousRibbon) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> chained{
      segment({-9.0F, 0.0F, 0.0F}, {-3.0F, 0.0F, 0.0F}, 3.0F),
      segment({-3.0F, 0.0F, 0.0F}, {3.0F, 0.0F, 0.0F}, 3.0F),
      segment({3.0F, 0.0F, 0.0F}, {9.0F, 0.0F, 0.0F}, 3.0F)};

  const auto surfaces = Render::Ground::build_road_network_surfaces(chained, settings);

  ASSERT_FALSE(surfaces.empty());
  for (const auto& surface : surfaces) {
    EXPECT_FALSE(surface.junction);
  }

  EXPECT_EQ(coverage_at(surfaces, 0.0F, 0.0F), 1);
  EXPECT_EQ(coverage_at(surfaces, -3.0F, 0.0F), 1);
  EXPECT_EQ(coverage_at(surfaces, 3.0F, 0.0F), 1);
}

TEST(RoadNetworkGeometryTest, CollapsesDuplicateAuthoredRunsOntoOneSurface) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> single{
      segment({-8.0F, 0.0F, 0.0F}, {8.0F, 0.0F, 0.0F}, 3.0F)};

  const std::vector<Game::Map::RoadSegment> duplicated{
      segment({-8.0F, 0.0F, 0.0F}, {8.0F, 0.0F, 0.0F}, 3.0F),
      segment({8.0F, 0.0F, 0.0F}, {-8.0F, 0.0F, 0.0F}, 3.0F)};

  const auto single_surfaces =
      Render::Ground::build_road_network_surfaces(single, settings);
  const auto duplicated_surfaces =
      Render::Ground::build_road_network_surfaces(duplicated, settings);

  EXPECT_EQ(duplicated_surfaces.size(), single_surfaces.size());
  EXPECT_EQ(coverage_at(duplicated_surfaces, 0.0F, 0.0F), 1);
}

TEST(RoadNetworkGeometryTest, BuildsOneUnifiedSurfaceAtACrossroads) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> crossroads{
      segment({-10.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, -10.0F}, {0.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 10.0F}, 4.0F)};

  const auto surfaces =
      Render::Ground::build_road_network_surfaces(crossroads, settings);

  const int junctions = static_cast<int>(
      std::count_if(surfaces.begin(), surfaces.end(), [](const auto& surface) {
        return surface.junction;
      }));
  EXPECT_EQ(junctions, 1);

  for (float x = -3.0F; x <= 3.0F; x += 0.5F) {
    for (float z = -3.0F; z <= 3.0F; z += 0.5F) {
      EXPECT_LE(coverage_at(surfaces, x, z), 1) << "at (" << x << ", " << z << ')';
    }
  }
  EXPECT_EQ(coverage_at(surfaces, 0.0F, 0.0F), 1);
}

TEST(RoadNetworkGeometryTest, TJunctionLeavesNoUncoveredNotch) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> tee{
      segment({-10.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 10.0F}, 4.0F)};

  const auto surfaces = Render::Ground::build_road_network_surfaces(tee, settings);

  for (float x = -3.0F; x <= 3.0F; x += 0.25F) {
    EXPECT_EQ(coverage_at(surfaces, x, 0.0F), 1) << "at x=" << x;
  }

  for (float z = 0.25F; z <= 3.0F; z += 0.25F) {
    EXPECT_EQ(coverage_at(surfaces, 0.0F, z), 1) << "at z=" << z;
  }
}

TEST(RoadNetworkGeometryTest, EdgeFadeCoordinateSpansTheOuterBoundary) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> road{
      segment({-8.0F, 0.0F, 0.0F}, {8.0F, 0.0F, 0.0F}, 4.0F)};

  const auto surfaces = Render::Ground::build_road_network_surfaces(road, settings);
  ASSERT_FALSE(surfaces.empty());

  bool saw_boundary = false;
  bool saw_interior = false;
  for (const auto& surface : surfaces) {
    for (const auto& vertex : surface.mesh->get_vertices()) {
      EXPECT_GE(vertex.tex_coord[1], 0.0F);
      EXPECT_LE(vertex.tex_coord[1], 1.0F);
      saw_boundary = saw_boundary || vertex.tex_coord[1] <= 0.0F;
      saw_interior = saw_interior || vertex.tex_coord[1] >= 1.0F;
    }
  }
  EXPECT_TRUE(saw_boundary);
  EXPECT_TRUE(saw_interior);
}

TEST(RoadNetworkGeometryTest, KeepsRoadSurfaceAboveASlope) {
  const int size = 21;
  const auto cells = static_cast<std::size_t>(size * size);
  std::vector<float> heights(cells, 0.0F);
  for (int z = 0; z < size; ++z) {
    for (int x = 0; x < size; ++x) {

      const float distance = std::abs(static_cast<float>(x) - 10.0F);
      heights[static_cast<std::size_t>(z * size + x)] =
          std::max(0.0F, 6.0F - distance * 1.5F);
    }
  }
  const std::vector<Game::Map::TerrainType> types(cells, Game::Map::TerrainType::Flat);
  Game::Map::TerrainHeightMap height_map(size, size, 1.0F);
  height_map.restore_from_data(heights, types, {}, {});

  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> road{
      segment({-8.0F, 0.0F, 0.0F}, {8.0F, 0.0F, 0.0F}, 4.0F)};
  const auto surfaces = Render::Ground::build_road_network_surfaces(road, settings);
  ASSERT_FALSE(surfaces.empty());

  for (const auto& surface : surfaces) {
    for (const auto& vertex : surface.mesh->get_vertices()) {
      const float ground =
          height_map.get_height_at(vertex.position[0], vertex.position[2]);
      EXPECT_GE(vertex.position[1], ground)
          << "road sank into the slope at (" << vertex.position[0] << ", "
          << vertex.position[2] << ')';
    }
  }
}

TEST(RoadNetworkGeometryTest, LiftsApproachOntoTheBridgeDeck) {
  auto height_map = flat_height_map(61, 0.0F);
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(4.0F, 0.0F, 0.0F);
  bridge.end = QVector3D(-4.0F, 0.0F, 0.0F);
  bridge.width = Game::Map::k_min_bridge_width;
  bridge.height = 0.7F;
  const std::vector<Game::Map::Bridge> bridges{bridge};

  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.bridges = &bridges;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> road{
      segment({16.0F, 0.0F, 0.0F}, {5.5F, 0.0F, 0.0F}, 4.0F)};
  const auto surfaces = Render::Ground::build_road_network_surfaces(road, settings);
  ASSERT_FALSE(surfaces.empty());

  const float deck_y = Game::Map::bridge_deck_world_y(bridge, 0.0F);
  const float abutment = Game::Map::bridge_abutment_reach(bridge.width);
  const float deck_edge_x = bridge.start.x() + abutment;

  float highest_near_deck = std::numeric_limits<float>::lowest();
  float lowest_far_from_deck = std::numeric_limits<float>::max();
  for (const auto& surface : surfaces) {
    for (const auto& vertex : surface.mesh->get_vertices()) {
      if (std::abs(vertex.position[0] - deck_edge_x) < 0.35F) {
        highest_near_deck = std::max(highest_near_deck, vertex.position[1]);
      }
      if (vertex.position[0] > 14.0F) {
        lowest_far_from_deck = std::min(lowest_far_from_deck, vertex.position[1]);
      }
    }
  }

  ASSERT_GT(highest_near_deck, std::numeric_limits<float>::lowest());
  ASSERT_LT(lowest_far_from_deck, std::numeric_limits<float>::max());
  EXPECT_NEAR(highest_near_deck, deck_y, 0.05F);
  EXPECT_NEAR(lowest_far_from_deck, Game::Map::k_road_surface_y_offset, 0.05F);
}

TEST(RoadNetworkGeometryTest, ProvidesVisibilityProxyForEverySurface) {
  auto height_map = flat_height_map(41, 0.0F);
  RoadNetworkSettings settings;
  settings.height_map = &height_map;
  settings.tile_size = 1.0F;

  const std::vector<Game::Map::RoadSegment> tee{
      segment({-10.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, 4.0F),
      segment({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 10.0F}, 4.0F)};

  const auto surfaces = Render::Ground::build_road_network_surfaces(tee, settings);
  ASSERT_FALSE(surfaces.empty());
  for (const auto& surface : surfaces) {
    ASSERT_NE(surface.mesh, nullptr);
    EXPECT_GT(surface.visibility_width, 0.0F);
    EXPECT_TRUE(surface_bounds_contains(
        surface, surface.visibility_start.x(), surface.visibility_start.z()));
    EXPECT_TRUE(surface_bounds_contains(
        surface, surface.visibility_end.x(), surface.visibility_end.z()));
  }
}

} // namespace
