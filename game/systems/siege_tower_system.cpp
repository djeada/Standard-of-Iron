#include "siege_tower_system.h"

#include <QVector3D>
#include <queue>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_core.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

using Engine::Core::EntityID;
using Engine::Core::SiegeTowerComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WallWalkerComponent;

constexpr float k_dock_reach = 4.6F;
constexpr float k_wall_link_reach = 2.7F;
constexpr float k_gate_link_reach = 4.6F;
constexpr float k_node_arrival = 0.35F;
constexpr float k_drop_off_distance = 5.0F;

struct WallNode {
  EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  bool gate{false};
};

auto is_live_wall(Engine::Core::World& world,
                  EntityID id,
                  const UnitComponent& unit) -> bool {
  return unit.health > 0 && !world.has<Engine::Core::PendingRemovalComponent>(id) &&
         Game::Units::is_wall_network_spawn(unit.spawn_type);
}

auto gather_walls(Engine::Core::World& world, int owner_id) -> std::vector<WallNode> {
  std::vector<WallNode> nodes;
  for (auto [id, unit, transform] :
       world.view<const UnitComponent, const TransformComponent>()) {
    if (unit.owner_id != owner_id || !is_live_wall(world, id, unit)) {
      continue;
    }
    nodes.push_back({id,
                     transform.position.x,
                     transform.position.z,
                     unit.spawn_type == Game::Units::SpawnType::WallGate});
  }
  return nodes;
}

auto linked(const WallNode& a, const WallNode& b) -> bool {
  float const reach = (a.gate || b.gate) ? k_gate_link_reach : k_wall_link_reach;
  return std::hypot(a.x - b.x, a.z - b.z) <= reach;
}

auto nearest_node(const std::vector<WallNode>& nodes, float x, float z) -> int {
  int best = -1;
  float best_d = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    float const d = std::hypot(nodes[i].x - x, nodes[i].z - z);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

auto next_hop(const std::vector<WallNode>& nodes, int from, int to) -> int {
  if (from == to || from < 0 || to < 0) {
    return to;
  }
  std::vector<int> previous(nodes.size(), -2);
  std::queue<int> frontier;
  frontier.push(to);
  previous[static_cast<std::size_t>(to)] = -1;
  while (!frontier.empty()) {
    int const at = frontier.front();
    frontier.pop();
    if (at == from) {
      break;
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      if (previous[i] == -2 && linked(nodes[static_cast<std::size_t>(at)], nodes[i])) {
        previous[i] = at;
        frontier.push(static_cast<int>(i));
      }
    }
  }
  int const hop = previous[static_cast<std::size_t>(from)];
  return hop >= 0 ? hop : from;
}

void unload_garrison(Engine::Core::World& world,
                     Engine::Core::Entity& tower_entity,
                     SiegeTowerComponent& tower,
                     const UnitComponent& tower_unit,
                     const WallNode& wall) {
  auto registry = Game::Map::MapTransformer::get_factory_registry();
  if (!registry) {
    return;
  }
  Game::Units::SpawnParams params;
  params.position = QVector3D(wall.x, 0.0F, wall.z);
  params.player_id = tower_unit.owner_id;
  params.nation_id = tower_unit.nation_id;
  params.spawn_type = Game::Units::SpawnType::Swordsman;
  params.ai_controlled =
      world.has<Engine::Core::AIControlledComponent>(tower_entity.get_id());
  params.is_initial_spawn = false;
  if (const auto* transform = tower_entity.get_component<TransformComponent>()) {
    params.rotation_y = transform->rotation.y;
  }
  auto unit = registry->create(params.spawn_type, world, params);
  if (!unit) {
    return;
  }
  if (auto* entity = world.get_entity(unit->id())) {
    auto* walker = entity->add_component<WallWalkerComponent>();
    walker->wall_id = wall.id;
  }
  tower.garrison_aboard = false;
}

} // namespace

void SiegeTowerSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto const& services = Game::Session::services_for(*world);

  std::vector<EntityID> towers;
  for (auto [id, tower] : world->view<SiegeTowerComponent>()) {
    (void)tower;
    towers.push_back(id);
  }

  for (EntityID const id : towers) {
    auto* entity = world->get_entity(id);
    auto* tower = world->try_get<SiegeTowerComponent>(id);
    auto* unit = world->try_get<UnitComponent>(id);
    auto* transform = world->try_get<TransformComponent>(id);
    if (entity == nullptr || tower == nullptr || unit == nullptr ||
        transform == nullptr || unit->health <= 0) {
      continue;
    }

    if (tower->state == SiegeTowerComponent::State::Docked) {
      tower->ramp = std::min(
          1.0F, tower->ramp + delta_time / SiegeTowerComponent::k_ramp_drop_seconds);
      continue;
    }

    WallNode best;
    float best_d = k_dock_reach;
    for (auto [wall_id, wall_unit, wall_transform] :
         world->view<const UnitComponent, const TransformComponent>()) {
      if (!is_live_wall(*world, wall_id, wall_unit) ||
          wall_unit.owner_id == unit->owner_id ||
          (services.owners != nullptr &&
           services.owners->are_allies(wall_unit.owner_id, unit->owner_id))) {
        continue;
      }
      float const d = std::hypot(wall_transform.position.x - transform->position.x,
                                 wall_transform.position.z - transform->position.z);
      if (d < best_d) {
        best_d = d;
        best = {wall_id,
                wall_transform.position.x,
                wall_transform.position.z,
                wall_unit.spawn_type == Game::Units::SpawnType::WallGate};
      }
    }
    if (best.id == 0) {
      continue;
    }

    tower->state = SiegeTowerComponent::State::Docked;
    tower->docked_wall_id = best.id;
    if (auto* movement = world->try_get<Engine::Core::MovementComponent>(id)) {
      movement->stop();
    }
    if (tower->garrison_aboard) {
      unload_garrison(*world, *entity, *tower, *unit, best);
    }
  }
}

void WallWalkSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  std::vector<EntityID> walkers;
  for (auto [id, walker] : world->view<WallWalkerComponent>()) {
    (void)walker;
    walkers.push_back(id);
  }

  for (EntityID const id : walkers) {
    auto* walker = world->try_get<WallWalkerComponent>(id);
    auto* unit = world->try_get<UnitComponent>(id);
    auto* transform = world->try_get<TransformComponent>(id);
    auto* movement = world->try_get<Engine::Core::MovementComponent>(id);
    if (walker == nullptr || unit == nullptr || transform == nullptr) {
      continue;
    }
    if (unit->health <= 0 || world->has<Engine::Core::PendingRemovalComponent>(id)) {
      world->remove<WallWalkerComponent>(id);
      continue;
    }

    auto const* anchor_unit = world->try_get<UnitComponent>(walker->wall_id);
    bool const anchor_live =
        anchor_unit != nullptr && anchor_unit->health > 0 &&
        !world->has<Engine::Core::PendingRemovalComponent>(walker->wall_id);
    if (!anchor_live) {
      world->remove<WallWalkerComponent>(id);
      continue;
    }
    auto const run = gather_walls(*world, anchor_unit->owner_id);

    int const here = nearest_node(run, transform->position.x, transform->position.z);
    if (here < 0 ||
        std::hypot(run[static_cast<std::size_t>(here)].x - transform->position.x,
                   run[static_cast<std::size_t>(here)].z - transform->position.z) >
            3.0F) {
      world->remove<WallWalkerComponent>(id);
      continue;
    }
    walker->wall_id = run[static_cast<std::size_t>(here)].id;

    if (movement == nullptr || !movement->get_has_target()) {
      continue;
    }

    float const goal_x = movement->get_has_requested_goal()
                             ? movement->get_requested_goal_x()
                             : movement->get_goal_x();
    float const goal_z = movement->get_has_requested_goal()
                             ? movement->get_requested_goal_z()
                             : movement->get_goal_y();
    int const goal_node = nearest_node(run, goal_x, goal_z);
    bool const leaves_wall =
        std::hypot(run[static_cast<std::size_t>(goal_node)].x - goal_x,
                   run[static_cast<std::size_t>(goal_node)].z - goal_z) >
        k_drop_off_distance;
    int const hop = next_hop(run, here, goal_node);
    auto const& target = run[static_cast<std::size_t>(hop)];

    float const dx = target.x - transform->position.x;
    float const dz = target.z - transform->position.z;
    float const dist = std::hypot(dx, dz);
    if (hop == here && dist <= k_node_arrival) {
      if (leaves_wall) {
        world->remove<WallWalkerComponent>(id);
      } else {
        movement->stop();
      }
      continue;
    }
    float const step = std::min(dist, std::max(unit->speed, 0.5F) * delta_time);
    if (dist > 0.0001F) {
      transform->position.x += dx / dist * step;
      transform->position.z += dz / dist * step;
      transform->rotation.y = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
    }
  }
}

} // namespace Game::Systems
