#include "mist_volume_builder.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Render {

namespace {

void simplify_polyline(const std::vector<QVector2D>& points,
                       std::size_t first,
                       std::size_t last,
                       float epsilon,
                       std::vector<bool>& keep) {
  if (last <= first + 1) {
    return;
  }
  const QVector2D chord = points[last] - points[first];
  const float chord_length = chord.length();
  float worst_distance = -1.0F;
  std::size_t worst_index = first;
  for (std::size_t i = first + 1; i < last; ++i) {
    const QVector2D offset = points[i] - points[first];
    const float distance =
        chord_length > 1e-4F
            ? std::abs(offset.x() * chord.y() - offset.y() * chord.x()) / chord_length
            : offset.length();
    if (distance > worst_distance) {
      worst_distance = distance;
      worst_index = i;
    }
  }
  if (worst_distance > epsilon) {
    keep[worst_index] = true;
    simplify_polyline(points, first, worst_index, epsilon, keep);
    simplify_polyline(points, worst_index, last, epsilon, keep);
  }
}

} // namespace

auto build_mist_volumes(const MistSources& sources,
                        const MistSurfaceHeight& surface_y) -> std::vector<MistVolume> {
  std::vector<MistVolume> volumes;
  const auto height_at = [&surface_y](float world_x, float world_z) {
    return surface_y ? surface_y(world_x, world_z) : 0.0F;
  };

  if (sources.fog_zones != nullptr) {
    for (const auto& zone : *sources.fog_zones) {
      MistVolume mist;
      const float base_y = height_at(zone.x, zone.z);
      mist.start = QVector3D(zone.x, base_y, zone.z);
      mist.end = mist.start;
      mist.radius = std::max(std::max(zone.width, zone.height) * 0.5F, 1.0F);
      mist.strength = std::clamp(k_miasma_strength_base +
                                     zone.density * k_miasma_strength_per_density,
                                 0.0F,
                                 0.85F);
      mist.kind = MistVolume::Kind::Miasma;
      volumes.push_back(mist);
    }
  }

  if (sources.rivers != nullptr) {
    const auto& rivers = *sources.rivers;
    constexpr float k_join_epsilon_sq = 0.25F;
    std::size_t river_index = 0;
    while (river_index < rivers.size()) {
      const float width = rivers[river_index].width;
      std::vector<QVector2D> points;
      points.emplace_back(rivers[river_index].start.x(), rivers[river_index].start.z());
      while (river_index < rivers.size()) {
        const auto& segment = rivers[river_index];
        const QVector2D seg_start(segment.start.x(), segment.start.z());
        if ((seg_start - points.back()).lengthSquared() > k_join_epsilon_sq) {
          break;
        }
        points.emplace_back(segment.end.x(), segment.end.z());
        ++river_index;
      }

      std::vector<bool> keep(points.size(), false);
      keep.front() = true;
      keep.back() = true;
      simplify_polyline(
          points, 0, points.size() - 1, std::max(1.5F, width * 0.4F), keep);

      QVector2D previous = points.front();
      for (std::size_t i = 1; i < points.size(); ++i) {
        if (!keep[i]) {
          continue;
        }
        const QVector2D mid = (previous + points[i]) * 0.5F;
        MistVolume mist;
        const float base_y = height_at(mid.x(), mid.y());
        mist.start = QVector3D(previous.x(), base_y, previous.y());
        mist.end = QVector3D(points[i].x(), base_y, points[i].y());
        mist.radius = std::max(width * 0.5F, 0.5F);
        mist.strength = k_water_mist_strength;
        mist.kind = MistVolume::Kind::WaterMist;
        volumes.push_back(mist);
        previous = points[i];
      }
    }
  }

  if (sources.lakes != nullptr) {
    for (const auto& lake : *sources.lakes) {
      MistVolume mist;
      const float base_y = height_at(lake.center.x(), lake.center.z());
      mist.start = QVector3D(lake.center.x(), base_y, lake.center.z());
      mist.end = mist.start;
      mist.radius = std::max(std::max(lake.width, lake.depth) * 0.5F, 1.0F);
      mist.strength = k_water_mist_strength;
      mist.kind = MistVolume::Kind::WaterMist;
      volumes.push_back(mist);
    }
  }

  if (volumes.size() > static_cast<std::size_t>(k_max_mist_volumes)) {
    volumes.resize(static_cast<std::size_t>(k_max_mist_volumes));
  }
  return volumes;
}

auto merge_mist_volumes(std::vector<MistVolume> priority,
                        const std::vector<MistVolume>& rest)
    -> std::vector<MistVolume> {
  for (const auto& volume : rest) {
    if (priority.size() >= static_cast<std::size_t>(k_max_mist_volumes)) {
      break;
    }
    priority.push_back(volume);
  }
  if (priority.size() > static_cast<std::size_t>(k_max_mist_volumes)) {
    priority.resize(static_cast<std::size_t>(k_max_mist_volumes));
  }
  return priority;
}

} // namespace Render
