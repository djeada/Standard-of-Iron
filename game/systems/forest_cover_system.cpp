#include "forest_cover_system.h"

#include <array>
#include <cstdint>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_combat.h"
#include "../core/component_core.h"
#include "../core/component_economy.h"
#include "../core/component_gameplay.h"
#include "../core/component_structures.h"
#include "../core/world.h"
#include "nav_grid.h"
#include "order_service.h"
#include "owner_registry.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

using Engine::Core::ForestCoverComponent;

class FellowMasks {
public:
  explicit FellowMasks(const OwnerRegistry* owners)
      : m_owners(owners) {}

  auto of(int owner_id) -> std::uint64_t {
    const std::uint64_t own = ForestCoverComponent::owner_bit(owner_id);
    if (own == 0U) {
      return 0U;
    }
    const auto slot = static_cast<std::size_t>(owner_id);
    if (!m_known[slot]) {
      std::uint64_t mask = own;
      if (m_owners != nullptr) {
        for (const int ally : m_owners->get_allies_of(owner_id)) {
          mask |= ForestCoverComponent::owner_bit(ally);
        }
      }
      m_masks[slot] = mask;
      m_known[slot] = true;
    }
    return m_masks[slot];
  }

private:
  const OwnerRegistry* m_owners;
  std::array<std::uint64_t, 64> m_masks{};
  std::array<bool, 64> m_known{};
};

auto may_take_cover(const Engine::Core::Entity& entity,
                    const Engine::Core::UnitComponent& unit) -> bool {
  return unit.health > 0 && !entity.has_component<Engine::Core::BuildingComponent>() &&
         !entity.has_component<Engine::Core::WildlifeComponent>() &&
         !entity.has_component<Engine::Core::PendingRemovalComponent>();
}

auto stands_in_forest(const Engine::Core::TransformComponent& transform,
                      const Pathfinding& pathfinder) -> bool {
  const Point cell = NavGrid::world_to_grid(transform.position.x, transform.position.z);
  return pathfinder.is_forest(cell.x, cell.y);
}

auto gives_itself_away(const Engine::Core::Entity& entity) -> bool {
  const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  return attack != nullptr && (attack->in_melee_lock ||
                               attack->time_since_last < k_forest_reveal_after_strike);
}

} // namespace

void ForestCoverSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }
  const auto* pathfinder = NavGrid::get_pathfinder();
  const auto units = world->collect_entities_with<Engine::Core::UnitComponent>();

  std::vector<Engine::Core::Entity*> covered;
  std::vector<Engine::Core::Entity*> sheltered;
  for (auto* entity : units) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* cover = entity->get_component<ForestCoverComponent>();
    const bool in_forest = pathfinder != nullptr && unit != nullptr &&
                           transform != nullptr && may_take_cover(*entity, *unit) &&
                           stands_in_forest(*transform, *pathfinder);
    if (!in_forest) {
      if (cover != nullptr) {
        cover->in_forest = false;
        cover->concealed = false;
        cover->seen_by = 0U;
      }
      continue;
    }
    if (cover == nullptr) {
      sheltered.push_back(entity);
    }
    if (gives_itself_away(*entity)) {
      if (cover != nullptr) {
        cover->in_forest = true;
        cover->concealed = false;
        cover->seen_by = 0U;
      }
      continue;
    }
    covered.push_back(entity);
  }
  for (auto* entity : sheltered) {
    auto* cover = entity->add_component<ForestCoverComponent>();
    cover->in_forest = true;
  }
  if (covered.empty()) {
    return;
  }
  spot(*world, covered);
  lose_track_of_the_hidden(*world);
}

void ForestCoverSystem::spot(Engine::Core::World& world,
                             const std::vector<Engine::Core::Entity*>& covered) {

  FellowMasks fellows(Game::Session::services_for(world).owners);
  for (auto* entity : covered) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto& position =
        entity->get_component<Engine::Core::TransformComponent>()->position;
    std::uint64_t seen_by = fellows.of(unit->owner_id);
    world.spatial_index().for_each_in_radius(
        position.x,
        position.z,
        k_forest_spot_distance,
        [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
          if (entry.id == entity->get_id() ||
              !entry.is(Engine::Core::WorldSpatialIndex::k_alive) ||
              entry.is(Engine::Core::WorldSpatialIndex::k_pending_removal)) {
            return;
          }
          const auto* watcher = world.get_entity(entry.id);
          const auto* watcher_unit =
              watcher != nullptr ? watcher->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
          const auto* watcher_transform =
              watcher != nullptr
                  ? watcher->get_component<Engine::Core::TransformComponent>()
                  : nullptr;
          if (watcher_unit == nullptr || watcher_transform == nullptr ||
              watcher_unit->health <= 0 ||
              watcher->has_component<Engine::Core::WildlifeComponent>()) {
            return;
          }
          const float dx = watcher_transform->position.x - position.x;
          const float dz = watcher_transform->position.z - position.z;
          if ((dx * dx) + (dz * dz) > k_forest_spot_distance * k_forest_spot_distance) {
            return;
          }
          seen_by |= fellows.of(watcher_unit->owner_id);
        });

    auto* cover = entity->get_component<ForestCoverComponent>();
    cover->in_forest = true;
    cover->concealed = true;
    cover->seen_by = seen_by;
  }
}

void ForestCoverSystem::lose_track_of_the_hidden(Engine::Core::World& world) {
  std::vector<Engine::Core::Entity*> lost;
  for (auto [id, target] : world.view<Engine::Core::AttackTargetComponent>()) {
    if (target.is_player_command || target.target_id == 0) {
      continue;
    }
    const auto* prey = world.get_entity(target.target_id);
    const auto* cover =
        prey != nullptr ? prey->get_component<ForestCoverComponent>() : nullptr;
    auto* hunter = world.get_entity(id);
    const auto* hunter_unit = hunter != nullptr
                                  ? hunter->get_component<Engine::Core::UnitComponent>()
                                  : nullptr;
    if (cover != nullptr && hunter_unit != nullptr &&
        cover->hidden_from(hunter_unit->owner_id)) {
      lost.push_back(hunter);
    }
  }
  for (auto* hunter : lost) {
    OrderService::clear_attack_target(hunter);
  }
}

} // namespace Game::Systems
