#include "ai_snapshot_builder.h"
#include "../../core/component.h"
#include "../../core/world.h"

namespace Game::Systems::AI {

AISnapshot AISnapshotBuilder::build(const Engine::Core::World &world,
                                    int aiOwnerId) const {
  AISnapshot snapshot;
  snapshot.playerId = aiOwnerId;

  auto friendlies = world.getUnitsOwnedBy(aiOwnerId);
  snapshot.friendlies.reserve(friendlies.size());

  int skippedNoAI = 0;
  int skippedNoUnit = 0;
  int skippedDead = 0;
  int added = 0;

  for (auto *entity : friendlies) {
    if (!entity->hasComponent<Engine::Core::AIControlledComponent>()) {
      skippedNoAI++;
      continue;
    }

    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit) {
      skippedNoUnit++;
      continue;
    }

    if (unit->health <= 0) {
      skippedDead++;
      continue;
    }

    EntitySnapshot data;
    data.id = entity->getId();
    data.unitType = unit->unitType;
    data.ownerId = unit->ownerId;
    data.health = unit->health;
    data.maxHealth = unit->maxHealth;
    data.isBuilding = entity->hasComponent<Engine::Core::BuildingComponent>();

    if (auto *transform =
            entity->getComponent<Engine::Core::TransformComponent>()) {
      data.posX = transform->position.x;
      data.posY = 0.0f;
      data.posZ = transform->position.z;
    }

    if (auto *movement =
            entity->getComponent<Engine::Core::MovementComponent>()) {
      data.movement.hasComponent = true;
      data.movement.hasTarget = movement->hasTarget;
    }

    if (auto *production =
            entity->getComponent<Engine::Core::ProductionComponent>()) {
      data.production.hasComponent = true;
      data.production.inProgress = production->inProgress;
      data.production.buildTime = production->buildTime;
      data.production.timeRemaining = production->timeRemaining;
      data.production.producedCount = production->producedCount;
      data.production.maxUnits = production->maxUnits;
      data.production.productType = production->productType;
      data.production.rallySet = production->rallySet;
      data.production.rallyX = production->rallyX;
      data.production.rallyZ = production->rallyZ;
    }

    snapshot.friendlies.push_back(std::move(data));
    added++;
  }

  auto enemies = world.getEnemyUnits(aiOwnerId);
  snapshot.visibleEnemies.reserve(enemies.size());

  for (auto *entity : enemies) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform)
      continue;

    ContactSnapshot contact;
    contact.id = entity->getId();
    contact.isBuilding =
        entity->hasComponent<Engine::Core::BuildingComponent>();
    contact.posX = transform->position.x;
    contact.posY = 0.0f;
    contact.posZ = transform->position.z;

    contact.health = unit->health;
    contact.maxHealth = unit->maxHealth;
    contact.unitType = unit->unitType;

    snapshot.visibleEnemies.push_back(std::move(contact));
  }

  return snapshot;
}

} // namespace Game::Systems::AI
