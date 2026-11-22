#include "render/ground/road_network_geometry.h"

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

namespace Render::Ground {

using namespace road_network_detail;

namespace {

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
  return std::clamp(road_width * 0.22F, 0.16F, 0.90F);
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
