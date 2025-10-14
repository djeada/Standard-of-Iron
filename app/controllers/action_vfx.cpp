#include "action_vfx.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/game_config.h"
#include "../../game/systems/arrow_system.h"

namespace App::Controllers {

void ActionVFX::spawnAttackArrow(Engine::Core::World *world,
                                Engine::Core::EntityID targetId) {
  if (!world)
    return;

  auto *arrowSystem = world->getSystem<Game::Systems::ArrowSystem>();
  if (!arrowSystem)
    return;

  auto *targetEntity = world->getEntity(targetId);
  if (!targetEntity)
    return;

  auto *targetTrans =
      targetEntity->getComponent<Engine::Core::TransformComponent>();
  if (!targetTrans)
    return;

  QVector3D targetPos(targetTrans->position.x, targetTrans->position.y + 1.0f,
                      targetTrans->position.z);
  QVector3D aboveTarget = targetPos + QVector3D(0, 2.0f, 0);

  arrowSystem->spawnArrow(aboveTarget, targetPos, QVector3D(1.0f, 0.2f, 0.2f),
                         Game::GameConfig::instance().arrow().speedAttack);
}

} 
