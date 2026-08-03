#include "activity_markers.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace App::Models {

namespace {

using Game::Systems::ActivityKind;
using Game::Systems::ActivityState;

auto is_standing_job(ActivityKind kind) -> bool {
  switch (kind) {
  case ActivityKind::Construct:
  case ActivityKind::Repair:
  case ActivityKind::Dismantle:
  case ActivityKind::ChopWood:
  case ActivityKind::MineStone:
  case ActivityKind::MineIron:
  case ActivityKind::Deliver:
  case ActivityKind::Train:
  case ActivityKind::Blocked:
    return true;
  default:
    return false;
  }
}

struct Cluster {
  QString activity;
  QString state;
  int count = 0;
  std::uint64_t lead_entity_id = 0;
  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_z = 0.0;

  [[nodiscard]] auto centroid_x() const -> float {
    return static_cast<float>(sum_x / std::max(1, count));
  }
  [[nodiscard]] auto centroid_z() const -> float {
    return static_cast<float>(sum_z / std::max(1, count));
  }
  [[nodiscard]] auto centroid_y() const -> float {
    return static_cast<float>(sum_y / std::max(1, count));
  }
};

auto to_marker(const Cluster& cluster) -> ActivityMarker {
  ActivityMarker marker;
  marker.activity = cluster.activity;
  marker.state = cluster.state;
  marker.count = cluster.count;
  marker.lead_entity_id = cluster.lead_entity_id;
  marker.x = cluster.centroid_x();
  marker.y = cluster.centroid_y();
  marker.z = cluster.centroid_z();
  return marker;
}

void absorb(Cluster& cluster, const ActivityMarkerSource& source) {
  if (cluster.count == 0 || source.entity_id < cluster.lead_entity_id) {
    cluster.lead_entity_id = source.entity_id;
  }
  cluster.count += 1;
  cluster.sum_x += source.x;
  cluster.sum_y += source.y;
  cluster.sum_z += source.z;
}

} // namespace

auto activity_deserves_marker(const Game::Systems::UnitActivity& activity,
                              bool selected) -> bool {
  if (!Game::Systems::activity_is_noteworthy(activity)) {
    return false;
  }
  if (selected) {
    return true;
  }

  return is_standing_job(activity.kind) || activity.state != ActivityState::Active;
}

auto group_activity_markers(const std::vector<ActivityMarkerSource>& sources,
                            const ActivityMarkerOptions& options)
    -> std::vector<ActivityMarker> {

  std::map<std::pair<QString, QString>, std::vector<Cluster>> buckets;

  const float radius_sq = options.cluster_radius * options.cluster_radius;

  for (const ActivityMarkerSource& source : sources) {
    const QString activity = QString::fromUtf8(
        Game::Systems::activity_kind_id(source.activity.kind).data(),
        static_cast<int>(Game::Systems::activity_kind_id(source.activity.kind).size()));
    const QString state = QString::fromUtf8(
        Game::Systems::activity_state_id(source.activity.state).data(),
        static_cast<int>(
            Game::Systems::activity_state_id(source.activity.state).size()));

    auto& clusters = buckets[{activity, state}];
    Cluster* match = nullptr;
    for (Cluster& cluster : clusters) {
      const float dx = cluster.centroid_x() - source.x;
      const float dz = cluster.centroid_z() - source.z;
      if (dx * dx + dz * dz <= radius_sq) {
        match = &cluster;
        break;
      }
    }
    if (match == nullptr) {
      Cluster cluster;
      cluster.activity = activity;
      cluster.state = state;
      clusters.push_back(cluster);
      match = &clusters.back();
    }
    absorb(*match, source);
  }

  std::vector<ActivityMarker> markers;
  for (const auto& [key, clusters] : buckets) {
    for (const Cluster& cluster : clusters) {
      markers.push_back(to_marker(cluster));
    }
  }

  if (static_cast<int>(markers.size()) <= options.max_markers) {
    return markers;
  }

  markers.clear();
  for (const auto& [key, clusters] : buckets) {
    Cluster merged;
    merged.activity = key.first;
    merged.state = key.second;
    for (const Cluster& cluster : clusters) {
      if (merged.count == 0 || cluster.lead_entity_id < merged.lead_entity_id) {
        merged.lead_entity_id = cluster.lead_entity_id;
      }
      merged.count += cluster.count;
      merged.sum_x += cluster.sum_x;
      merged.sum_y += cluster.sum_y;
      merged.sum_z += cluster.sum_z;
    }
    markers.push_back(to_marker(merged));
  }

  if (static_cast<int>(markers.size()) > options.max_markers) {
    std::stable_sort(markers.begin(),
                     markers.end(),
                     [](const ActivityMarker& lhs, const ActivityMarker& rhs) {
                       return lhs.count > rhs.count;
                     });
    markers.resize(static_cast<std::size_t>(options.max_markers));
  }
  return markers;
}

} // namespace App::Models
