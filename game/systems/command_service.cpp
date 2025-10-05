#include "command_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include <QVector3D>

namespace Game {
namespace Systems {

void CommandService::moveUnits(Engine::Core::World &world,
                               const std::vector<Engine::Core::EntityID> &units,
                               const std::vector<QVector3D> &targets) {
  if (units.size() != targets.size())
    return;
  for (size_t i = 0; i < units.size(); ++i) {
    auto *e = world.getEntity(units[i]);
    if (!e)
      continue;
    auto *mv = e->getComponent<Engine::Core::MovementComponent>();
    if (!mv)
      mv = e->addComponent<Engine::Core::MovementComponent>();
    if (!mv)
      continue;
    mv->targetX = targets[i].x();
    mv->targetY = targets[i].z();
    mv->hasTarget = true;

    e->removeComponent<Engine::Core::AttackTargetComponent>();
  }
}

void CommandService::attackTarget(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    Engine::Core::EntityID targetId, bool shouldChase) {
  if (targetId == 0)
    return;

  for (auto unitId : units) {
    auto *e = world.getEntity(unitId);
    if (!e)
      continue;

    auto *attackTarget = e->getComponent<Engine::Core::AttackTargetComponent>();
    if (!attackTarget) {
      attackTarget = e->addComponent<Engine::Core::AttackTargetComponent>();
    }
    if (!attackTarget)
      continue;

    attackTarget->targetId = targetId;
    attackTarget->shouldChase = shouldChase;
  }
}

} // namespace Systems
} // namespace Game
