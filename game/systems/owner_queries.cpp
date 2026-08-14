#include "owner_queries.h"

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "owner_registry.h"
#include "troop_count_registry.h"

namespace Game::Systems {

namespace {

template <typename Predicate>
auto collect_units(const Engine::Core::World& world,
                   Predicate&& predicate) -> std::vector<Engine::Core::Entity*> {
  std::vector<Engine::Core::Entity*> result;
  result.reserve(world.entity_count());
  world.for_each_entity([&](Engine::Core::Entity& entity) {
    auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && predicate(*unit)) {
      result.push_back(&entity);
    }
  });
  return result;
}

} // namespace

auto allied_units(const Engine::Core::World& world,
                  int owner_id) -> std::vector<Engine::Core::Entity*> {
  auto& owners = OwnerRegistry::instance();
  return collect_units(
      world, [owner_id, &owners](const Engine::Core::UnitComponent& unit) {
        return unit.owner_id == owner_id || owners.are_allies(owner_id, unit.owner_id);
      });
}

auto enemy_units(const Engine::Core::World& world,
                 int owner_id) -> std::vector<Engine::Core::Entity*> {
  auto& owners = OwnerRegistry::instance();
  return collect_units(world,
                       [owner_id, &owners](const Engine::Core::UnitComponent& unit) {
                         return owners.are_enemies(owner_id, unit.owner_id);
                       });
}

auto troop_count_for(int owner_id) -> int {
  return TroopCountRegistry::instance().get_troop_count(owner_id);
}

} // namespace Game::Systems
