#include "action_vfx.h"
#include "../core/world.h"
#include "../core/entity.h"
#include "../core/component.h"
#include "../systems/arrow_system.h"
#include "../game_config.h"

namespace Game {
namespace Visuals {

void ActionVFX::spawnAttackArrow(Engine::Core::World &world,
                                Engine::Core::Entity *targetEntity) {
  if (!targetEntity)
    return;

  auto *arrowSystem = world.getSystem<Game::Systems::ArrowSystem>();
  if (!arrowSystem)
    return;

  auto *targetTrans =
      targetEntity->getComponent<Engine::Core::TransformComponent>();
  if (!targetTrans)
    return;

  QVector3D targetPos(targetTrans->position.x,
                      targetTrans->position.y + 1.0f,
                      targetTrans->position.z);
  QVector3D aboveTarget = targetPos + QVector3D(0, 2.0f, 0);

  arrowSystem->spawnArrow(aboveTarget, targetPos,
                          QVector3D(1.0f, 0.2f, 0.2f),
                          Game::GameConfig::instance().arrow().speedAttack);
}

} // namespace Visuals
} // namespace Game
