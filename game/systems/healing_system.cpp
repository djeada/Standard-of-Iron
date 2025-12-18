#include "healing_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "healing_beam_system.h"
#include <QDebug>
#include <cmath>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems {

void HealingSystem::update(Engine::Core::World *world, float delta_time) {
  process_healing(world, delta_time);
}

void HealingSystem::process_healing(Engine::Core::World *world,
                                    float delta_time) {
  auto healers = world->get_entities_with<Engine::Core::HealerComponent>();
  auto *healing_beam_system = world->get_system<HealingBeamSystem>();

  for (auto *healer : healers) {
    if (healer->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *healer_unit = healer->get_component<Engine::Core::UnitComponent>();
    auto *healer_transform =
        healer->get_component<Engine::Core::TransformComponent>();
    auto *healer_comp = healer->get_component<Engine::Core::HealerComponent>();

    if ((healer_unit == nullptr) || (healer_transform == nullptr) ||
        (healer_comp == nullptr)) {
      continue;
    }

    if (healer_unit->health <= 0) {
      continue;
    }

    healer_comp->time_since_last_heal += delta_time;

    if (healer_comp->time_since_last_heal < healer_comp->healing_cooldown) {
      continue;
    }

    bool healed_any = false;
    auto units = world->get_entities_with<Engine::Core::UnitComponent>();
    for (auto *target : units) {
      if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
      auto *target_transform =
          target->get_component<Engine::Core::TransformComponent>();

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

        healer_comp->healing_target_x = target_transform->position.x;
        healer_comp->healing_target_z = target_transform->position.z;

        if (dist > 0.1F) {
          float const target_yaw = std::atan2(dx, dz) * 180.0F / 3.14159265F;
          healer_transform->desired_yaw = target_yaw;
          healer_transform->has_desired_yaw = true;
        }

        if (healing_beam_system != nullptr) {
          QVector3D const healer_pos(healer_transform->position.x,
                                     healer_transform->position.y + 1.2F,
                                     healer_transform->position.z);
          QVector3D const target_pos(target_transform->position.x,
                                     target_transform->position.y + 0.8F,
                                     target_transform->position.z);

          QVector3D const heal_color(0.4F, 1.0F, 0.5F);
          healing_beam_system->spawn_beam(healer_pos, target_pos, heal_color,
                                          0.7F);
        }

        healed_any = true;
      }
    }

    if (healed_any) {
      healer_comp->time_since_last_heal = 0.0F;

      healer_comp->is_healing_active = true;
    } else {
      healer_comp->is_healing_active = false;
    }
  }
}

} 
