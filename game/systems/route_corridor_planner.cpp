#include "route_corridor_planner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include "nav_grid.h"

namespace Game::Systems {

namespace {

constexpr float k_point_epsilon_sq = 1.0e-6F;
constexpr float k_center_probe_step = 0.2F;

void mix_route_id(std::uint64_t& value, std::uint64_t input) {
  value ^= input + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U);
}

void append_distinct(std::vector<QVector3D>& points, const QVector3D& point) {
  if (!points.empty()) {
    QVector3D const delta = point - points.back();
    if (delta.x() * delta.x() + delta.z() * delta.z() <= k_point_epsilon_sq) {
      return;
    }
  }
  points.push_back(point);
}

auto path_points(Pathfinding& pathfinder,
                 const QVector3D& start,
                 const QVector3D& destination,
                 Pathfinding::Passability passability,
                 float clearance) -> std::vector<QVector3D> {
  Point const start_cell = NavGrid::world_to_grid(start.x(), start.z());
  Point const destination_cell =
      NavGrid::world_to_grid(destination.x(), destination.z());
  auto const cells =
      pathfinder.find_path(start_cell, destination_cell, passability, clearance);
  if (cells.empty()) {
    return {};
  }

  std::vector<QVector3D> points;
  points.reserve(cells.size() + 1U);
  append_distinct(points, start);

  QVector3D const travel = destination - start;
  float const travel_length = std::hypot(travel.x(), travel.z());
  QVector3D const heading =
      travel_length > 1.0e-4F ? travel / travel_length : QVector3D();
  bool leading = travel_length > 1.0e-4F;
  for (auto const& cell : cells) {
    QVector3D const waypoint = pathfinder.path_waypoint_world_position(cell);
    if (leading) {
      QVector3D const offset = waypoint - start;
      float const forward = (offset.x() * heading.x()) + (offset.z() * heading.z());
      if (forward <= 0.0F) {
        continue;
      }
      leading = false;
    }
    append_distinct(points, waypoint);
  }
  if (points.size() < 2U) {

    points.clear();
    append_distinct(points, start);
  }
  if (pathfinder.is_world_segment_walkable(
          points.back(), destination, passability, clearance)) {
    append_distinct(points, destination);
  }
  return points;
}

auto append_connector(Pathfinding& pathfinder,
                      std::vector<QVector3D>& route,
                      const QVector3D& from,
                      const QVector3D& to,
                      Pathfinding::Passability passability,
                      float clearance) -> bool {
  if (pathfinder.is_world_segment_walkable(from, to, passability, clearance)) {
    append_distinct(route, to);
    return true;
  }
  auto const connector = path_points(pathfinder, from, to, passability, clearance);
  if (connector.size() < 2U) {
    return false;
  }
  for (auto const& point : connector) {
    append_distinct(route, point);
  }
  return true;
}

auto tangent_at(const std::vector<QVector3D>& centerline,
                std::size_t index) -> QVector3D {
  QVector3D tangent;
  if (index + 1U < centerline.size()) {
    tangent = centerline[index + 1U] - centerline[index];
  }
  if (tangent.lengthSquared() <= k_point_epsilon_sq && index > 0U) {
    tangent = centerline[index] - centerline[index - 1U];
  }
  tangent.setY(0.0F);
  if (tangent.lengthSquared() <= k_point_epsilon_sq) {
    return {0.0F, 0.0F, 1.0F};
  }
  return tangent.normalized();
}

void center_constrained_waypoints(Pathfinding& pathfinder,
                                  std::vector<QVector3D>& points,
                                  Pathfinding::Passability passability,
                                  float clearance) {
  if (points.size() < 3U) {
    return;
  }
  float const probe_limit = std::clamp(clearance * 2.0F + 1.0F, 2.0F, 8.0F);
  auto const original = points;
  std::vector<float> center_offsets(points.size(), 0.0F);
  std::vector<bool> has_center_offset(points.size(), false);
  for (std::size_t index = 1U; index + 1U < points.size(); ++index) {
    QVector3D const tangent = tangent_at(original, index);
    QVector3D const lateral(tangent.z(), 0.0F, -tangent.x());
    auto probe_side = [&](float sign) {
      float clear_distance = 0.0F;
      for (float distance = k_center_probe_step; distance <= probe_limit;
           distance += k_center_probe_step) {
        if (!pathfinder.is_world_position_walkable(
                original[index] + lateral * (distance * sign), passability)) {
          return std::pair(clear_distance, true);
        }
        clear_distance = distance;
      }
      return std::pair(clear_distance, false);
    };
    auto const [left_clearance, left_bounded] = probe_side(-1.0F);
    auto const [right_clearance, right_bounded] = probe_side(1.0F);
    if (!left_bounded || !right_bounded) {
      continue;
    }
    center_offsets[index] = (right_clearance - left_clearance) * 0.5F;
    has_center_offset[index] = true;
  }

  float const center_hold_distance = std::max(2.5F, clearance + 1.0F);
  float const center_fade_distance = center_hold_distance + 3.0F;
  std::vector<float> route_distance(points.size(), 0.0F);
  for (std::size_t index = 1U; index < points.size(); ++index) {
    route_distance[index] =
        route_distance[index - 1U] + (original[index] - original[index - 1U]).length();
  }
  auto smoothed_offsets = center_offsets;
  for (std::size_t source = 1U; source + 1U < points.size(); ++source) {
    if (!has_center_offset[source]) {
      continue;
    }
    for (std::size_t candidate = 1U; candidate + 1U < points.size(); ++candidate) {
      float const distance =
          std::abs(route_distance[candidate] - route_distance[source]);
      if (distance >= center_fade_distance) {
        continue;
      }
      float const weight = distance <= center_hold_distance
                               ? 1.0F
                               : (center_fade_distance - distance) /
                                     (center_fade_distance - center_hold_distance);
      float const offset = center_offsets[source] * weight;
      if (std::abs(offset) > std::abs(smoothed_offsets[candidate])) {
        smoothed_offsets[candidate] = offset;
      }
    }
  }

  auto centered = original;
  for (std::size_t index = 1U; index + 1U < centered.size(); ++index) {
    QVector3D const tangent = tangent_at(original, index);
    QVector3D const lateral(tangent.z(), 0.0F, -tangent.x());
    centered[index] += lateral * smoothed_offsets[index];
    if (!pathfinder.is_world_position_walkable(centered[index], passability)) {
      return;
    }
  }
  for (std::size_t index = 1U; index < centered.size(); ++index) {
    if (!pathfinder.is_world_segment_walkable(
            centered[index - 1U], centered[index], passability)) {
      return;
    }
  }
  points = std::move(centered);
}

} // namespace

auto RouteCorridorPlanner::plan(Pathfinding& pathfinder,
                                const QVector3D& start,
                                const QVector3D& destination,
                                Pathfinding::Passability passability,
                                float clearance) -> RouteCorridorPlan {
  RouteCorridorPlan result;
  pathfinder.update_navigation_grid();
  result.centerline =
      path_points(pathfinder, start, destination, passability, clearance);
  if (result.reachable()) {
    center_constrained_waypoints(pathfinder, result.centerline, passability, clearance);
    result.id = identity(result.centerline, pathfinder.navigation_revision());
  }
  return result;
}

auto RouteCorridorPlanner::identity(const std::vector<QVector3D>& points,
                                    std::uint64_t topology_revision) -> std::uint64_t {
  if (points.empty()) {
    return 0U;
  }
  std::uint64_t result = 0xcbf29ce484222325ULL;
  mix_route_id(result, topology_revision);
  for (auto const& point : points) {
    Point const cell = NavGrid::world_to_grid(point.x(), point.z());
    mix_route_id(result, static_cast<std::uint32_t>(cell.x));
    mix_route_id(result, static_cast<std::uint32_t>(cell.y));
  }
  return result == 0U ? 1U : result;
}

auto RouteCorridorPlanner::fit_lane(Pathfinding& pathfinder,
                                    const RouteCorridorPlan& corridor,
                                    const QVector3D& member_start,
                                    const QVector3D& member_destination,
                                    float lateral_offset,
                                    Pathfinding::Passability passability,
                                    float clearance) -> RouteLanePlan {
  RouteLanePlan result;
  result.lateral_offset = lateral_offset;
  if (!corridor.reachable()) {
    return result;
  }

  std::size_t entry_index = 0;
  {
    float best = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < corridor.centerline.size(); ++index) {
      float const distance =
          (corridor.centerline[index] - member_start).lengthSquared();
      if (distance < best) {
        best = distance;
        entry_index = index;
      }
    }

    if (entry_index + 1U >= corridor.centerline.size() &&
        corridor.centerline.size() >= 2U) {
      entry_index = corridor.centerline.size() - 2U;
    }
  }

  std::vector<QVector3D> lane_points;
  lane_points.reserve(corridor.centerline.size());
  float previous_scale = 1.0F;
  bool awaiting_reform = false;

  constexpr std::array<float, 5> k_scale_steps{1.0F, 0.75F, 0.5F, 0.3F, 0.16F};
  for (std::size_t index = entry_index; index < corridor.centerline.size(); ++index) {
    QVector3D const tangent = tangent_at(corridor.centerline, index);
    QVector3D const right(tangent.z(), 0.0F, -tangent.x());
    QVector3D const previous = lane_points.empty() ? member_start : lane_points.back();
    float const max_scale = std::min(1.0F, previous_scale + 0.25F);

    bool placed = false;
    for (float const scale : k_scale_steps) {
      if (scale > max_scale + 0.001F) {
        continue;
      }
      QVector3D const candidate =
          corridor.centerline[index] + right * (lateral_offset * scale);
      if (!pathfinder.is_world_position_walkable(candidate, passability, clearance) ||
          !pathfinder.is_world_segment_walkable(
              previous, candidate, passability, clearance)) {
        continue;
      }
      append_distinct(lane_points, candidate);
      previous_scale = scale;
      result.minimum_lateral_scale = std::min(result.minimum_lateral_scale, scale);
      if (scale < 0.99F && !result.opening_point.has_value()) {
        result.opening_point = candidate;
      }
      if (scale < 0.99F) {
        awaiting_reform = true;
        result.reform_point.reset();
      } else if (awaiting_reform) {
        result.reform_point = candidate;
        awaiting_reform = false;
      }
      placed = true;
      break;
    }
    if (!placed) {

      QVector3D const center = corridor.centerline[index];
      if (!append_connector(
              pathfinder, lane_points, previous, center, passability, clearance)) {
        return result;
      }
      previous_scale = k_scale_steps.back();
      result.minimum_lateral_scale = 0.0F;
      if (!result.opening_point.has_value()) {
        result.opening_point = center;
      }
      awaiting_reform = true;
      result.reform_point.reset();
    }
  }

  result.waypoints.reserve(lane_points.size() + 4U);
  append_distinct(result.waypoints, member_start);
  if (!append_connector(pathfinder,
                        result.waypoints,
                        member_start,
                        lane_points.front(),
                        passability,
                        clearance)) {
    result.waypoints.clear();
    return result;
  }
  for (auto const& point : lane_points) {
    append_distinct(result.waypoints, point);
  }
  if (!append_connector(pathfinder,
                        result.waypoints,
                        result.waypoints.back(),
                        member_destination,
                        passability,
                        clearance)) {
    result.waypoints.clear();
  }
  if (result.opening_point.has_value() && !result.reform_point.has_value()) {
    result.reform_point = member_destination;
  }
  return result;
}

} // namespace Game::Systems
