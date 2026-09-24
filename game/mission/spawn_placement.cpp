#include "game/mission/spawn_placement.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/systems/command_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/walkability.h"

namespace Game::Mission {
namespace {

constexpr float k_gap = 0.3F;
constexpr int k_footprint_probes = 8;
constexpr float k_two_pi = 6.2831853F;

struct Occupied {
  QVector3D centre;
  float radius;
};

auto collect_occupied(Engine::Core::World& world,
                      Engine::Core::EntityID exclude) -> std::vector<Occupied> {
  std::vector<Occupied> occupied;
  world.each<Engine::Core::MovementComponent>([&](Engine::Core::EntityID other,
                                                  Engine::Core::MovementComponent&) {
    if (other == exclude || world.has<Engine::Core::BuildingComponent>(other)) {
      return;
    }
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(other);
    if (transform == nullptr) {
      return;
    }
    occupied.push_back({QVector3D(transform->position.x, 0.0F, transform->position.z),
                        Game::Systems::CommandService::get_unit_radius(world, other)});
  });
  return occupied;
}

} // namespace

auto find_free_ground_near(Engine::Core::World& world,
                           Engine::Core::EntityID placed,
                           const QVector3D& origin,
                           BuildingFootprints buildings) -> std::optional<QVector3D> {
  const float radius = Game::Systems::CommandService::get_unit_radius(world, placed);
  Game::Systems::BodyProfile ground;
  if (buildings == BuildingFootprints::Refuse) {

    ground.radius = radius;
    ground.stops_at_building_facade = true;
  }
  if (const auto* movement = world.try_get<Engine::Core::MovementComponent>(placed)) {
    ground.passability = movement->get_can_enter_forest()
                             ? Game::Systems::Pathfinding::Passability::Light
                             : Game::Systems::Pathfinding::Passability::Heavy;
  }

  const std::vector<Occupied> occupied = collect_occupied(world, placed);

  const auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  const bool authored_inside_a_zone =
      undead != nullptr && undead->would_wake_a_zone(origin.x(), origin.z(), 0.0F);
  const auto wakes_the_dead = [&](const QVector3D& centre) {
    return undead != nullptr && !authored_inside_a_zone &&
           undead->would_wake_a_zone(centre.x(), centre.z(), radius);
  };

  const auto fits = [&](const QVector3D& centre) {
    if (wakes_the_dead(centre) ||
        !Game::Systems::Walkability::can_stand(centre, ground)) {
      return false;
    }
    for (int probe = 0; probe < k_footprint_probes; ++probe) {
      const float angle =
          static_cast<float>(probe) * k_two_pi / static_cast<float>(k_footprint_probes);
      const QVector3D edge(centre.x() + (std::sin(angle) * radius * 0.6F),
                           0.0F,
                           centre.z() + (std::cos(angle) * radius * 0.6F));
      if (!Game::Systems::Walkability::can_stand(edge, ground)) {
        return false;
      }
    }
    return std::none_of(occupied.begin(), occupied.end(), [&](const Occupied& other) {
      return (other.centre - centre).length() < other.radius + radius + k_gap;
    });
  };

  const QVector3D centre(origin.x(), 0.0F, origin.z());
  const float step = std::max(0.5F, radius * 0.5F);
  for (int ring = 0; ring <= k_spawn_search_rings; ++ring) {
    const float distance = step * static_cast<float>(ring);
    const int samples =
        ring == 0
            ? 1
            : std::max(6, static_cast<int>(std::ceil(k_two_pi * distance / step)));
    for (int sample = 0; sample < samples; ++sample) {
      const float angle =
          static_cast<float>(sample) * k_two_pi / static_cast<float>(samples);
      const QVector3D candidate(centre.x() + (std::sin(angle) * distance),
                                0.0F,
                                centre.z() + (std::cos(angle) * distance));
      if (fits(candidate)) {
        return candidate;
      }
    }
  }
  return std::nullopt;
}

auto place_clear_of_units(Engine::Core::World& world,
                          Engine::Core::EntityID placed,
                          const QVector3D& origin,
                          BuildingFootprints buildings) -> std::optional<QVector3D> {
  auto* transform = world.try_get<Engine::Core::TransformComponent>(placed);
  if (transform == nullptr) {
    return std::nullopt;
  }
  const auto free_ground = find_free_ground_near(world, placed, origin, buildings);
  if (!free_ground.has_value()) {
    return std::nullopt;
  }
  transform->position.x = free_ground->x();
  transform->position.z = free_ground->z();
  return free_ground;
}

} // namespace Game::Mission
