#include "app/commander/commander_motor.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "app/commander/commander_wall_deck.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/entity.h"
#include "game/core/simulation_timing.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/movement/body_profile.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/navigation/walkability.h"

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

  constexpr std::array<float, 4> k_deflections{0.52F, -0.52F, 1.05F, -1.05F};
  for (float const angle : k_deflections) {
    float const cosine = std::cos(angle);
    float const sine = std::sin(angle);
    float const deflected_x = ((delta_x * cosine) - (delta_z * sine)) * cosine;
    float const deflected_z = ((delta_x * sine) + (delta_z * cosine)) * cosine;
    if (traversable_step(
            profile, from_x, from_z, from_x + deflected_x, from_z + deflected_z)) {
      return {.x = from_x + deflected_x, .z = from_z + deflected_z, .moved = true};
    }
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
  auto const profile = Game::Systems::motor_profile_for(commander);
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
  return walkable_point(Game::Systems::motor_profile_for(commander), x, z);
}

auto CommanderMotor::is_on_wall(const Engine::Core::Entity& commander) -> bool {
  auto const* registry = commander.registry();
  auto const* walker =
      registry == nullptr
          ? nullptr
          : registry->try_get<Engine::Core::WallWalkerComponent>(commander.get_id());
  return walker != nullptr && walker->player_driven;
}

auto CommanderMotor::wall_step(const Engine::Core::Entity& commander,
                               Engine::Core::TransformComponent& transform,
                               const CommanderMotorRequest& request)
    -> std::optional<CommanderMotorResult> {
  constexpr float k_wall_reach = 14.0F;
  auto const commander_id = commander.get_id();
  auto const* unit = m_world->try_get<Engine::Core::UnitComponent>(commander_id);
  if (m_world->get_entity(commander_id) == nullptr || unit == nullptr) {
    return std::nullopt;
  }
  auto* walker = m_world->try_get<Engine::Core::WallWalkerComponent>(commander_id);
  if (walker != nullptr && !walker->player_driven) {
    if (!walker->aloft()) {
      m_world->remove<Engine::Core::WallWalkerComponent>(commander_id);
      walker = nullptr;
    } else {
      walker->player_driven = true;
      walker->phase = Engine::Core::WallWalkerComponent::Phase::OnDeck;
      walker->path.clear();
      walker->exit_chain.clear();
      walker->has_goal = false;
    }
  }
  bool const aloft = walker != nullptr;
  auto const nodes = WallDeck::gather(
      *m_world, unit->owner_id, request.from.x(), request.from.z(), k_wall_reach);
  if (nodes.empty()) {
    if (aloft) {
      m_world->remove<Engine::Core::WallWalkerComponent>(commander_id);
    }
    return std::nullopt;
  }

  auto settle = [&](float x, float z, const WallDeck::Surface& surface) {
    CommanderMotorResult result;
    result.source = request.source;
    result.moved = true;
    result.slid = std::abs(x - request.to.x()) > 1.0e-5F ||
                  std::abs(z - request.to.z()) > 1.0e-5F;
    float const inverse_dt = request.dt > 0.0F ? 1.0F / request.dt : 0.0F;
    result.velocity = QVector3D(
        (x - request.from.x()) * inverse_dt, 0.0F, (z - request.from.z()) * inverse_dt);
    transform.position.x = x;
    transform.position.z = z;
    walker->elevation = surface.elevation;
    walker->wall_id = surface.node_id;
    result.position = QVector3D(x, transform.position.y, z);
    return result;
  };

  auto const target = WallDeck::surface_at(nodes, request.to.x(), request.to.z());
  if (!aloft) {
    auto const here = WallDeck::surface_at(nodes, request.from.x(), request.from.z());
    bool const boarding = target.has_value() && target->on_access &&
                          target->elevation <= WallDeck::k_board_elevation &&
                          (!here.has_value() || target->elevation >= here->elevation);
    if (!boarding || request.airborne) {
      return std::nullopt;
    }
    walker = m_world->emplace<Engine::Core::WallWalkerComponent>(commander_id);
    if (walker == nullptr) {
      return std::nullopt;
    }
    walker->player_driven = true;
    walker->phase = Engine::Core::WallWalkerComponent::Phase::OnDeck;
    return settle(request.to.x(), request.to.z(), *target);
  }

  if (target.has_value()) {
    return settle(request.to.x(), request.to.z(), *target);
  }

  auto const here = WallDeck::surface_at(nodes, request.from.x(), request.from.z());
  if (here.has_value() && here->on_access &&
      here->elevation <= WallDeck::k_board_elevation) {
    m_world->remove<Engine::Core::WallWalkerComponent>(commander_id);
    return std::nullopt;
  }

  bool const x_first = std::abs(request.to.x() - request.from.x()) >=
                       std::abs(request.to.z() - request.from.z());
  for (int pass = 0; pass < 2; ++pass) {
    bool const along_x = (pass == 0) == x_first;
    float const x = along_x ? request.to.x() : request.from.x();
    float const z = along_x ? request.from.z() : request.to.z();
    if (auto const slide = WallDeck::surface_at(nodes, x, z); slide.has_value()) {
      return settle(x, z, *slide);
    }
  }

  if (!here.has_value()) {
    if (auto const deck =
            WallDeck::nearest_deck_point(nodes, request.from.x(), request.from.z());
        deck.has_value()) {
      return settle(deck->x, deck->z, deck->surface);
    }
  }

  CommanderMotorResult blocked;
  blocked.source = request.source;
  blocked.blocked = true;
  blocked.position = request.from;
  return blocked;
}

auto CommanderMotor::advance(const Engine::Core::Entity& commander,
                             Engine::Core::TransformComponent& transform,
                             const CommanderMotorRequest& request)
    -> CommanderMotorResult {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_motor());
  if (m_world != nullptr) {
    if (auto const on_wall = wall_step(commander, transform, request);
        on_wall.has_value()) {
      m_last = *on_wall;
      return *on_wall;
    }
  }
  auto const profile = Game::Systems::motor_profile_for(commander);
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
