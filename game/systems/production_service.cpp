#include "production_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../systems/nation_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/troop_config.h"
#include "core/entity.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include <vector>

namespace Game::Systems {

static auto
findFirstSelectedBarracks(Engine::Core::World &world,
                          const std::vector<Engine::Core::EntityID> &selected,
                          int owner_id) -> Engine::Core::Entity * {
  for (auto id : selected) {
    if (auto *e = world.getEntity(id)) {
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if ((u == nullptr) || u->owner_id != owner_id) {
        continue;
      }
      if (u->spawn_type == Game::Units::SpawnType::Barracks) {
        return e;
      }
    }
  }
  return nullptr;
}

namespace {

auto resolve_nation_id(const Engine::Core::UnitComponent *unit,
                       int owner_id) -> std::string {
  if ((unit != nullptr) && !unit->nation_id.empty()) {
    return unit->nation_id;
  }

  auto &registry = NationRegistry::instance();
  if (const auto *nation = registry.getNationForPlayer(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

void apply_production_profile(Engine::Core::ProductionComponent *prod,
                              const std::string &nation_id,
                              Game::Units::TroopType unit_type) {
  if (prod == nullptr) {
    return;
  }
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);
  prod->buildTime = profile.production.build_time;
  prod->villagerCost = profile.individuals_per_unit;
}

} // namespace

auto ProductionService::startProductionForFirstSelectedBarracks(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int owner_id,
    Game::Units::TroopType unit_type) -> ProductionResult {
  auto *e = findFirstSelectedBarracks(world, selected, owner_id);
  if (e == nullptr) {
    return ProductionResult::NoBarracks;
  }
  auto *unit = e->getComponent<Engine::Core::UnitComponent>();
  const std::string nation_id = resolve_nation_id(unit, owner_id);
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);

  auto *p = e->getComponent<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = e->addComponent<Engine::Core::ProductionComponent>();
  }
  if (p == nullptr) {
    return ProductionResult::NoBarracks;
  }

  int const individuals_per_unit = profile.individuals_per_unit;

  if (p->producedCount + individuals_per_unit > p->maxUnits) {
    return ProductionResult::PerBarracksLimitReached;
  }

  int const current_troops =
      Engine::Core::World::countTroopsForPlayer(owner_id);
  int const max_troops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
  if (current_troops + individuals_per_unit > max_troops) {
    return ProductionResult::GlobalTroopLimitReached;
  }

  const int max_queue_size = 5;
  int total_in_queue = p->inProgress ? 1 : 0;
  total_in_queue += static_cast<int>(p->productionQueue.size());

  if (total_in_queue >= max_queue_size) {
    return ProductionResult::QueueFull;
  }

  if (p->inProgress) {
    p->productionQueue.push_back(unit_type);
  } else {
    p->product_type = unit_type;
    apply_production_profile(p, nation_id, unit_type);
    p->timeRemaining = p->buildTime;
    p->inProgress = true;
  }

  return ProductionResult::Success;
}

auto ProductionService::setRallyForFirstSelectedBarracks(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int owner_id, float x,
    float z) -> bool {
  auto *e = findFirstSelectedBarracks(world, selected, owner_id);
  if (e == nullptr) {
    return false;
  }
  auto *p = e->getComponent<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = e->addComponent<Engine::Core::ProductionComponent>();
  }
  if (p == nullptr) {
    return false;
  }
  p->rallyX = x;
  p->rallyZ = z;
  p->rallySet = true;
  return true;
}

auto ProductionService::getSelectedBarracksState(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int owner_id,
    ProductionState &outState) -> bool {
  auto *e = findFirstSelectedBarracks(world, selected, owner_id);
  if (e == nullptr) {
    outState = {};
    return false;
  }
  outState.has_barracks = true;
  if (auto *p = e->getComponent<Engine::Core::ProductionComponent>()) {
    outState.inProgress = p->inProgress;
    outState.product_type = p->product_type;
    outState.timeRemaining = p->timeRemaining;
    outState.buildTime = p->buildTime;
    outState.producedCount = p->producedCount;
    outState.maxUnits = p->maxUnits;
    outState.villagerCost = p->villagerCost;
    outState.queueSize = static_cast<int>(p->productionQueue.size());
    outState.productionQueue = p->productionQueue;
  }
  return true;
}

} // namespace Game::Systems
