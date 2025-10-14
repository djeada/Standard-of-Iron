#include "production_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include <cmath>

namespace Game {
namespace Systems {

void ProductionSystem::update(Engine::Core::World *world, float deltaTime) {
  if (!world)
    return;
  auto entities = world->getEntitiesWith<Engine::Core::ProductionComponent>();
  for (auto *e : entities) {
    auto *prod = e->getComponent<Engine::Core::ProductionComponent>();
    if (!prod)
      continue;
    if (!prod->inProgress)
      continue;
    if (prod->producedCount >= prod->maxUnits) {
      prod->inProgress = false;
      continue;
    }
    prod->timeRemaining -= deltaTime;
    if (prod->timeRemaining <= 0.0f) {

      auto *t = e->getComponent<Engine::Core::TransformComponent>();
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if (t && u) {

        int currentTroops = world->countTroopsForPlayer(u->ownerId);
        int maxTroops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
        if (currentTroops >= maxTroops) {
          prod->inProgress = false;
          prod->timeRemaining = 0.0f;
          continue;
        }

        QVector3D spawnPos;
        if (prod->rallySet) {
          spawnPos = QVector3D(prod->rallyX, 0.0f, prod->rallyZ);
        } else {
          float radius = 3.0f + 0.3f * float(prod->producedCount % 10);
          float angle = 0.6f * float(prod->producedCount);
          spawnPos = QVector3D(t->position.x + radius * std::cos(angle), 0.0f,
                               t->position.z + radius * std::sin(angle));
        }
        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
          Game::Units::SpawnParams sp;
          sp.position = spawnPos;
          sp.playerId = u->ownerId;
          sp.unitType = prod->productType;
          sp.aiControlled =
              e->hasComponent<Engine::Core::AIControlledComponent>();
          reg->create(prod->productType, *world, sp);
        }

        prod->producedCount += 1;
      }

      prod->inProgress = false;
      prod->timeRemaining = 0.0f;
    }
  }
}

} // namespace Systems
} // namespace Game
