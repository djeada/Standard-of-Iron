#include "walkability.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "building_collision_registry.h"
#include "gate_service.h"
#include "nav_grid.h"

namespace Game::Systems::Walkability {

namespace {

using CellValue = Pathfinding::CellValue;

auto registry() -> const BuildingCollisionRegistry& {
  return BuildingCollisionRegistry::instance();
}

constexpr float k_bearing_samples = 24.0F;
constexpr float k_two_pi = 6.28318530718F;

auto padding_covers_cell(const Pathfinding& pathfinder,
                         int grid_x,
                         int grid_z) -> bool {
  constexpr float k_cell_slack = 1.0F;
  float const cell_x = static_cast<float>(grid_x) + pathfinder.get_grid_offset_x();
  float const cell_z = static_cast<float>(grid_z) + pathfinder.get_grid_offset_z();
  auto const& buildings = registry();
  auto covers = [cell_x, cell_z](const BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return false;
    }
    float const half_width =
        (footprint.width * 0.5F) + footprint.grid_padding + k_cell_slack;
    float const half_depth =
        (footprint.depth * 0.5F) + footprint.grid_padding + k_cell_slack;
    return std::abs(cell_x - footprint.center_x) <= half_width &&
           std::abs(cell_z - footprint.center_z) <= half_depth;
  };
  for (auto const& building : buildings.get_all_buildings()) {
    if (covers(building)) {
      return true;
    }
  }
  for (auto const& obstacle : buildings.authored_obstacles()) {
    if (covers(obstacle)) {
      return true;
    }
  }
  return false;
}

auto building_body_penetration(float x, float z, float radius) -> float {
  auto const& buildings = registry();
  float deepest = 0.0F;
  auto measure = [x, z, radius, &deepest](const BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return;
    }
    float const half_width = footprint.body_width * 0.5F;
    float const half_depth = footprint.body_depth * 0.5F;
    float const closest_x = std::clamp(
        x, footprint.body_center_x - half_width, footprint.body_center_x + half_width);
    float const closest_z = std::clamp(
        z, footprint.body_center_z - half_depth, footprint.body_center_z + half_depth);
    float const dx = x - closest_x;
    float const dz = z - closest_z;
    float const distance = std::hypot(dx, dz);
    deepest = std::max(deepest, radius - distance);
  };
  for (auto const& building : buildings.get_all_buildings()) {
    measure(building);
  }
  for (auto const& obstacle : buildings.authored_obstacles()) {
    measure(obstacle);
  }
  return std::max(0.0F, deepest);
}

auto cell_is_open(const Pathfinding& pathfinder,
                  int grid_x,
                  int grid_z,
                  const BodyProfile& profile) -> bool {
  if (pathfinder.is_walkable(grid_x, grid_z, profile.passability)) {
    return true;
  }
  if (!profile.stops_at_building_facade) {
    return false;
  }

  if (!pathfinder.is_terrain_walkable(grid_x, grid_z)) {
    return false;
  }
  auto const value = pathfinder.cell_value(grid_x, grid_z);
  if (value == CellValue::Tree || value == CellValue::Boulder ||
      value == CellValue::IronOre || value == CellValue::Forest) {
    return false;
  }
  return padding_covers_cell(pathfinder, grid_x, grid_z);
}

template <typename Report>
void sweep_body_circle(const Pathfinding& pathfinder,
                       const QVector3D& position,
                       const BodyProfile& profile,
                       float radius,
                       Report&& report) {
  Point const grid = pathfinder.world_to_grid(position.x(), position.z());
  float const half_cell = pathfinder.grid_cell_size() * 0.5F;
  float const center_u = position.x() - pathfinder.get_grid_offset_x();
  float const center_v = position.z() - pathfinder.get_grid_offset_z();

  int const min_x = static_cast<int>(std::floor(center_u - radius - half_cell));
  int const max_x = static_cast<int>(std::ceil(center_u + radius + half_cell));
  int const min_z = static_cast<int>(std::floor(center_v - radius - half_cell));
  int const max_z = static_cast<int>(std::ceil(center_v + radius + half_cell));

  for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
    for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
      if (cell_is_open(pathfinder, cell_x, cell_z, profile)) {
        continue;
      }
      if (cell_x == grid.x && cell_z == grid.y) {

        if (!report(radius + half_cell)) {
          return;
        }
        continue;
      }
      float const gap_x =
          std::max(0.0F, std::abs(center_u - static_cast<float>(cell_x)) - half_cell);
      float const gap_z =
          std::max(0.0F, std::abs(center_v - static_cast<float>(cell_z)) - half_cell);
      float const gap = std::hypot(gap_x, gap_z);
      if (gap >= radius) {
        continue;
      }
      if (!report(radius - gap)) {
        return;
      }
    }
  }
}

auto current_pathfinder() -> Pathfinding* {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
  }
  return pathfinder;
}

auto can_stand_on(Pathfinding* pathfinder,
                  const QVector3D& position,
                  const BodyProfile& profile) -> bool {
  if (pathfinder == nullptr) {

    return building_body_penetration(position.x(), position.z(), profile.radius) <=
           0.0F;
  }

  float const radius = profile.clearance();
  bool blocked = false;
  sweep_body_circle(*pathfinder, position, profile, radius, [&blocked](float) -> bool {
    blocked = true;
    return false;
  });
  if (blocked) {
    return false;
  }
  if (profile.stops_at_building_facade) {
    return building_body_penetration(position.x(), position.z(), profile.radius) <=
           0.0F;
  }
  return true;
}

} // namespace

void refresh() {
  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    pathfinder->update_navigation_grid();
  }
}

auto can_stand(const QVector3D& position, const BodyProfile& profile) -> bool {
  return can_stand_on(current_pathfinder(), position, profile);
}

auto penetration(const QVector3D& position, const BodyProfile& profile) -> float {
  auto* pathfinder = current_pathfinder();
  float deepest =
      profile.stops_at_building_facade
          ? building_body_penetration(position.x(), position.z(), profile.radius)
          : 0.0F;
  if (pathfinder == nullptr) {
    return deepest;
  }
  float const radius = profile.clearance();
  sweep_body_circle(
      *pathfinder, position, profile, radius, [&deepest](float depth) -> bool {
        deepest = std::max(deepest, depth);
        return true;
      });
  return deepest;
}

auto can_traverse(const QVector3D& from,
                  const QVector3D& to,
                  const BodyProfile& profile) -> bool {
  if (GateService::blocks_move(from, to)) {
    return false;
  }
  auto* pathfinder = current_pathfinder();
  if (pathfinder == nullptr) {
    return can_stand_on(nullptr, to, profile);
  }

  QVector3D const delta = to - from;
  float const length = std::hypot(delta.x(), delta.z());
  float const spacing = std::max(0.2F, pathfinder->grid_cell_size() * 0.35F);
  int const samples = std::max(1, static_cast<int>(std::ceil(length / spacing)));
  for (int sample = 0; sample <= samples; ++sample) {
    float const t = static_cast<float>(sample) / static_cast<float>(samples);
    if (!can_stand_on(pathfinder, from + (delta * t), profile)) {
      return false;
    }
  }
  return true;
}

auto nearest_standable(const QVector3D& position,
                       const BodyProfile& profile,
                       float max_search_radius,
                       const std::optional<QVector3D>& approach_from)
    -> std::optional<QVector3D> {
  auto* pathfinder = current_pathfinder();
  if (can_stand_on(pathfinder, position, profile)) {
    return position;
  }
  if (pathfinder == nullptr) {
    return std::nullopt;
  }

  Point const origin = pathfinder->world_to_grid(position.x(), position.z());
  int const max_rings =
      std::max(1,
               static_cast<int>(std::ceil(
                   max_search_radius / std::max(0.01F, pathfinder->grid_cell_size()))));

  for (int ring = 1; ring <= max_rings; ++ring) {
    std::optional<QVector3D> best;
    float best_score = std::numeric_limits<float>::max();
    for (int dz = -ring; dz <= ring; ++dz) {
      for (int dx = -ring; dx <= ring; ++dx) {
        if (std::abs(dx) != ring && std::abs(dz) != ring) {
          continue;
        }
        Point const cell{origin.x + dx, origin.y + dz};
        QVector3D const candidate = pathfinder->grid_to_world(cell);
        if (!can_stand_on(pathfinder, candidate, profile)) {
          continue;
        }
        float score = (candidate - position).lengthSquared();
        if (approach_from.has_value()) {

          score += (candidate - *approach_from).lengthSquared() * 0.25F;
        }
        if (score < best_score) {
          best_score = score;
          best = candidate;
        }
      }
    }
    if (best.has_value()) {
      return best;
    }
  }
  return std::nullopt;
}

auto standing_point_around(const QVector3D& target,
                           float preferred_bearing_radians,
                           float distance,
                           const BodyProfile& profile) -> std::optional<QVector3D> {
  auto const samples = static_cast<int>(k_bearing_samples);

  for (int step = 0; step <= samples / 2; ++step) {
    for (int sign : {1, -1}) {
      if (step == 0 && sign < 0) {
        continue;
      }
      float const bearing =
          preferred_bearing_radians +
          (static_cast<float>(sign * step) * k_two_pi / static_cast<float>(samples));
      for (float scale : {1.0F, 0.85F, 1.2F}) {
        QVector3D const candidate(target.x() + (std::cos(bearing) * distance * scale),
                                  target.y(),
                                  target.z() + (std::sin(bearing) * distance * scale));
        if (!can_stand(candidate, profile)) {
          continue;
        }
        if (!can_traverse(candidate, target, profile)) {
          continue;
        }
        return candidate;
      }
    }
  }
  return std::nullopt;
}

} // namespace Game::Systems::Walkability
