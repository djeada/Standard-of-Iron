#include "app/commander/commander_motor.h"

#include <algorithm>
#include <cmath>

#include "game/core/component.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/session/session_context.h"

namespace App::Core {

namespace {

constexpr float k_commander_body_radius = 0.34F;

auto structure_blocks_commander_body(Game::Session::SessionContext& session,
                                     float x,
                                     float z) -> bool {
  auto const& registry = session.building_collision();
  auto blocked_by = [x, z](const Game::Systems::BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return false;
    }
    float const half_width = footprint.body_width * 0.5F;
    float const half_depth = footprint.body_depth * 0.5F;
    float const closest_x = std::clamp(
        x, footprint.body_center_x - half_width, footprint.body_center_x + half_width);
    float const closest_z = std::clamp(
        z, footprint.body_center_z - half_depth, footprint.body_center_z + half_depth);
    float const dx = x - closest_x;
    float const dz = z - closest_z;
    return ((dx * dx) + (dz * dz)) < k_commander_body_radius * k_commander_body_radius;
  };
  for (auto const& building : registry.get_all_buildings()) {
    if (blocked_by(building)) {
      return true;
    }
  }
  for (auto const& obstacle : registry.authored_obstacles()) {
    if (blocked_by(obstacle)) {
      return true;
    }
  }
  return false;
}

auto structure_padding_covers_cell(Game::Session::SessionContext& session,
                                   const Game::Systems::Pathfinding& pathfinder,
                                   int grid_x,
                                   int grid_z) -> bool {

  constexpr float k_cell_slack = 1.0F;
  float const cell_x = static_cast<float>(grid_x) + pathfinder.get_grid_offset_x();
  float const cell_z = static_cast<float>(grid_z) + pathfinder.get_grid_offset_z();
  auto const& registry = session.building_collision();
  auto covers = [cell_x, cell_z](const Game::Systems::BuildingFootprint& footprint) {
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
  for (auto const& building : registry.get_all_buildings()) {
    if (covers(building)) {
      return true;
    }
  }
  for (auto const& obstacle : registry.authored_obstacles()) {
    if (covers(obstacle)) {
      return true;
    }
  }
  return false;
}

constexpr float k_scatter_prop_block_radius = 0.52F;

auto scatter_prop_blocks_commander_body(const Game::Systems::Pathfinding& pathfinder,
                                        int grid_x,
                                        int grid_z,
                                        float x,
                                        float z) -> bool {
  using CellValue = Game::Systems::Pathfinding::CellValue;

  float const clearance = k_scatter_prop_block_radius + k_commander_body_radius;

  for (int offset_z = -1; offset_z <= 1; ++offset_z) {
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      int const cell_x = grid_x + offset_x;
      int const cell_z = grid_z + offset_z;
      auto const cell = pathfinder.cell_value(cell_x, cell_z);
      if (cell != CellValue::Tree && cell != CellValue::Boulder &&
          cell != CellValue::IronOre) {
        continue;
      }
      float const dx =
          x - (static_cast<float>(cell_x) + pathfinder.get_grid_offset_x());
      float const dz =
          z - (static_cast<float>(cell_z) + pathfinder.get_grid_offset_z());
      if (((dx * dx) + (dz * dz)) < clearance * clearance) {
        return true;
      }
    }
  }
  return false;
}

auto walkable_point(Game::Session::SessionContext& session, float x, float z) -> bool {
  using CellValue = Game::Systems::Pathfinding::CellValue;

  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    const auto grid = Game::Systems::NavGrid::world_to_grid(x, z);
    const auto cell = pathfinder->cell_value(grid.x, grid.y);

    if (cell != CellValue::Walkable) {
      if (!pathfinder->is_terrain_walkable(grid.x, grid.y)) {
        return false;
      }
      bool const scatter_prop_cell = cell == CellValue::Tree ||
                                     cell == CellValue::Boulder ||
                                     cell == CellValue::IronOre;
      if (!scatter_prop_cell &&
          !structure_padding_covers_cell(session, *pathfinder, grid.x, grid.y)) {
        return false;
      }
    }
    if (scatter_prop_blocks_commander_body(*pathfinder, grid.x, grid.y, x, z)) {
      return false;
    }
    return !structure_blocks_commander_body(session, x, z);
  }
  auto& terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized() &&
      !terrain.is_walkable(static_cast<int>(std::round(x)),
                           static_cast<int>(std::round(z)))) {
    return false;
  }
  return !structure_blocks_commander_body(session, x, z);
}

struct GroundMove {
  float x{0.0F};
  float z{0.0F};
  bool moved{false};
};

auto airborne_step(float to_x, float to_z) -> GroundMove {
  return {.x = to_x, .z = to_z, .moved = true};
}

auto resolve_ground_step(Game::Session::SessionContext& session,
                         float from_x,
                         float from_z,
                         float to_x,
                         float to_z) -> GroundMove {
  if (walkable_point(session, to_x, to_z)) {
    return {.x = to_x, .z = to_z, .moved = true};
  }

  float const delta_x = to_x - from_x;
  float const delta_z = to_z - from_z;
  bool const slide_x_free = std::abs(delta_x) > 1.0e-5F && walkable_point(session, to_x, from_z);
  bool const slide_z_free = std::abs(delta_z) > 1.0e-5F && walkable_point(session, from_x, to_z);

  if (slide_x_free && (!slide_z_free || std::abs(delta_x) >= std::abs(delta_z))) {
    return {.x = to_x, .z = from_z, .moved = true};
  }
  if (slide_z_free) {
    return {.x = from_x, .z = to_z, .moved = true};
  }
  return {.x = from_x, .z = from_z, .moved = false};
}

} // namespace

auto CommanderMotor::reachable_ground_position(Game::Session::SessionContext& session,
                                               const QVector3D& start,
                                               const QVector3D& desired,
                                               unsigned int ignore_entity_id)
    -> QVector3D {
  QVector3D candidate = desired;
  const float blocked_fraction = Game::Systems::first_building_intersection_fraction(
      session.building_collision(), start, desired, ignore_entity_id);
  if (blocked_fraction < 1.0F) {
    const float safe_fraction = std::clamp(blocked_fraction - 0.08F, 0.0F, 1.0F);
    candidate = start + (desired - start) * safe_fraction;
  }

  QVector3D best = start;
  constexpr int k_samples = 8;
  for (int sample_index = 1; sample_index <= k_samples; ++sample_index) {
    const float sample_t =
        static_cast<float>(sample_index) / static_cast<float>(k_samples);
    const QVector3D sample = start + (candidate - start) * sample_t;
    if (!walkable_point(session, sample.x(), sample.z())) {
      break;
    }
    best = sample;
  }
  return best;
}

auto CommanderMotor::body_radius() -> float {
  return k_commander_body_radius;
}

auto CommanderMotor::is_walkable_at(Game::Session::SessionContext& session,
                                    float x,
                                    float z) -> bool {
  return walkable_point(session, x, z);
}

auto CommanderMotor::advance(Game::Session::SessionContext& session,
                             Engine::Core::TransformComponent& transform,
                             const CommanderMotorRequest& request)
    -> CommanderMotorResult {
  GroundMove const step =
      request.airborne
          ? airborne_step(request.to.x(), request.to.z())
          : resolve_ground_step(session,
                                request.from.x(),
                                request.from.z(),
                                request.to.x(),
                                request.to.z());

  CommanderMotorResult result;
  result.source = request.source;
  result.moved = step.moved;
  result.blocked = !step.moved;
  result.slid = step.moved && (std::abs(step.x - request.to.x()) > 1.0e-5F ||
                               std::abs(step.z - request.to.z()) > 1.0e-5F);

  if (step.moved) {
    float const inverse_dt = request.dt > 0.0F ? 1.0F / request.dt : 0.0F;
    result.velocity = QVector3D((step.x - request.from.x()) * inverse_dt,
                                0.0F,
                                (step.z - request.from.z()) * inverse_dt);
    transform.position.x = step.x;
    transform.position.z = step.z;
    result.position = QVector3D(step.x, transform.position.y, step.z);
  } else {
    result.position = request.from;
  }

  m_last = result;
  return result;
}

auto CommanderMotor::teleport(Engine::Core::TransformComponent& transform,
                              const QVector3D& position,
                              CommanderDisplacementSource source)
    -> CommanderMotorResult {
  CommanderMotorResult result;
  result.source = source;
  result.moved = true;
  transform.position.x = position.x();
  transform.position.z = position.z();
  result.position = QVector3D(position.x(), transform.position.y, position.z());
  m_last = result;
  return result;
}

} // namespace App::Core
