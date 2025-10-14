#include "production_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"

namespace Game {
namespace Systems {

static Engine::Core::Entity *
findFirstSelectedBarracks(Engine::Core::World &world,
                          const std::vector<Engine::Core::EntityID> &selected,
                          int ownerId) {
  for (auto id : selected) {
    if (auto *e = world.getEntity(id)) {
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if (!u || u->ownerId != ownerId)
        continue;
      if (u->unitType == "barracks")
        return e;
    }
  }
  return nullptr;
}

bool ProductionService::startProductionForFirstSelectedBarracks(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int ownerId,
    const std::string &unitType) {
  auto *e = findFirstSelectedBarracks(world, selected, ownerId);
  if (!e)
    return false;
  auto *p = e->getComponent<Engine::Core::ProductionComponent>();
  if (!p)
    p = e->addComponent<Engine::Core::ProductionComponent>();
  if (!p)
    return false;
  if (p->producedCount >= p->maxUnits)
    return false;
  if (p->inProgress)
    return false;
  
  int currentTroops = world.countTroopsForPlayer(ownerId);
  int maxTroops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
  if (currentTroops >= maxTroops)
    return false;
  
  p->productType = unitType;
  p->timeRemaining = p->buildTime;
  p->inProgress = true;
  return true;
}

bool ProductionService::setRallyForFirstSelectedBarracks(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int ownerId, float x,
    float z) {
  auto *e = findFirstSelectedBarracks(world, selected, ownerId);
  if (!e)
    return false;
  auto *p = e->getComponent<Engine::Core::ProductionComponent>();
  if (!p)
    p = e->addComponent<Engine::Core::ProductionComponent>();
  if (!p)
    return false;
  p->rallyX = x;
  p->rallyZ = z;
  p->rallySet = true;
  return true;
}

bool ProductionService::getSelectedBarracksState(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int ownerId,
    ProductionState &outState) {
  auto *e = findFirstSelectedBarracks(world, selected, ownerId);
  if (!e) {
    outState = {};
    return false;
  }
  outState.hasBarracks = true;
  if (auto *p = e->getComponent<Engine::Core::ProductionComponent>()) {
    outState.inProgress = p->inProgress;
    outState.timeRemaining = p->timeRemaining;
    outState.buildTime = p->buildTime;
    outState.producedCount = p->producedCount;
    outState.maxUnits = p->maxUnits;
    outState.villagerCost = p->villagerCost;
  }
  return true;
}

} // namespace Systems
} // namespace Game
