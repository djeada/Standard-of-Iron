#include "ai_command_applier.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../units/troop_config.h"
#include "../command_service.h"
#include "ai_utils.h"

#include <QVector3D>

namespace Game::Systems::AI {

void AICommandApplier::apply(Engine::Core::World &world, int aiOwnerId,
                             const std::vector<AICommand> &commands) {

  for (const auto &command : commands) {
    switch (command.type) {

    case AICommandType::MoveUnits: {
      if (command.units.empty()) {
        break;
      }

      std::vector<float> expandedX, expandedY, expandedZ;

      if (command.moveTargetX.size() != command.units.size()) {
        replicateLastTargetIfNeeded(command.moveTargetX, command.moveTargetY,
                                    command.moveTargetZ, command.units.size(),
                                    expandedX, expandedY, expandedZ);
      } else {
        expandedX = command.moveTargetX;
        expandedY = command.moveTargetY;
        expandedZ = command.moveTargetZ;
      }

      if (expandedX.empty()) {
        break;
      }

      std::vector<Engine::Core::EntityID> ownedUnits;
      std::vector<QVector3D> ownedTargets;
      ownedUnits.reserve(command.units.size());
      ownedTargets.reserve(command.units.size());

      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        auto entityId = command.units[idx];
        auto *entity = world.getEntity(entityId);
        if (!entity) {
          continue;
        }

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId) {
          continue;
        }

        ownedUnits.push_back(entityId);
        ownedTargets.emplace_back(expandedX[idx], expandedY[idx],
                                  expandedZ[idx]);
      }

      if (ownedUnits.empty()) {
        break;
      }

      CommandService::MoveOptions opts;
      opts.allowDirectFallback = true;
      opts.clearAttackIntent = false;
      opts.groupMove = ownedUnits.size() > 1;
      CommandService::moveUnits(world, ownedUnits, ownedTargets, opts);
      break;
    }

    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.targetId == 0) {
        break;
      }

      std::vector<Engine::Core::EntityID> ownedUnits;
      ownedUnits.reserve(command.units.size());

      for (auto entityId : command.units) {
        auto *entity = world.getEntity(entityId);
        if (!entity) {
          continue;
        }

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId) {
          continue;
        }

        ownedUnits.push_back(entityId);
      }

      if (ownedUnits.empty()) {
        break;
      }

      CommandService::attackTarget(world, ownedUnits, command.targetId,
                                   command.shouldChase);
      break;
    }

    case AICommandType::StartProduction: {
      auto *entity = world.getEntity(command.buildingId);
      if (!entity) {
        break;
      }

      auto *production =
          entity->getComponent<Engine::Core::ProductionComponent>();
      if (!production) {
        break;
      }

      if (production->inProgress) {
        break;
      }

      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && unit->ownerId != aiOwnerId) {
        break;
      }

      int currentTroops = world.countTroopsForPlayer(aiOwnerId);
      int maxTroops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
      Game::Units::TroopType productType = production->productType;
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              productType);
      if (currentTroops + individualsPerUnit > maxTroops) {
        break;
      }

      production->productType = command.productType;

      production->timeRemaining = production->buildTime;
      production->inProgress = true;

      break;
    }
    }
  }
}

} // namespace Game::Systems::AI
