#include "ai_command_applier.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../command_service.h"
#include "ai_utils.h"

#include <QDebug>
#include <QVector3D>

namespace Game::Systems::AI {

void AICommandApplier::apply(Engine::Core::World &world, int aiOwnerId,
                             const std::vector<AICommand> &commands) {

  static int applyCounter = 0;
  if (!commands.empty() && ++applyCounter % 5 == 0) {
    qDebug() << "[AICommandApplier] Applying" << commands.size()
             << "commands for AI" << aiOwnerId;
  }

  for (const auto &command : commands) {
    switch (command.type) {

    case AICommandType::MoveUnits: {
      if (command.units.empty())
        break;

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

      if (expandedX.empty())
        break;

      std::vector<Engine::Core::EntityID> ownedUnits;
      std::vector<QVector3D> ownedTargets;
      ownedUnits.reserve(command.units.size());
      ownedTargets.reserve(command.units.size());

      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        auto entityId = command.units[idx];
        auto *entity = world.getEntity(entityId);
        if (!entity)
          continue;

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId)
          continue;

        ownedUnits.push_back(entityId);
        ownedTargets.emplace_back(expandedX[idx], expandedY[idx],
                                  expandedZ[idx]);
      }

      if (ownedUnits.empty())
        break;

      CommandService::MoveOptions opts;
      opts.allowDirectFallback = true;
      opts.clearAttackIntent = false;
      opts.groupMove = ownedUnits.size() > 1;
      CommandService::moveUnits(world, ownedUnits, ownedTargets, opts);
      break;
    }

    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.targetId == 0)
        break;

      std::vector<Engine::Core::EntityID> ownedUnits;
      ownedUnits.reserve(command.units.size());

      for (auto entityId : command.units) {
        auto *entity = world.getEntity(entityId);
        if (!entity)
          continue;

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId)
          continue;

        ownedUnits.push_back(entityId);
      }

      if (ownedUnits.empty())
        break;

      CommandService::attackTarget(world, ownedUnits, command.targetId,
                                   command.shouldChase);
      break;
    }

    case AICommandType::StartProduction: {
      auto *entity = world.getEntity(command.buildingId);
      if (!entity) {
        qDebug() << "[AICommandApplier] StartProduction: building entity not "
                    "found (ID="
                 << command.buildingId << ")";
        break;
      }

      auto *production =
          entity->getComponent<Engine::Core::ProductionComponent>();
      if (!production) {
        qDebug() << "[AICommandApplier] StartProduction: no "
                    "ProductionComponent on building";
        break;
      }

      if (production->inProgress) {
        qDebug() << "[AICommandApplier] StartProduction: production already in "
                    "progress";
        break;
      }

      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && unit->ownerId != aiOwnerId) {
        qDebug() << "[AICommandApplier] StartProduction: ownership mismatch "
                    "(building owner="
                 << unit->ownerId << "AI owner=" << aiOwnerId << ")";
        break;
      }

      if (!command.productType.empty())
        production->productType = command.productType;

      production->timeRemaining = production->buildTime;
      production->inProgress = true;

      qDebug() << "[AICommandApplier] ✓ Started production of"
               << QString::fromStdString(command.productType) << "for AI"
               << aiOwnerId;
      break;
    }
    }
  }
}

} // namespace Game::Systems::AI
