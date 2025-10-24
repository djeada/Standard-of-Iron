#include "production_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../units/troop_config.h"

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
      if (u->spawnTypeEnum && 
          *u->spawnTypeEnum == Game::Units::SpawnType::Barracks)
        return e;
    }
  }
  return nullptr;
}

ProductionResult ProductionService::startProductionForFirstSelectedBarracks(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &selected, int ownerId,
    Game::Units::TroopType unitType) {
  auto *e = findFirstSelectedBarracks(world, selected, ownerId);
  if (!e)
    return ProductionResult::NoBarracks;
  auto *p = e->getComponent<Engine::Core::ProductionComponent>();
  if (!p)
    p = e->addComponent<Engine::Core::ProductionComponent>();
  if (!p)
    return ProductionResult::NoBarracks;

  int individualsPerUnit =
      Game::Units::TroopConfig::instance().getIndividualsPerUnit(unitType);

  if (p->producedCount + individualsPerUnit > p->maxUnits)
    return ProductionResult::PerBarracksLimitReached;

  int currentTroops = world.countTroopsForPlayer(ownerId);
  int maxTroops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
  if (currentTroops + individualsPerUnit > maxTroops)
    return ProductionResult::GlobalTroopLimitReached;

  const int maxQueueSize = 5;
  int totalInQueue = p->inProgress ? 1 : 0;
  totalInQueue += static_cast<int>(p->productionQueue.size());

  if (totalInQueue >= maxQueueSize)
    return ProductionResult::QueueFull;

  if (p->inProgress) {
    p->productionQueue.push_back(unitType);
  } else {
    p->productType = unitType;
    p->timeRemaining = p->buildTime;
    p->inProgress = true;
  }

  return ProductionResult::Success;
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
    outState.productType = p->productType;
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

} // namespace Systems
} // namespace Game
