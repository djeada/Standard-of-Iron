#pragma once

#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain.h"
#include "game/map/terrain_surface.h"
#include "render/ground/road_network_geometry.h"

namespace Render::Ground::road_network_detail {

constexpr float k_epsilon = 1.0e-4F;
constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;

constexpr float k_max_mitre_turn_cos = 0.82F;
constexpr std::size_t k_lateral_samples = 7;

[[nodiscard]] inline auto flat(const QVector3D& value) -> QVector2D {
  return {value.x(), value.z()};
}

[[nodiscard]] inline auto cross_2d(const QVector2D& a, const QVector2D& b) -> float {
  return a.x() * b.y() - a.y() * b.x();
}

[[nodiscard]] inline auto perpendicular_of(const QVector2D& dir) -> QVector2D {
  return {-dir.y(), dir.x()};
}

[[nodiscard]] inline auto
sample_height_clamped(const Game::Map::TerrainHeightMap& height_map,
                      float world_x,
                      float world_z) -> float {
  const auto& heights = height_map.get_height_data();
  const int grid_width = height_map.get_width();
  const int grid_height = height_map.get_height();
  const float tile_size = std::max(height_map.get_tile_size(), 1.0e-4F);
  if (heights.empty() || grid_width <= 0 || grid_height <= 0) {
    return 0.0F;
  }

  const float gx =
      (world_x / tile_size) + (static_cast<float>(grid_width) * 0.5F - 0.5F);
  const float gz =
      (world_z / tile_size) + (static_cast<float>(grid_height) * 0.5F - 0.5F);
  return Game::Map::sample_triangulated_height(
      heights.data(), grid_width, grid_height, gx, gz);
}

[[nodiscard]] inline auto
sample_surface_height(const Game::Map::TerrainHeightMap& height_map,
                      const QVector2D& position,
                      float footprint) -> float {
  const float tile_size = std::max(height_map.get_tile_size(), 1.0e-4F);
  const float radius =
      std::max(footprint, tile_size * Game::Map::k_road_surface_envelope_tiles);
  float highest = sample_height_clamped(height_map, position.x(), position.y());
  constexpr int k_taps = Game::Map::k_road_surface_envelope_taps;
  for (int index = 0; index < k_taps; ++index) {
    const float angle =
        k_two_pi * static_cast<float>(index) / static_cast<float>(k_taps);
    highest = std::max(highest,
                       sample_height_clamped(height_map,
                                             position.x() + std::cos(angle) * radius,
                                             position.y() + std::sin(angle) * radius));
  }
  return highest;
}

struct GraphNode {
  QVector2D position;
  std::vector<int> incident;
  bool junction = false;
  bool bridge_head = false;
  float bridge_deck_y = 0.0F;
  float bridge_half_width = 0.0F;
};

struct GraphEdge {
  int node_a = 0;
  int node_b = 0;
  float width = 3.0F;
  QString style;
  bool consumed = false;
};

struct RoadGraph {
  std::vector<GraphNode> nodes;
  std::vector<GraphEdge> edges;
};

struct Chain {
  std::vector<int> nodes;
  float width = 3.0F;
  float length = 0.0F;
  QString style;
};

struct JunctionArm {
  QVector2D direction;
  float half_width = 1.0F;
  float angle = 0.0F;
  float trim = 0.0F;
  float max_trim = 0.0F;
  int chain = -1;
  bool chain_start = false;
};

[[nodiscard]] auto build_graph(const std::vector<Game::Map::RoadSegment>& segments,
                               float tile_size) -> RoadGraph;
void split_crossing_edges(RoadGraph& graph, float tolerance);
[[nodiscard]] auto other_end(const GraphEdge& edge, int node) -> int;
[[nodiscard]] auto
direction_from(const RoadGraph& graph, int from, int to) -> QVector2D;
void classify_junctions(RoadGraph& graph);
[[nodiscard]] auto build_chains(RoadGraph& graph) -> std::vector<Chain>;

} // namespace Render::Ground::road_network_detail
