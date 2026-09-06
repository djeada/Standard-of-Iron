#include "app/commander/commander_motor.h"

#include <algorithm>
#include <cmath>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/core/simulation_timing.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/body_profile.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/walkability.h"

namespace App::Core {

namespace {

auto walkable_point(const Game::Systems::BodyProfile& profile,
                    float x,
                    float z) -> bool {
  return Game::Systems::Walkability::can_stand(QVector3D(x, 0.0F, z), profile);
}

auto traversable_step(const Game::Systems::BodyProfile& profile,
                      float from_x,
                      float from_z,
                      float to_x,
                      float to_z) -> bool {
  return Game::Systems::Walkability::can_traverse(
      QVector3D(from_x, 0.0F, from_z), QVector3D(to_x, 0.0F, to_z), profile);
}

struct GroundMove {
  float x{0.0F};
  float z{0.0F};
  bool moved{false};
};

auto airborne_step(float to_x, float to_z) -> GroundMove {
  return {.x = to_x, .z = to_z, .moved = true};
}

auto resolve_ground_step(const Game::Systems::BodyProfile& profile,
                         float from_x,
                         float from_z,
                         float to_x,
                         float to_z) -> GroundMove {
  if (traversable_step(profile, from_x, from_z, to_x, to_z)) {
    return {.x = to_x, .z = to_z, .moved = true};
  }

  float const delta_x = to_x - from_x;
  float const delta_z = to_z - from_z;
  bool const slide_x_free = std::abs(delta_x) > 1.0e-5F &&
                            traversable_step(profile, from_x, from_z, to_x, from_z);
  bool const slide_z_free = std::abs(delta_z) > 1.0e-5F &&
                            traversable_step(profile, from_x, from_z, from_x, to_z);

  if (slide_x_free && (!slide_z_free || std::abs(delta_x) >= std::abs(delta_z))) {
    return {.x = to_x, .z = from_z, .moved = true};
  }
  if (slide_z_free) {
    return {.x = from_x, .z = to_z, .moved = true};
  }

  QVector3D const here(from_x, 0.0F, from_z);
  if (Game::Systems::Walkability::penetration(here, profile) <= 0.0F) {
    return {.x = from_x, .z = from_z, .moved = false};
  }
  constexpr float k_escape_search_radius = 6.0F;
  auto const escape = Game::Systems::Walkability::nearest_standable(
      here, profile, k_escape_search_radius);
  if (!escape.has_value()) {
    return {.x = from_x, .z = from_z, .moved = false};
  }
  float const escape_dx = escape->x() - from_x;
  float const escape_dz = escape->z() - from_z;
  float const escape_distance = std::hypot(escape_dx, escape_dz);
  if (escape_distance <= 1.0e-5F) {
    return {.x = from_x, .z = from_z, .moved = false};
  }

  float const budget = std::max(std::hypot(delta_x, delta_z), 0.02F);
  float const travel = std::min(budget, escape_distance);
  return {.x = from_x + (escape_dx / escape_distance * travel),
          .z = from_z + (escape_dz / escape_distance * travel),
          .moved = true};
}

} // namespace

auto CommanderMotor::reachable_ground_position(Game::Session::SessionContext& session,
                                               const Engine::Core::Entity& commander,
                                               const QVector3D& start,
                                               const QVector3D& desired,
                                               unsigned int ignore_entity_id)
    -> QVector3D {
  auto const profile = Game::Systems::body_profile_for(commander);
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
    if (!walkable_point(profile, sample.x(), sample.z())) {
      break;
    }
    best = sample;
  }
  return best;
}

auto CommanderMotor::body_radius() -> float {
  return Game::Systems::k_person_body_radius;
}

auto CommanderMotor::is_walkable_at(const Engine::Core::Entity& commander,
                                    float x,
                                    float z) -> bool {
  return walkable_point(Game::Systems::body_profile_for(commander), x, z);
}

auto CommanderMotor::advance(const Engine::Core::Entity& commander,
                             Engine::Core::TransformComponent& transform,
                             const CommanderMotorRequest& request)
    -> CommanderMotorResult {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_motor());
  auto const profile = Game::Systems::body_profile_for(commander);
  GroundMove const step = request.airborne
                              ? airborne_step(request.to.x(), request.to.z())
                              : resolve_ground_step(profile,
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
