#include "ai_snapshot_builder.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "systems/ai_system/ai_types.h"
#include <utility>

namespace Game::Systems::AI {

auto AISnapshotBuilder::build(const Engine::Core::World &world,
                              int aiOwnerId) -> AISnapshot {
  AISnapshot snapshot;
  snapshot.player_id = aiOwnerId;

  auto friendlies = world.getUnitsOwnedBy(aiOwnerId);
  snapshot.friendlies.reserve(friendlies.size());

  int skipped_no_ai = 0;
  int skipped_no_unit = 0;
  int skipped_dead = 0;
  int added = 0;

  for (auto *entity : friendlies) {
    if (!entity->hasComponent<Engine::Core::AIControlledComponent>()) {
      skipped_no_ai++;
      continue;
    }

    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      skipped_no_unit++;
      continue;
    }

    if (unit->health <= 0) {
      skipped_dead++;
      continue;
    }

    EntitySnapshot data;
    data.id = entity->getId();
    data.spawn_type = unit->spawn_type;
    data.owner_id = unit->owner_id;
    data.health = unit->health;
    data.max_health = unit->max_health;
    data.isBuilding = entity->hasComponent<Engine::Core::BuildingComponent>();

    if (auto *transform =
            entity->getComponent<Engine::Core::TransformComponent>()) {
      data.posX = transform->position.x;
      data.posY = 0.0F;
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
      data.production.product_type = production->product_type;
      data.production.rallySet = production->rallySet;
      data.production.rallyX = production->rallyX;
      data.production.rallyZ = production->rallyZ;
      data.production.queueSize =
          static_cast<int>(production->productionQueue.size());
    }

    snapshot.friendlies.push_back(std::move(data));
    added++;
  }

  auto enemies = world.getEnemyUnits(aiOwnerId);
  snapshot.visibleEnemies.reserve(enemies.size());

  for (auto *entity : enemies) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    ContactSnapshot contact;
    contact.id = entity->getId();
    contact.isBuilding =
        entity->hasComponent<Engine::Core::BuildingComponent>();
    contact.posX = transform->position.x;
    contact.posY = 0.0F;
    contact.posZ = transform->position.z;

    contact.health = unit->health;
    contact.max_health = unit->max_health;
    contact.spawn_type = unit->spawn_type;

    snapshot.visibleEnemies.push_back(std::move(contact));
  }

  return snapshot;
}

} // namespace Game::Systems::AI
