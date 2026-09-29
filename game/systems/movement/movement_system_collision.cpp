#include "movement_system_collision.h"

#include <algorithm>
#include <cmath>

#include "route_follow_system.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/navigation/walkability.h"

namespace Game::Systems::MovementCollision {

namespace {

constexpr float k_motor_substep_cells = 0.45F;
constexpr int k_max_motor_substeps = 8;

} // namespace

MotorCollision::MotorCollision(const Engine::Core::Entity& entity,
                               float origin_x,
                               float origin_z,
                               bool respect_body_radius,
                               bool escaping)
    : m_entity(&entity)
    , m_respect_body_radius(respect_body_radius)
    , m_escaping(escaping) {
  QVector3D const origin(origin_x, 0.0F, origin_z);
  if (auto const* pathfinder = NavGrid::get_pathfinder(); pathfinder != nullptr) {
    Point const cell = NavGrid::world_to_grid(origin_x, origin_z);
    m_origin_on_terrain = pathfinder->is_terrain_walkable(cell.x, cell.y);
  }
  m_valid_tile = allowed_here(origin);
  if (!m_valid_tile) {
    m_trapped_depth = Walkability::penetration(origin, body_profile(entity));
  }
}

auto MotorCollision::point_allowed(float wx, float wz) const -> bool {
  if (m_escaping) {
    return terrain_allows(wx, wz);
  }
  QVector3D const point(wx, 0.0F, wz);
  if (m_valid_tile) {
    return allowed_here(point);
  }
  if (m_trapped_depth > 0.0F) {
    return Walkability::penetration(point, body_profile(*m_entity)) < m_trapped_depth;
  }
  return allowed_here(point);
}

auto MotorCollision::step_allowed(float from_x,
                                  float from_z,
                                  float to_x,
                                  float to_z) const -> bool {
  if (!point_allowed(to_x, to_z)) {
    return false;
  }
  Point const from_cell = NavGrid::world_to_grid(from_x, from_z);
  Point const to_cell = NavGrid::world_to_grid(to_x, to_z);
  if (from_cell.x == to_cell.x || from_cell.y == to_cell.y) {
    return true;
  }
  QVector3D const across_x = NavGrid::grid_to_world({to_cell.x, from_cell.y});
  QVector3D const across_z = NavGrid::grid_to_world({from_cell.x, to_cell.y});
  return point_allowed(across_x.x(), across_x.z()) &&
         point_allowed(across_z.x(), across_z.z());
}

auto MotorCollision::terrain_allows(float wx, float wz) const -> bool {
  auto const* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr || !m_origin_on_terrain) {
    return true;
  }
  Point const cell = NavGrid::world_to_grid(wx, wz);
  return pathfinder->is_terrain_walkable(cell.x, cell.y);
}

auto MotorCollision::allowed_here(const QVector3D& point) const -> bool {
  if (m_respect_body_radius) {
    return Walkability::can_stand(point, body_profile(*m_entity));
  }
  return is_movement_point_allowed(point, *m_entity);
}

auto sweep_through(const MotorCollision& collision,
                   float origin_x,
                   float origin_z,
                   float delta_x,
                   float delta_z) -> SweepResult {
  auto const* pathfinder = NavGrid::get_pathfinder();
  float const cell_size = pathfinder != nullptr ? pathfinder->grid_cell_size() : 1.0F;
  float const substep_length = std::max(0.05F, cell_size * k_motor_substep_cells);
  auto const allowed = [&collision](float fx, float fz, float tx, float tz) {
    return collision.step_allowed(fx, fz, tx, tz);
  };
  SweepResult result;
  float const total = std::hypot(delta_x, delta_z);
  if (total <= 1.0e-6F) {
    return result;
  }

  int const substeps =
      std::clamp(static_cast<int>(std::ceil(total / std::max(0.02F, substep_length))),
                 1,
                 k_max_motor_substeps);

  float x = origin_x;
  float z = origin_z;
  float remaining_x = delta_x;
  float remaining_z = delta_z;

  for (int step = 0; step < substeps; ++step) {
    float const fraction = 1.0F / static_cast<float>(substeps - step);
    float const step_x = remaining_x * fraction;
    float const step_z = remaining_z * fraction;
    if (std::hypot(step_x, step_z) <= 1.0e-7F) {
      break;
    }

    if (allowed(x, z, x + step_x, z + step_z)) {
      x += step_x;
      z += step_z;
      remaining_x -= step_x;
      remaining_z -= step_z;
      continue;
    }

    bool const x_clear = allowed(x, z, x + step_x, z);
    bool const z_clear = allowed(x, z, x, z + step_z);

    float normal_x = 0.0F;
    float normal_z = 0.0F;
    if (x_clear && (!z_clear || std::abs(step_x) >= std::abs(step_z))) {
      x += step_x;
      normal_z = step_z > 0.0F ? -1.0F : 1.0F;
    } else if (z_clear) {
      z += step_z;
      normal_x = step_x > 0.0F ? -1.0F : 1.0F;
    } else {
      result.blocked = true;
      result.contact = true;
      float const length = std::hypot(remaining_x, remaining_z);
      if (length > 1.0e-6F) {
        result.normal_x = -remaining_x / length;
        result.normal_z = -remaining_z / length;
      }
      break;
    }

    result.contact = true;
    result.normal_x = normal_x;
    result.normal_z = normal_z;

    remaining_x -= step_x;
    remaining_z -= step_z;
    float const into = remaining_x * normal_x + remaining_z * normal_z;
    if (into < 0.0F) {
      remaining_x -= normal_x * into;
      remaining_z -= normal_z * into;
    }
  }

  result.accepted_dx = x - origin_x;
  result.accepted_dz = z - origin_z;
  result.rejected_dx = delta_x - result.accepted_dx;
  result.rejected_dz = delta_z - result.accepted_dz;
  result.accepted_fraction = std::clamp(
      std::hypot(result.accepted_dx, result.accepted_dz) / total, 0.0F, 1.0F);
  return result;
}

auto slide_body_to(const Engine::Core::Entity& entity,
                   Engine::Core::TransformComponent& transform,
                   float target_x,
                   float target_z,
                   bool respect_body_radius) -> SweepResult {
  MotorCollision const collision(
      entity, transform.position.x, transform.position.z, respect_body_radius);
  auto const sweep = sweep_through(collision,
                                   transform.position.x,
                                   transform.position.z,
                                   target_x - transform.position.x,
                                   target_z - transform.position.z);
  transform.position.x += sweep.accepted_dx;
  transform.position.z += sweep.accepted_dz;
  return sweep;
}

void unstick_body(const Engine::Core::Entity& entity,
                  Engine::Core::TransformComponent& transform,
                  float delta_time) {
  if (entity.has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  if (auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && commander->jump_active) {
    return;
  }

  QVector3D const here(transform.position.x, 0.0F, transform.position.z);
  if (is_movement_point_allowed(here, entity)) {
    return;
  }

  BodyProfile const profile = motor_profile_for(entity);
  if (Walkability::penetration(here, profile) <= 0.0F) {
    return;
  }

  constexpr float k_escape_search_radius = 16.0F;
  auto const escape =
      Walkability::nearest_standable(here, profile, k_escape_search_radius);
  if (!escape.has_value()) {
    return;
  }
  float const dx = escape->x() - here.x();
  float const dz = escape->z() - here.z();
  float const distance = std::hypot(dx, dz);
  if (distance <= 1.0e-4F) {
    return;
  }

  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  float const speed = unit != nullptr ? std::max(0.5F, unit->speed) : 1.5F;
  float const travel = std::min(distance, speed * std::max(delta_time, 0.0F));
  transform.position.x += dx / distance * travel;
  transform.position.z += dz / distance * travel;
}

} // namespace Game::Systems::MovementCollision
