#include "production_system.h"
#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/troop_config.h"
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

    auto *unitComp = e->getComponent<Engine::Core::UnitComponent>();
    if (unitComp && Game::Core::isNeutralOwner(unitComp->ownerId)) {
      continue;
    }

    if (!prod->inProgress)
      continue;

    int individualsPerUnit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            prod->productType);

    if (prod->producedCount + individualsPerUnit > prod->maxUnits) {
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
        if (currentTroops + individualsPerUnit > maxTroops) {
          prod->inProgress = false;
          prod->timeRemaining = 0.0f;
          continue;
        }

        float exitOffset = 2.5f + 0.2f * float(prod->producedCount % 5);
        float exitAngle = 0.5f * float(prod->producedCount % 8);
        QVector3D exitPos =
            QVector3D(t->position.x + exitOffset * std::cos(exitAngle), 0.0f,
                      t->position.z + exitOffset * std::sin(exitAngle));

        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
          Game::Units::SpawnParams sp;
          sp.position = exitPos;
          sp.playerId = u->ownerId;
          sp.unitType = prod->productType;
          sp.aiControlled =
              e->hasComponent<Engine::Core::AIControlledComponent>();
          auto unit = reg->create(prod->productType, *world, sp);

          if (unit && prod->rallySet) {
            unit->moveTo(prod->rallyX, prod->rallyZ);
          }
        }

        prod->producedCount += individualsPerUnit;
      }

      prod->inProgress = false;
      prod->timeRemaining = 0.0f;

      if (!prod->productionQueue.empty()) {
        prod->productType = prod->productionQueue.front();
        prod->productionQueue.erase(prod->productionQueue.begin());
        prod->timeRemaining = prod->buildTime;
        prod->inProgress = true;
      }
    }
  }
}

} // namespace Systems
} // namespace Game
