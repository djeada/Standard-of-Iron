#include "road_network_geometry.h"

#include <QVector2D>

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

namespace Render::Ground {

namespace {

constexpr float k_epsilon = 1.0e-4F;
constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;

constexpr float k_max_mitre_turn_cos = 0.82F;
constexpr std::size_t k_lateral_samples = 7;

[[nodiscard]] auto flat(const QVector3D& value) -> QVector2D {
  return {value.x(), value.z()};
}

[[nodiscard]] auto cross_2d(const QVector2D& a, const QVector2D& b) -> float {
  return a.x() * b.y() - a.y() * b.x();
}

[[nodiscard]] auto perpendicular_of(const QVector2D& dir) -> QVector2D {
  return {-dir.y(), dir.x()};
}

[[nodiscard]] auto sample_height_clamped(const Game::Map::TerrainHeightMap& height_map,
                                         float world_x,
                                         float world_z) -> float {
  const auto& heights = height_map.get_height_data();
  const int grid_width = height_map.get_width();
  const int grid_height = height_map.get_height();
  const float tile_size = std::max(height_map.get_tile_size(), 1.0e-4F);
  if (heights.empty() || grid_width <= 0 || grid_height <= 0) {
    return 0.0F;
  }

  float gx = (world_x / tile_size) + (static_cast<float>(grid_width) * 0.5F - 0.5F);
  float gz = (world_z / tile_size) + (static_cast<float>(grid_height) * 0.5F - 0.5F);
  gx = std::clamp(gx, 0.0F, static_cast<float>(grid_width - 1));
  gz = std::clamp(gz, 0.0F, static_cast<float>(grid_height - 1));

  const int x0 = static_cast<int>(std::floor(gx));
  const int z0 = static_cast<int>(std::floor(gz));
  const int x1 = std::min(x0 + 1, grid_width - 1);
  const int z1 = std::min(z0 + 1, grid_height - 1);
  const float tx = gx - static_cast<float>(x0);
  const float tz = gz - static_cast<float>(z0);

  const float h00 = heights[static_cast<std::size_t>(z0 * grid_width + x0)];
  const float h10 = heights[static_cast<std::size_t>(z0 * grid_width + x1)];
  const float h01 = heights[static_cast<std::size_t>(z1 * grid_width + x0)];
  const float h11 = heights[static_cast<std::size_t>(z1 * grid_width + x1)];
  return (h00 * (1.0F - tx) + h10 * tx) * (1.0F - tz) +
         (h01 * (1.0F - tx) + h11 * tx) * tz;
}

[[nodiscard]] auto sample_surface_height(const Game::Map::TerrainHeightMap& height_map,
                                         const QVector2D& position,
                                         float footprint) -> float {
  const float tile_size = std::max(height_map.get_tile_size(), 1.0e-4F);
  const float radius = std::max(footprint, tile_size * 0.35F);
  float highest = sample_height_clamped(height_map, position.x(), position.y());
  constexpr int k_taps = 4;
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

class NodeGrid {
public:
  explicit NodeGrid(float cell_size)
      : m_cell_size(std::max(cell_size, 0.05F)) {}

  [[nodiscard]] auto find(const std::vector<GraphNode>& nodes,
                          const QVector2D& position,
                          float tolerance) const -> int {
    const int cx = cell_of(position.x());
    const int cz = cell_of(position.y());
    int best = -1;
    float best_distance_sq = tolerance * tolerance;
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dx = -1; dx <= 1; ++dx) {
        const auto bucket = m_buckets.find(key_of(cx + dx, cz + dz));
        if (bucket == m_buckets.end()) {
          continue;
        }
        for (const int candidate : bucket->second) {
          const float distance_sq =
              (nodes[static_cast<std::size_t>(candidate)].position - position)
                  .lengthSquared();
          if (distance_sq <= best_distance_sq) {
            best_distance_sq = distance_sq;
            best = candidate;
          }
        }
      }
    }
    return best;
  }

  void insert(const QVector2D& position, int index) {
    m_buckets[key_of(cell_of(position.x()), cell_of(position.y()))].push_back(index);
  }

private:
  [[nodiscard]] auto cell_of(float value) const -> int {
    return static_cast<int>(std::floor(value / m_cell_size));
  }
  [[nodiscard]] static auto key_of(int x, int z) -> std::int64_t {
    return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
  }

  float m_cell_size;
  std::unordered_map<std::int64_t, std::vector<int>> m_buckets;
};

[[nodiscard]] auto build_graph(const std::vector<Game::Map::RoadSegment>& segments,
                               float tile_size) -> RoadGraph {
  RoadGraph graph;
  const float weld_tolerance = std::max(tile_size * 0.45F, 0.05F);
  NodeGrid grid(std::max(weld_tolerance * 2.0F, 0.2F));

  auto node_for = [&](const QVector3D& world) {
    const QVector2D position = flat(world);
    const int existing = grid.find(graph.nodes, position, weld_tolerance);
    if (existing >= 0) {
      return existing;
    }
    const int index = static_cast<int>(graph.nodes.size());
    graph.nodes.push_back(GraphNode{position, {}, false, false, 0.0F});
    grid.insert(position, index);
    return index;
  };

  std::unordered_map<std::int64_t, int> edge_lookup;
  for (const auto& segment : segments) {
    const int a = node_for(segment.start);
    const int b = node_for(segment.end);
    if (a == b) {
      continue;
    }
    const auto key = (static_cast<std::int64_t>(std::min(a, b)) << 32) |
                     static_cast<std::uint32_t>(std::max(a, b));
    const auto existing = edge_lookup.find(key);
    if (existing != edge_lookup.end()) {
      auto& edge = graph.edges[static_cast<std::size_t>(existing->second)];
      edge.width = std::max(edge.width, segment.width);
      continue;
    }
    const int index = static_cast<int>(graph.edges.size());
    graph.edges.push_back(GraphEdge{a, b, segment.width, segment.style, false});
    edge_lookup.emplace(key, index);
    graph.nodes[static_cast<std::size_t>(a)].incident.push_back(index);
    graph.nodes[static_cast<std::size_t>(b)].incident.push_back(index);
  }
  return graph;
}

class EdgeGrid {
public:
  EdgeGrid(const RoadGraph& graph, float cell_size)
      : m_cell_size(std::max(cell_size, 1.0F)) {
    for (std::size_t index = 0; index < graph.edges.size(); ++index) {
      const auto& edge = graph.edges[index];
      const QVector2D a = graph.nodes[static_cast<std::size_t>(edge.node_a)].position;
      const QVector2D b = graph.nodes[static_cast<std::size_t>(edge.node_b)].position;
      for_each_cell(a, b, [&](std::int64_t key) {
        m_buckets[key].push_back(static_cast<int>(index));
      });
    }
  }

  template <typename Visitor>
  void visit(const QVector2D& a, const QVector2D& b, const Visitor& visitor) const {
    m_seen.clear();
    for_each_cell(a, b, [&](std::int64_t key) {
      const auto bucket = m_buckets.find(key);
      if (bucket == m_buckets.end()) {
        return;
      }
      for (const int index : bucket->second) {
        if (m_seen.insert(index).second) {
          visitor(index);
        }
      }
    });
  }

private:
  template <typename Fn>
  void for_each_cell(const QVector2D& a, const QVector2D& b, const Fn& fn) const {
    const int min_x = cell_of(std::min(a.x(), b.x()));
    const int max_x = cell_of(std::max(a.x(), b.x()));
    const int min_z = cell_of(std::min(a.y(), b.y()));
    const int max_z = cell_of(std::max(a.y(), b.y()));
    for (int z = min_z - 1; z <= max_z + 1; ++z) {
      for (int x = min_x - 1; x <= max_x + 1; ++x) {
        fn(key_of(x, z));
      }
    }
  }
  [[nodiscard]] auto cell_of(float value) const -> int {
    return static_cast<int>(std::floor(value / m_cell_size));
  }
  [[nodiscard]] static auto key_of(int x, int z) -> std::int64_t {
    return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
  }

  float m_cell_size;
  std::unordered_map<std::int64_t, std::vector<int>> m_buckets;
  mutable std::unordered_set<int> m_seen;
};

void split_crossing_edges(RoadGraph& graph, float tolerance) {
  const float tolerance_sq = tolerance * tolerance;
  NodeGrid intersection_grid(std::max(tolerance * 2.0F, 0.2F));
  for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
    intersection_grid.insert(graph.nodes[index].position, static_cast<int>(index));
  }

  auto node_at = [&](const QVector2D& position) {
    const int existing = intersection_grid.find(graph.nodes, position, tolerance);
    if (existing >= 0) {
      return existing;
    }
    graph.nodes.push_back(GraphNode{position, {}, false, false, 0.0F, 0.0F});
    const int index = static_cast<int>(graph.nodes.size() - 1U);
    intersection_grid.insert(position, index);
    return index;
  };

  constexpr int k_max_passes = 3;
  for (int pass = 0; pass < k_max_passes; ++pass) {

    std::unordered_map<int, std::vector<std::pair<float, int>>> splits;

    const auto record = [&](int edge_index, float t, int node_index) {
      auto& list = splits[edge_index];
      for (const auto& existing : list) {
        if (std::abs(existing.first - t) < 1.0e-4F) {
          return;
        }
      }
      list.emplace_back(t, node_index);
    };

    const std::size_t edge_count = graph.edges.size();
    const EdgeGrid grid(graph, std::max(tolerance * 12.0F, 8.0F));

    for (std::size_t node_index = 0; node_index < graph.nodes.size(); ++node_index) {
      const QVector2D point = graph.nodes[node_index].position;
      grid.visit(point, point, [&](int edge_index) {
        const auto& edge = graph.edges[static_cast<std::size_t>(edge_index)];
        if (static_cast<int>(node_index) == edge.node_a ||
            static_cast<int>(node_index) == edge.node_b) {
          return;
        }
        const QVector2D a = graph.nodes[static_cast<std::size_t>(edge.node_a)].position;
        const QVector2D b = graph.nodes[static_cast<std::size_t>(edge.node_b)].position;
        const QVector2D delta = b - a;
        const float length_sq = delta.lengthSquared();
        if (length_sq <= tolerance_sq * 4.0F) {
          return;
        }
        const float margin = tolerance / std::sqrt(length_sq);
        const float t = QVector2D::dotProduct(point - a, delta) / length_sq;
        if (t <= margin || t >= 1.0F - margin) {
          return;
        }
        if ((a + delta * t - point).lengthSquared() > tolerance_sq) {
          return;
        }
        record(edge_index, t, static_cast<int>(node_index));
      });
    }

    for (std::size_t first = 0; first < edge_count; ++first) {
      const auto edge_a = graph.edges[first];
      const QVector2D a0 =
          graph.nodes[static_cast<std::size_t>(edge_a.node_a)].position;
      const QVector2D a1 =
          graph.nodes[static_cast<std::size_t>(edge_a.node_b)].position;
      const QVector2D delta_a = a1 - a0;
      grid.visit(a0, a1, [&](int second) {
        if (static_cast<std::size_t>(second) <= first) {
          return;
        }
        const auto edge_b = graph.edges[static_cast<std::size_t>(second)];
        if (edge_b.node_a == edge_a.node_a || edge_b.node_a == edge_a.node_b ||
            edge_b.node_b == edge_a.node_a || edge_b.node_b == edge_a.node_b) {
          return;
        }
        const QVector2D b0 =
            graph.nodes[static_cast<std::size_t>(edge_b.node_a)].position;
        const QVector2D b1 =
            graph.nodes[static_cast<std::size_t>(edge_b.node_b)].position;
        if (std::max(a0.x(), a1.x()) < std::min(b0.x(), b1.x()) ||
            std::max(b0.x(), b1.x()) < std::min(a0.x(), a1.x()) ||
            std::max(a0.y(), a1.y()) < std::min(b0.y(), b1.y()) ||
            std::max(b0.y(), b1.y()) < std::min(a0.y(), a1.y())) {
          return;
        }
        const QVector2D delta_b = b1 - b0;
        const float determinant = cross_2d(delta_a, delta_b);
        if (std::abs(determinant) < 1.0e-6F) {
          return;
        }
        const QVector2D offset = b0 - a0;
        const float ta = cross_2d(offset, delta_b) / determinant;
        const float tb = cross_2d(offset, delta_a) / determinant;
        const float margin_a = tolerance / std::max(delta_a.length(), tolerance);
        const float margin_b = tolerance / std::max(delta_b.length(), tolerance);
        if (ta <= margin_a || ta >= 1.0F - margin_a || tb <= margin_b ||
            tb >= 1.0F - margin_b) {
          return;
        }
        const int shared = node_at(a0 + delta_a * ta);
        record(static_cast<int>(first), ta, shared);
        record(second, tb, shared);
      });
    }

    if (splits.empty()) {
      return;
    }

    std::vector<GraphEdge> rebuilt;
    rebuilt.reserve(graph.edges.size() + splits.size() * 2U);
    for (std::size_t edge_index = 0; edge_index < graph.edges.size(); ++edge_index) {
      const auto edge = graph.edges[edge_index];
      const auto found = splits.find(static_cast<int>(edge_index));
      if (found == splits.end()) {
        rebuilt.push_back(edge);
        continue;
      }
      auto points = found->second;
      std::sort(points.begin(), points.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
      });
      int previous = edge.node_a;
      for (const auto& point : points) {
        if (point.second == previous) {
          continue;
        }
        rebuilt.push_back(
            GraphEdge{previous, point.second, edge.width, edge.style, false});
        previous = point.second;
      }
      if (previous != edge.node_b) {
        rebuilt.push_back(
            GraphEdge{previous, edge.node_b, edge.width, edge.style, false});
      }
    }

    graph.edges = std::move(rebuilt);
    for (auto& node : graph.nodes) {
      node.incident.clear();
    }
    for (std::size_t edge_index = 0; edge_index < graph.edges.size(); ++edge_index) {
      const auto& edge = graph.edges[edge_index];
      graph.nodes[static_cast<std::size_t>(edge.node_a)].incident.push_back(
          static_cast<int>(edge_index));
      graph.nodes[static_cast<std::size_t>(edge.node_b)].incident.push_back(
          static_cast<int>(edge_index));
    }
  }
}

[[nodiscard]] auto other_end(const GraphEdge& edge, int node) -> int {
  return edge.node_a == node ? edge.node_b : edge.node_a;
}

[[nodiscard]] auto
direction_from(const RoadGraph& graph, int from, int to) -> QVector2D {
  QVector2D delta = graph.nodes[static_cast<std::size_t>(to)].position -
                    graph.nodes[static_cast<std::size_t>(from)].position;
  const float length = delta.length();
  if (length < k_epsilon) {
    return {1.0F, 0.0F};
  }
  return delta / length;
}

void classify_junctions(RoadGraph& graph) {
  for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
    auto& node = graph.nodes[index];
    if (node.incident.size() != 2U) {
      node.junction = node.incident.size() > 2U;
      continue;
    }
    const auto& first = graph.edges[static_cast<std::size_t>(node.incident[0])];
    const auto& second = graph.edges[static_cast<std::size_t>(node.incident[1])];
    if (first.style != second.style ||
        std::abs(first.width - second.width) >
            std::max(first.width, second.width) * 0.15F) {
      node.junction = true;
      continue;
    }
    const auto self = static_cast<int>(index);
    const QVector2D incoming = direction_from(graph, other_end(first, self), self);
    const QVector2D outgoing = direction_from(graph, self, other_end(second, self));
    node.junction = QVector2D::dotProduct(incoming, outgoing) < k_max_mitre_turn_cos;
  }
}

[[nodiscard]] auto build_chains(RoadGraph& graph) -> std::vector<Chain> {
  std::vector<Chain> chains;

  auto walk = [&](int start_node, int start_edge) {
    Chain chain;
    chain.width = graph.edges[static_cast<std::size_t>(start_edge)].width;
    chain.style = graph.edges[static_cast<std::size_t>(start_edge)].style;
    chain.nodes.push_back(start_node);

    int current_node = start_node;
    int current_edge = start_edge;
    while (true) {
      auto& edge = graph.edges[static_cast<std::size_t>(current_edge)];
      edge.consumed = true;
      chain.width = std::max(chain.width, edge.width);
      const int next_node = other_end(edge, current_node);
      chain.nodes.push_back(next_node);
      const auto& node = graph.nodes[static_cast<std::size_t>(next_node)];
      if (node.junction || node.incident.size() != 2U) {
        break;
      }
      const int continuation =
          node.incident[0] == current_edge ? node.incident[1] : node.incident[0];
      if (graph.edges[static_cast<std::size_t>(continuation)].consumed) {
        break;
      }
      current_node = next_node;
      current_edge = continuation;
    }
    if (chain.nodes.size() < 2U) {
      return;
    }
    for (std::size_t index = 1; index < chain.nodes.size(); ++index) {
      chain.length +=
          (graph.nodes[static_cast<std::size_t>(chain.nodes[index])].position -
           graph.nodes[static_cast<std::size_t>(chain.nodes[index - 1U])].position)
              .length();
    }
    chains.push_back(std::move(chain));
  };

  for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
    const auto& node = graph.nodes[index];
    if (!node.junction && node.incident.size() == 2U) {
      continue;
    }

    const std::vector<int> incident = node.incident;
    for (const int edge : incident) {
      if (!graph.edges[static_cast<std::size_t>(edge)].consumed) {
        walk(static_cast<int>(index), edge);
      }
    }
  }

  for (std::size_t index = 0; index < graph.edges.size(); ++index) {
    if (graph.edges[index].consumed) {
      continue;
    }
    walk(graph.edges[index].node_a, static_cast<int>(index));
  }
  return chains;
}

struct Station {
  QVector2D center;
  QVector2D normal;
  float half_width = 1.0F;
  float distance = 0.0F;
  float fade_scale = 1.0F;
  float deck_blend = 0.0F;
  float deck_y = 0.0F;
  float deck_half_width = 0.0F;
};

struct LateralSample {
  float offset = 0.0F;
  float fade = 0.0F;
};

[[nodiscard]] auto lateral_profile(float half_width, float fade_width)
    -> std::array<LateralSample, k_lateral_samples> {
  const float band =
      std::clamp(fade_width / std::max(half_width, k_epsilon), 0.04F, 0.45F);
  const float shoulder = 1.0F - band;
  return {LateralSample{-1.0F, 0.0F},
          LateralSample{-shoulder, 1.0F},
          LateralSample{-shoulder * 0.5F, 1.0F},
          LateralSample{0.0F, 1.0F},
          LateralSample{shoulder * 0.5F, 1.0F},
          LateralSample{shoulder, 1.0F},
          LateralSample{1.0F, 0.0F}};
}

struct MeshBuilder {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  auto push(const QVector2D& position,
            float height,
            float across,
            float fade) -> unsigned int {
    Render::GL::Vertex vertex{};
    vertex.position = {position.x(), height, position.y()};
    vertex.normal = {0.0F, 1.0F, 0.0F};
    vertex.tex_coord = {across, fade};
    vertices.push_back(vertex);
    return static_cast<unsigned int>(vertices.size() - 1U);
  }

  void triangle(unsigned int a, unsigned int b, unsigned int c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
  }

  void quad(unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }

  [[nodiscard]] auto build() const -> std::unique_ptr<Render::GL::Mesh> {
    if (vertices.empty() || indices.empty()) {
      return nullptr;
    }
    return std::make_unique<Render::GL::Mesh>(vertices, indices);
  }
};

struct SurfaceContext {
  const Game::Map::TerrainHeightMap* height_map = nullptr;
  float tile_size = 1.0F;
  float y_offset = 0.02F;
};

[[nodiscard]] auto surface_height(const SurfaceContext& context,
                                  const QVector2D& position,
                                  float footprint) -> float {
  if (context.height_map == nullptr) {
    return context.y_offset;
  }
  return sample_surface_height(*context.height_map, position, footprint) +
         context.y_offset;
}

[[nodiscard]] auto build_stations(const RoadGraph& graph,
                                  const Chain& chain,
                                  float step,
                                  float trim_start,
                                  float trim_end) -> std::vector<Station> {
  std::vector<QVector2D> points;
  points.reserve(chain.nodes.size());
  for (const int node : chain.nodes) {
    points.push_back(graph.nodes[static_cast<std::size_t>(node)].position);
  }

  std::vector<QVector2D> directions;
  std::vector<float> lengths;
  float total_length = 0.0F;
  for (std::size_t index = 1; index < points.size(); ++index) {
    QVector2D delta = points[index] - points[index - 1U];
    const float length = delta.length();
    lengths.push_back(length);
    directions.push_back(length > k_epsilon ? delta / length : QVector2D(1.0F, 0.0F));
    total_length += length;
  }
  if (total_length <= k_epsilon || lengths.empty()) {
    return {};
  }

  const float half_width = std::max(chain.width * 0.5F, 0.05F);
  const float span_start = std::min(trim_start, total_length * 0.45F);
  const float span_end = total_length - std::min(trim_end, total_length * 0.45F);
  if (span_end - span_start <= k_epsilon) {
    return {};
  }

  std::vector<float> stops;
  const int sample_count =
      std::max(2, static_cast<int>(std::ceil((span_end - span_start) / step)) + 1);
  stops.reserve(static_cast<std::size_t>(sample_count) + points.size());
  for (int index = 0; index < sample_count; ++index) {
    stops.push_back(span_start +
                    (span_end - span_start) * (static_cast<float>(index) /
                                               static_cast<float>(sample_count - 1)));
  }
  float running = 0.0F;
  for (std::size_t index = 0; index + 1U < lengths.size(); ++index) {
    running += lengths[index];
    if (running > span_start + k_epsilon && running < span_end - k_epsilon) {
      stops.push_back(running);
    }
  }
  std::sort(stops.begin(), stops.end());
  stops.erase(
      std::unique(stops.begin(),
                  stops.end(),
                  [&](float a, float b) { return std::abs(a - b) < step * 0.15F; }),
      stops.end());

  std::vector<Station> stations;
  stations.reserve(stops.size());
  for (const float distance : stops) {
    float remaining = distance;
    std::size_t segment = 0;
    while (segment + 1U < lengths.size() && remaining > lengths[segment]) {
      remaining -= lengths[segment];
      ++segment;
    }
    const float local = std::clamp(remaining, 0.0F, lengths[segment]);
    const QVector2D center = points[segment] + directions[segment] * local;

    QVector2D normal = perpendicular_of(directions[segment]);
    float mitre = 1.0F;
    const bool at_vertex_start = local <= k_epsilon && segment > 0;
    const bool at_vertex_end =
        local >= lengths[segment] - k_epsilon && segment + 1U < lengths.size();
    if (at_vertex_start || at_vertex_end) {
      const QVector2D incoming =
          at_vertex_start ? directions[segment - 1U] : directions[segment];
      const QVector2D outgoing =
          at_vertex_start ? directions[segment] : directions[segment + 1U];
      QVector2D blended = perpendicular_of(incoming) + perpendicular_of(outgoing);
      if (blended.lengthSquared() > k_epsilon) {
        blended.normalize();
        normal = blended;
        mitre =
            1.0F /
            std::max(QVector2D::dotProduct(normal, perpendicular_of(incoming)), 0.55F);
      }
    }

    Station station;
    station.center = center;
    station.normal = normal;

    const float wobble =
        (value_noise(center.x() * 0.13F, center.y() * 0.13F) - 0.5F) * 0.14F;
    station.half_width = half_width * mitre * (1.0F + wobble);
    station.distance = distance;
    stations.push_back(station);
  }
  return stations;
}

void apply_dead_end_fade(std::vector<Station>& stations,
                         float road_width,
                         bool fade_start,
                         bool fade_end) {
  if (stations.size() < 2U || (!fade_start && !fade_end)) {
    return;
  }
  const float first = stations.front().distance;
  const float last = stations.back().distance;
  const float taper = std::min(road_width * 1.6F, (last - first) * 0.35F);
  if (taper <= k_epsilon) {
    return;
  }
  for (auto& station : stations) {
    float scale = 1.0F;
    if (fade_start) {
      scale = std::min(scale, (station.distance - first) / taper);
    }
    if (fade_end) {
      scale = std::min(scale, (last - station.distance) / taper);
    }
    scale = std::clamp(scale, 0.0F, 1.0F);
    station.fade_scale = scale * scale * (3.0F - 2.0F * scale);
    station.half_width *= 0.80F + 0.20F * station.fade_scale;
  }
}

void apply_bridge_blend(std::vector<Station>& stations,
                        const GraphNode& start_node,
                        const GraphNode& end_node,
                        float blend_distance) {
  if (stations.empty() || blend_distance <= k_epsilon) {
    return;
  }
  const float first = stations.front().distance;
  const float last = stations.back().distance;
  for (auto& station : stations) {
    auto blend_towards = [&](float weight, const GraphNode& node) {
      const float smoothed = std::clamp(weight, 0.0F, 1.0F);
      const float eased = smoothed * smoothed * (3.0F - 2.0F * smoothed);
      if (eased > station.deck_blend) {
        station.deck_blend = eased;
        station.deck_y = node.bridge_deck_y;
        station.deck_half_width = node.bridge_half_width;
      }
    };
    if (start_node.bridge_head) {
      blend_towards(1.0F - (station.distance - first) / blend_distance, start_node);
    }
    if (end_node.bridge_head) {
      blend_towards(1.0F - (last - station.distance) / blend_distance, end_node);
    }

    if (station.deck_blend > 0.0F && station.deck_half_width > station.half_width) {
      station.half_width +=
          (station.deck_half_width - station.half_width) * station.deck_blend;
    }
  }
}

[[nodiscard]] auto station_height(const SurfaceContext& context,
                                  const Station& station,
                                  const QVector2D& position,
                                  float footprint) -> float {
  const float terrain = surface_height(context, position, footprint);
  if (station.deck_blend <= 0.0F) {
    return terrain;
  }
  return terrain + (station.deck_y - terrain) * station.deck_blend;
}

[[nodiscard]] auto
build_chain_mesh(const SurfaceContext& context,
                 const std::vector<Station>& stations,
                 std::size_t first,
                 std::size_t last,
                 float road_width) -> std::unique_ptr<Render::GL::Mesh> {
  if (last <= first) {
    return nullptr;
  }
  const float fade_width = road_edge_fade_width(road_width);
  MeshBuilder builder;
  std::array<unsigned int, k_lateral_samples> previous_row{};
  std::array<unsigned int, k_lateral_samples> row{};
  bool has_previous = false;

  for (std::size_t index = first; index <= last; ++index) {
    const Station& station = stations[index];
    const auto profile = lateral_profile(station.half_width, fade_width);
    const float footprint =
        std::max(station.half_width / static_cast<float>(k_lateral_samples - 1U),
                 context.tile_size * 0.30F);
    for (std::size_t lateral = 0; lateral < k_lateral_samples; ++lateral) {
      const auto& sample = profile[lateral];
      const QVector2D position =
          station.center + station.normal * (sample.offset * station.half_width);
      row[lateral] = builder.push(position,
                                  station_height(context, station, position, footprint),
                                  (sample.offset + 1.0F) * 0.5F,
                                  sample.fade * station.fade_scale);
    }
    if (has_previous) {
      for (std::size_t lateral = 0; lateral + 1U < k_lateral_samples; ++lateral) {
        builder.quad(previous_row[lateral],
                     row[lateral],
                     row[lateral + 1U],
                     previous_row[lateral + 1U]);
      }
    }
    previous_row = row;
    has_previous = true;
  }
  return builder.build();
}

struct JunctionBoundaryPoint {
  QVector2D position;
  float across = 0.5F;
  float fade = 0.0F;
  bool on_road_boundary = false;
};

[[nodiscard]] auto distance_to_rim(const std::vector<JunctionBoundaryPoint>& boundary,
                                   const QVector2D& point) -> float {
  float best = std::numeric_limits<float>::max();
  const std::size_t count = boundary.size();
  for (std::size_t index = 0; index < count; ++index) {
    const auto& current = boundary[index];
    const auto& next = boundary[(index + 1U) % count];
    if (!current.on_road_boundary) {
      continue;
    }
    if (!next.on_road_boundary) {
      best = std::min(best, (point - current.position).length());
      continue;
    }
    const QVector2D edge = next.position - current.position;
    const float length_sq = edge.lengthSquared();
    float t = 0.0F;
    if (length_sq > k_epsilon) {
      t = std::clamp(QVector2D::dotProduct(point - current.position, edge) / length_sq,
                     0.0F,
                     1.0F);
    }
    best = std::min(best, (point - (current.position + edge * t)).length());
  }
  return best == std::numeric_limits<float>::max() ? 0.0F : best;
}

[[nodiscard]] auto
build_junction_boundary(const std::vector<JunctionArm>& arms,
                        const QVector2D& center,
                        float fade_width) -> std::vector<JunctionBoundaryPoint> {
  std::vector<JunctionBoundaryPoint> boundary;
  boundary.reserve(arms.size() * (k_lateral_samples + 4U));

  for (std::size_t index = 0; index < arms.size(); ++index) {
    const auto& arm = arms[index];
    const QVector2D normal = perpendicular_of(arm.direction);
    const QVector2D chord_center = center + arm.direction * arm.trim;
    const auto profile = lateral_profile(arm.half_width, fade_width);
    for (const auto& sample : profile) {
      JunctionBoundaryPoint point;
      point.position = chord_center + normal * (sample.offset * arm.half_width);
      point.across = (sample.offset + 1.0F) * 0.5F;
      point.fade = sample.fade;
      point.on_road_boundary = sample.fade <= 0.0F;
      boundary.push_back(point);
    }

    const auto& next_arm = arms[(index + 1U) % arms.size()];
    const QVector2D from = chord_center + normal * arm.half_width;
    const QVector2D to = center + next_arm.direction * next_arm.trim -
                         perpendicular_of(next_arm.direction) * next_arm.half_width;
    if ((to - from).length() <= fade_width * 0.35F) {
      continue;
    }

    QVector2D control = (from + to) * 0.5F;
    const float determinant = cross_2d(arm.direction, next_arm.direction);
    if (std::abs(determinant) > 1.0e-3F) {
      const float along = cross_2d(to - from, next_arm.direction) / determinant;
      const QVector2D candidate = from + arm.direction * along;
      if ((candidate - center).length() <
          std::max(arm.half_width, next_arm.half_width) * 3.0F) {
        control = candidate;
      }
    }
    constexpr int k_fillet_samples = 4;
    for (int sample = 1; sample <= k_fillet_samples; ++sample) {
      const float t =
          static_cast<float>(sample) / static_cast<float>(k_fillet_samples + 1);
      const float inv = 1.0F - t;
      JunctionBoundaryPoint point;
      point.position = from * (inv * inv) + control * (2.0F * inv * t) + to * (t * t);
      point.across = 0.5F;
      point.fade = 0.0F;
      point.on_road_boundary = true;
      boundary.push_back(point);
    }
  }

  std::vector<JunctionBoundaryPoint> compacted;
  compacted.reserve(boundary.size());
  for (const auto& point : boundary) {
    if (!compacted.empty() &&
        (compacted.back().position - point.position).lengthSquared() < 1.0e-6F) {
      compacted.back().on_road_boundary =
          compacted.back().on_road_boundary || point.on_road_boundary;
      continue;
    }
    compacted.push_back(point);
  }
  return compacted;
}

[[nodiscard]] auto
build_junction_mesh(const SurfaceContext& context,
                    const QVector2D& center,
                    const std::vector<JunctionArm>& arms,
                    float road_width) -> std::unique_ptr<Render::GL::Mesh> {
  if (arms.size() < 2U) {
    return nullptr;
  }
  const float fade_width = road_edge_fade_width(road_width);
  const auto boundary = build_junction_boundary(arms, center, fade_width);
  if (boundary.size() < 3U) {
    return nullptr;
  }

  MeshBuilder builder;
  const float footprint = std::max(road_width * 0.22F, context.tile_size * 0.30F);
  const unsigned int center_index =
      builder.push(center, surface_height(context, center, footprint), 0.5F, 1.0F);

  constexpr std::array<float, 2> k_rings{0.55F, 1.0F};
  std::array<std::vector<unsigned int>, k_rings.size()> rings;
  for (std::size_t ring = 0; ring < k_rings.size(); ++ring) {
    rings[ring].reserve(boundary.size());
    for (const auto& point : boundary) {
      const QVector2D position = center + (point.position - center) * k_rings[ring];
      const float fade =
          ring + 1U == k_rings.size()
              ? point.fade
              : std::clamp(
                    distance_to_rim(boundary, position) / fade_width, 0.0F, 1.0F);
      rings[ring].push_back(builder.push(
          position, surface_height(context, position, footprint), point.across, fade));
    }
  }

  const std::size_t count = boundary.size();
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t next = (index + 1U) % count;
    builder.triangle(center_index, rings[0][index], rings[0][next]);
    for (std::size_t ring = 0; ring + 1U < rings.size(); ++ring) {
      builder.quad(rings[ring][index],
                   rings[ring + 1U][index],
                   rings[ring + 1U][next],
                   rings[ring][next]);
    }
  }
  return builder.build();
}

void resolve_junction_trims(std::vector<JunctionArm>& arms, float min_trim) {
  for (std::size_t index = 0; index < arms.size(); ++index) {
    auto& arm = arms[index];
    float trim = min_trim;
    for (std::size_t other = 0; other < arms.size(); ++other) {
      if (other == index) {
        continue;
      }
      const auto& neighbour = arms[other];
      const float determinant = cross_2d(arm.direction, neighbour.direction);
      if (std::abs(determinant) < 1.0e-3F) {
        continue;
      }
      QVector2D arm_normal = perpendicular_of(arm.direction);
      if (QVector2D::dotProduct(arm_normal, neighbour.direction) < 0.0F) {
        arm_normal = -arm_normal;
      }
      QVector2D neighbour_normal = perpendicular_of(neighbour.direction);
      if (QVector2D::dotProduct(neighbour_normal, arm.direction) < 0.0F) {
        neighbour_normal = -neighbour_normal;
      }
      const QVector2D offset =
          neighbour_normal * neighbour.half_width - arm_normal * arm.half_width;
      trim = std::max(trim, cross_2d(offset, neighbour.direction) / determinant);
    }
    const float ceiling =
        std::max(min_trim, std::min(arm.half_width * 6.0F, arm.max_trim));
    arm.trim = std::clamp(trim, min_trim, std::max(ceiling, min_trim));
  }
}

void snap_chain_ends_to_bridges(RoadGraph& graph,
                                const std::vector<Game::Map::Bridge>& bridges,
                                float tile_size) {
  for (auto& node : graph.nodes) {
    if (node.incident.size() != 1U) {
      continue;
    }
    float best_distance = std::numeric_limits<float>::max();
    QVector2D best_position;
    float best_deck = 0.0F;
    float best_half_width = 0.0F;
    for (const auto& bridge : bridges) {
      QVector2D axis = flat(bridge.end) - flat(bridge.start);
      if (axis.lengthSquared() < k_epsilon) {
        continue;
      }
      axis.normalize();

      const float abutment = Game::Map::bridge_abutment_reach(bridge.width);
      const float tolerance = std::max(bridge.width * 0.75F, tile_size * 3.0F);
      struct Head {
        QVector2D anchor;
        QVector2D outward;
        float deck_y;
      };
      const std::array<Head, 2> heads{
          Head{flat(bridge.start), -axis, Game::Map::bridge_deck_world_y(bridge, 0.0F)},
          Head{flat(bridge.end), axis, Game::Map::bridge_deck_world_y(bridge, 1.0F)}};
      for (const auto& head : heads) {
        const QVector2D offset = node.position - head.anchor;

        if (QVector2D::dotProduct(offset, head.outward) < -abutment) {
          continue;
        }
        const float distance = offset.length();
        if (distance < tolerance && distance < best_distance) {
          best_distance = distance;
          best_position = head.anchor + head.outward * abutment;
          best_deck = head.deck_y;
          best_half_width = bridge.width * 0.5F;
        }
      }
    }
    if (best_distance < std::numeric_limits<float>::max()) {
      node.position = best_position;
      node.bridge_head = true;
      node.bridge_deck_y = best_deck;
      node.bridge_half_width = best_half_width;
    }
  }
}

} // namespace

auto road_edge_fade_width(float road_width) -> float {
  return std::clamp(road_width * 0.09F, 0.12F, 0.45F);
}

auto build_road_network_surfaces(const std::vector<Game::Map::RoadSegment>& segments,
                                 const RoadNetworkSettings& settings)
    -> std::vector<RoadNetworkSurface> {
  std::vector<RoadNetworkSurface> surfaces;
  if (segments.empty()) {
    return surfaces;
  }

  const float tile_size = std::max(settings.tile_size, 0.05F);
  RoadGraph graph = build_graph(segments, tile_size);
  if (graph.edges.empty()) {
    return surfaces;
  }
  split_crossing_edges(graph, std::max(tile_size * 0.75F, 0.25F));
  classify_junctions(graph);
  if (settings.bridges != nullptr) {
    snap_chain_ends_to_bridges(graph, *settings.bridges, tile_size);
  }

  const std::vector<Chain> chains = build_chains(graph);
  if (chains.empty()) {
    return surfaces;
  }

  std::unordered_map<int, std::vector<JunctionArm>> junction_arms;
  for (std::size_t chain_index = 0; chain_index < chains.size(); ++chain_index) {
    const auto& chain = chains[chain_index];
    const float half_width = std::max(chain.width * 0.5F, 0.05F);
    const auto add_arm = [&](int node, int neighbour, bool at_start) {
      if (graph.nodes[static_cast<std::size_t>(node)].incident.size() < 2U) {
        return;
      }
      JunctionArm arm;
      arm.direction = direction_from(graph, node, neighbour);
      arm.half_width = half_width;
      arm.max_trim = chain.length * 0.42F;
      arm.chain = static_cast<int>(chain_index);
      arm.chain_start = at_start;
      junction_arms[node].push_back(arm);
    };
    add_arm(chain.nodes.front(), chain.nodes[1], true);
    add_arm(chain.nodes.back(), chain.nodes[chain.nodes.size() - 2U], false);
  }

  for (auto& entry : junction_arms) {
    auto& arms = entry.second;
    for (auto& arm : arms) {
      arm.angle = std::atan2(arm.direction.y(), arm.direction.x());
    }
    std::sort(arms.begin(), arms.end(), [](const JunctionArm& a, const JunctionArm& b) {
      return a.angle < b.angle;
    });
    float min_trim = 0.0F;
    for (const auto& arm : arms) {
      min_trim = std::max(min_trim, arm.half_width * 0.65F);
    }
    resolve_junction_trims(arms, min_trim);
  }

  SurfaceContext context;
  context.height_map = settings.height_map;
  context.tile_size = tile_size;
  context.y_offset = settings.y_offset;

  const float step = std::max(tile_size * 0.6F, 0.2F);
  constexpr std::size_t k_stations_per_mesh = 64;

  for (std::size_t chain_index = 0; chain_index < chains.size(); ++chain_index) {
    const auto& chain = chains[chain_index];
    const int start_node = chain.nodes.front();
    const int end_node = chain.nodes.back();

    const auto trim_for = [&](int node, bool at_start) {
      const auto found = junction_arms.find(node);
      if (found == junction_arms.end()) {
        return 0.0F;
      }
      for (const auto& arm : found->second) {
        if (arm.chain == static_cast<int>(chain_index) && arm.chain_start == at_start) {
          return arm.trim;
        }
      }
      return 0.0F;
    };

    auto stations = build_stations(
        graph, chain, step, trim_for(start_node, true), trim_for(end_node, false));
    if (stations.size() < 2U) {
      continue;
    }
    const auto& start_graph_node = graph.nodes[static_cast<std::size_t>(start_node)];
    const auto& end_graph_node = graph.nodes[static_cast<std::size_t>(end_node)];
    apply_dead_end_fade(
        stations,
        chain.width,
        start_graph_node.incident.size() == 1U && !start_graph_node.bridge_head,
        end_graph_node.incident.size() == 1U && !end_graph_node.bridge_head);
    apply_bridge_blend(stations,
                       start_graph_node,
                       end_graph_node,
                       std::max(chain.width * 2.0F, tile_size * 4.0F));

    for (std::size_t first = 0; first + 1U < stations.size();
         first += k_stations_per_mesh) {
      const std::size_t last =
          std::min(first + k_stations_per_mesh, stations.size() - 1U);
      auto mesh = build_chain_mesh(context, stations, first, last, chain.width);
      if (mesh == nullptr) {
        continue;
      }
      RoadNetworkSurface surface;
      surface.mesh = std::move(mesh);
      surface.style = chain.style;
      surface.visibility_start =
          QVector3D(stations[first].center.x(), 0.0F, stations[first].center.y());
      surface.visibility_end =
          QVector3D(stations[last].center.x(), 0.0F, stations[last].center.y());
      surface.visibility_width = chain.width;
      surfaces.push_back(std::move(surface));
    }
  }

  for (const auto& entry : junction_arms) {
    const auto& arms = entry.second;
    if (arms.size() < 2U) {
      continue;
    }
    float road_width = 0.0F;

    const JunctionArm* dominant = &arms.front();
    for (const auto& arm : arms) {
      road_width = std::max(road_width, arm.half_width * 2.0F);
      if (arm.half_width > dominant->half_width) {
        dominant = &arm;
      }
    }
    const QVector2D center =
        graph.nodes[static_cast<std::size_t>(entry.first)].position;
    auto mesh = build_junction_mesh(context, center, arms, road_width);
    if (mesh == nullptr) {
      continue;
    }
    RoadNetworkSurface surface;
    surface.mesh = std::move(mesh);
    surface.style = chains[static_cast<std::size_t>(dominant->chain)].style;
    surface.visibility_start = QVector3D(center.x(), 0.0F, center.y());
    surface.visibility_end = surface.visibility_start;
    surface.visibility_width = road_width;
    surface.junction = true;
    surfaces.push_back(std::move(surface));
  }

  return surfaces;
}

} // namespace Render::Ground
