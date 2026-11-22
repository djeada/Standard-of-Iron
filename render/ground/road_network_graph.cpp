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
#include "game/map/terrain.h"
#include "game/map/terrain_surface.h"
#include "render/ground/road_network_graph_internal.h"

namespace Render::Ground::road_network_detail {

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

} // namespace Render::Ground::road_network_detail
