#include "healing_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "arrow_system.h"
#include <cmath>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems {

void HealingSystem::update(Engine::Core::World *world, float delta_time) {
  processHealing(world, delta_time);
}

void HealingSystem::processHealing(Engine::Core::World *world,
                                   float delta_time) {
  auto healers = world->getEntitiesWith<Engine::Core::HealerComponent>();
  auto *arrow_system = world->getSystem<ArrowSystem>();

  for (auto *healer : healers) {
    if (healer->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *healer_unit = healer->getComponent<Engine::Core::UnitComponent>();
    auto *healer_transform =
        healer->getComponent<Engine::Core::TransformComponent>();
    auto *healer_comp = healer->getComponent<Engine::Core::HealerComponent>();

    if ((healer_unit == nullptr) || (healer_transform == nullptr) ||
        (healer_comp == nullptr)) {
      continue;
    }

    if (healer_unit->health <= 0) {
      continue;
    }

    healer_comp->timeSinceLastHeal += delta_time;

    if (healer_comp->timeSinceLastHeal < healer_comp->healing_cooldown) {
      continue;
    }

    bool healed_any = false;
    auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto *target : units) {
      if (target->hasComponent<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto *target_unit = target->getComponent<Engine::Core::UnitComponent>();
      auto *target_transform =
          target->getComponent<Engine::Core::TransformComponent>();

      if ((target_unit == nullptr) || (target_transform == nullptr)) {
        continue;
      }

      if (target_unit->health <= 0 ||
          target_unit->health >= target_unit->max_health) {
        continue;
      }

      if (target_unit->owner_id != healer_unit->owner_id) {
        continue;
      }

      float const dx =
          target_transform->position.x - healer_transform->position.x;
      float const dz =
          target_transform->position.z - healer_transform->position.z;
      float const dist = std::sqrt(dx * dx + dz * dz);

      if (dist <= healer_comp->healing_range) {
        target_unit->health += healer_comp->healing_amount;
        if (target_unit->health > target_unit->max_health) {
          target_unit->health = target_unit->max_health;
        }

        if (arrow_system != nullptr) {
          QVector3D const healer_pos(healer_transform->position.x,
                                     healer_transform->position.y + 1.0F,
                                     healer_transform->position.z);
          QVector3D const target_pos(target_transform->position.x,
                                     target_transform->position.y + 1.0F,
                                     target_transform->position.z);
          arrow_system->spawnArrow(healer_pos, target_pos,
                                   QVector3D(0.2F, 1.0F, 0.4F), 6.0F);
        }

        healed_any = true;
      }
    }

    if (healed_any) {
      healer_comp->timeSinceLastHeal = 0.0F;
    }
  }
}

} // namespace Game::Systems
