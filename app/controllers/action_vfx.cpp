#include "action_vfx.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/game_config.h"
#include "../../game/systems/arrow_system.h"
#include <qvectornd.h>

namespace App::Controllers {

void ActionVFX::spawnAttackArrow(Engine::Core::World *world,
                                 Engine::Core::EntityID target_id) {
  if (world == nullptr) {
    return;
  }

  auto *arrow_system = world->getSystem<Game::Systems::ArrowSystem>();
  if (arrow_system == nullptr) {
    return;
  }

  auto *target_entity = world->getEntity(target_id);
  if (target_entity == nullptr) {
    return;
  }

  auto *target_trans =
      target_entity->get_component<Engine::Core::TransformComponent>();
  if (target_trans == nullptr) {
    return;
  }

  QVector3D const target_pos(target_trans->position.x,
                             target_trans->position.y + 1.0F,
                             target_trans->position.z);
  QVector3D const above_target = target_pos + QVector3D(0, 2.0F, 0);

  arrow_system->spawnArrow(above_target, target_pos,
                           QVector3D(1.0F, 0.2F, 0.2F),
                           Game::GameConfig::instance().arrow().speedAttack);
}

} // namespace App::Controllers
