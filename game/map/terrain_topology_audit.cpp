#include "terrain_topology_audit.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace Game::Map {
namespace {

class DisjointSet {
public:
  explicit DisjointSet(std::size_t count)
      : m_parent(count) {
    std::iota(m_parent.begin(), m_parent.end(), std::size_t{0});
  }

  auto find(std::size_t value) -> std::size_t {
    while (m_parent[value] != value) {
      m_parent[value] = m_parent[m_parent[value]];
      value = m_parent[value];
    }
    return value;
  }

  void join(std::size_t a, std::size_t b) {
    a = find(a);
    b = find(b);
    if (a != b) {
      m_parent[b] = a;
    }
  }

private:
  std::vector<std::size_t> m_parent;
};

auto xz_distance_sq(const QVector3D& a, const QVector3D& b) -> float {
  const float dx = a.x() - b.x();
  const float dz = a.z() - b.z();
  return dx * dx + dz * dz;
}

auto segment_distance_sq(const QVector3D& point,
                         const QVector3D& start,
                         const QVector3D& end) -> float {
  QVector2D const p(point.x(), point.z());
  QVector2D const a(start.x(), start.z());
  QVector2D const b(end.x(), end.z());
  QVector2D const ab = b - a;
  const float length_sq = ab.lengthSquared();
  const float t =
      length_sq > 0.0001F
          ? std::clamp(QVector2D::dotProduct(p - a, ab) / length_sq, 0.0F, 1.0F)
          : 0.0F;
  return (p - (a + ab * t)).lengthSquared();
}

auto segments_intersect(const QVector3D& a0,
                        const QVector3D& a1,
                        const QVector3D& b0,
                        const QVector3D& b1) -> bool {
  auto cross =
      [](const QVector3D& origin, const QVector3D& first, const QVector3D& second) {
        return (first.x() - origin.x()) * (second.z() - origin.z()) -
               (first.z() - origin.z()) * (second.x() - origin.x());
      };
  const float ab0 = cross(a0, a1, b0);
  const float ab1 = cross(a0, a1, b1);
  const float ba0 = cross(b0, b1, a0);
  const float ba1 = cross(b0, b1, a1);
  constexpr float epsilon = 0.0001F;
  const bool collinear = std::abs(ab0) <= epsilon && std::abs(ab1) <= epsilon &&
                         std::abs(ba0) <= epsilon && std::abs(ba1) <= epsilon;
  if (collinear) {
    const auto overlaps = [](float a_start, float a_end, float b_start, float b_end) {
      constexpr float overlap_epsilon = 0.0001F;
      return std::max(std::min(a_start, a_end), std::min(b_start, b_end)) <=
             std::min(std::max(a_start, a_end), std::max(b_start, b_end)) +
                 overlap_epsilon;
    };
    return overlaps(a0.x(), a1.x(), b0.x(), b1.x()) &&
           overlaps(a0.z(), a1.z(), b0.z(), b1.z());
  }
  return ((ab0 <= epsilon && ab1 >= -epsilon) || (ab1 <= epsilon && ab0 >= -epsilon)) &&
         ((ba0 <= epsilon && ba1 >= -epsilon) || (ba1 <= epsilon && ba0 >= -epsilon));
}

template <typename Segment>
auto segment_component_count(const std::vector<Segment>& segments,
                             float join_distance,
                             std::size_t counted_segment_count =
                                 std::numeric_limits<std::size_t>::max()) -> int {
  if (segments.empty()) {
    return 0;
  }
  DisjointSet sets(segments.size());
  const float join_sq = join_distance * join_distance;
  for (std::size_t first = 0; first < segments.size(); ++first) {
    for (std::size_t second = first + 1; second < segments.size(); ++second) {
      if (segments_intersect(segments[first].start,
                             segments[first].end,
                             segments[second].start,
                             segments[second].end) ||
          segment_distance_sq(segments[first].start,
                              segments[second].start,
                              segments[second].end) <= join_sq ||
          segment_distance_sq(segments[first].end,
                              segments[second].start,
                              segments[second].end) <= join_sq ||
          segment_distance_sq(segments[second].start,
                              segments[first].start,
                              segments[first].end) <= join_sq ||
          segment_distance_sq(segments[second].end,
                              segments[first].start,
                              segments[first].end) <= join_sq) {
        sets.join(first, second);
      }
    }
  }
  std::vector<std::size_t> roots;
  const std::size_t count = std::min(counted_segment_count, segments.size());
  roots.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    roots.push_back(sets.find(index));
  }
  std::sort(roots.begin(), roots.end());
  return static_cast<int>(std::unique(roots.begin(), roots.end()) - roots.begin());
}

auto river_channels_touch(const RiverSegment& first,
                          const RiverSegment& second,
                          float tolerance) -> bool {
  const float reach = first.width * 0.5F + second.width * 0.5F + tolerance;
  const float reach_sq = reach * reach;
  return segments_intersect(first.start, first.end, second.start, second.end) ||
         segment_distance_sq(first.start, second.start, second.end) <= reach_sq ||
         segment_distance_sq(first.end, second.start, second.end) <= reach_sq ||
         segment_distance_sq(second.start, first.start, first.end) <= reach_sq ||
         segment_distance_sq(second.end, first.start, first.end) <= reach_sq;
}

auto river_component_count(const std::vector<RiverSegment>& rivers,
                           float tolerance) -> int {
  if (rivers.empty()) {
    return 0;
  }
  DisjointSet sets(rivers.size());
  for (std::size_t first = 0; first < rivers.size(); ++first) {
    for (std::size_t second = first + 1; second < rivers.size(); ++second) {
      if (river_channels_touch(rivers[first], rivers[second], tolerance)) {
        sets.join(first, second);
      }
    }
  }
  std::vector<std::size_t> roots;
  roots.reserve(rivers.size());
  for (std::size_t index = 0; index < rivers.size(); ++index) {
    roots.push_back(sets.find(index));
  }
  std::sort(roots.begin(), roots.end());
  return static_cast<int>(std::unique(roots.begin(), roots.end()) - roots.begin());
}

auto touches_map_boundary(const QVector3D& point,
                          const MapDefinition& map,
                          float tolerance) -> bool {
  const float half_width = (map.grid.width - 1) * map.grid.tile_size * 0.5F;
  const float half_height = (map.grid.height - 1) * map.grid.tile_size * 0.5F;
  return std::abs(point.x()) >= half_width - tolerance ||
         std::abs(point.z()) >= half_height - tolerance;
}

auto touches_lake(const QVector3D& point,
                  const std::vector<Lake>& lakes,
                  float tolerance) -> bool {
  for (const auto& lake : lakes) {
    if (point_on_lake_boundary(
            lake, point.x(), point.z(), std::max(tolerance, 0.25F))) {
      return true;
    }
  }
  return false;
}

auto entrance_cluster_count(const TerrainFeature& hill, float tile_size) -> int {
  if (hill.entrances.empty()) {
    return 0;
  }
  const float extent = std::max(1.0F, std::min(hill.width, hill.depth));
  const float cluster_distance = std::max(tile_size * 2.5F, extent * 0.12F);
  const float cluster_sq = cluster_distance * cluster_distance;
  DisjointSet sets(hill.entrances.size());
  for (std::size_t first = 0; first < hill.entrances.size(); ++first) {
    for (std::size_t second = first + 1; second < hill.entrances.size(); ++second) {
      if (xz_distance_sq(hill.entrances[first], hill.entrances[second]) <= cluster_sq) {
        sets.join(first, second);
      }
    }
  }
  std::vector<std::size_t> roots;
  roots.reserve(hill.entrances.size());
  for (std::size_t index = 0; index < hill.entrances.size(); ++index) {
    roots.push_back(sets.find(index));
  }
  std::sort(roots.begin(), roots.end());
  return static_cast<int>(std::unique(roots.begin(), roots.end()) - roots.begin());
}

} // namespace

auto audit_terrain_topology(const MapDefinition& map) -> TerrainTopologyAudit {
  TerrainTopologyAudit audit;
  const float tile_size = std::max(map.grid.tile_size, 0.1F);
  const float join_distance = tile_size * 1.75F;

  std::vector<RoadSegment> navigable_roads = map.roads;
  navigable_roads.reserve(map.roads.size() + map.bridges.size());
  for (const auto& bridge : map.bridges) {
    navigable_roads.push_back(
        {bridge.start, bridge.end, bridge.width, QStringLiteral("paved")});
  }
  audit.road_components =
      segment_component_count(navigable_roads, join_distance, map.roads.size());
  if (audit.road_components > 1) {
    audit.issues.push_back(QStringLiteral("road network has %1 disconnected components")
                               .arg(audit.road_components));
  }

  audit.river_components = river_component_count(map.rivers, join_distance);
  for (std::size_t segment_index = 0; segment_index < map.rivers.size();
       ++segment_index) {
    const auto& segment = map.rivers[segment_index];
    for (const QVector3D* endpoint : {&segment.start, &segment.end}) {
      bool joined = false;
      for (std::size_t other_index = 0; other_index < map.rivers.size();
           ++other_index) {
        if (other_index == segment_index) {
          continue;
        }
        const auto& other = map.rivers[other_index];
        const float bank_reach = other.width * 0.5F + join_distance;
        if (segment_distance_sq(*endpoint, other.start, other.end) <=
            bank_reach * bank_reach) {
          joined = true;
          break;
        }
      }
      const float water_tolerance = std::max(tile_size * 2.0F, segment.width);
      if (!joined && !touches_map_boundary(*endpoint, map, water_tolerance) &&
          !touches_lake(*endpoint, map.lakes, water_tolerance)) {
        ++audit.invalid_river_endpoints;
      }
    }
  }
  if (audit.invalid_river_endpoints > 0) {
    audit.issues.push_back(QStringLiteral("%1 river endpoints terminate inside the map")
                               .arg(audit.invalid_river_endpoints));
  }

  for (const auto& feature : map.terrain) {
    if (feature.type == TerrainType::Hill &&
        entrance_cluster_count(feature, tile_size) < 2) {
      ++audit.hills_without_two_approaches;
    }
  }
  if (audit.hills_without_two_approaches > 0) {
    audit.issues.push_back(QStringLiteral("%1 tactical hills have fewer than two "
                                          "distinct defended approaches")
                               .arg(audit.hills_without_two_approaches));
  }

  for (const auto& lake : map.lakes) {
    const float lake_extent = std::max(lake.width, lake.depth) * 0.5F;
    bool anchored = touches_map_boundary(lake.center, map, lake_extent + tile_size);
    if (!anchored) {
      const float influence = lake_extent + tile_size * 3.0F;
      for (const auto& road : map.roads) {
        if (segment_distance_sq(lake.center, road.start, road.end) <=
            influence * influence) {
          anchored = true;
          break;
        }
      }
    }
    if (!anchored) {
      ++audit.tactically_unanchored_lakes;
    }
  }
  if (audit.tactically_unanchored_lakes > 0) {
    audit.issues.push_back(QStringLiteral("%1 lakes neither constrain a route nor "
                                          "anchor a map boundary")
                               .arg(audit.tactically_unanchored_lakes));
  }

  return audit;
}

} // namespace Game::Map
