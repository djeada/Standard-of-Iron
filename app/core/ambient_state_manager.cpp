#include "ambient_state_manager.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include <QDebug>

AmbientStateManager::AmbientStateManager() = default;

void AmbientStateManager::update(float dt, Engine::Core::World *world,
                                  int local_owner_id,
                                  const EntityCache &entity_cache,
                                  const QString &victory_state) {
  m_ambient_check_timer += dt;
  const float check_interval = 2.0F;

  if (m_ambient_check_timer < check_interval) {
    return;
  }
  m_ambient_check_timer = 0.0F;

  Engine::Core::AmbientState new_state = Engine::Core::AmbientState::PEACEFUL;

  if (!victory_state.isEmpty()) {
    if (victory_state == "victory") {
      new_state = Engine::Core::AmbientState::VICTORY;
    } else if (victory_state == "defeat") {
      new_state = Engine::Core::AmbientState::DEFEAT;
    }
  } else if (is_player_in_combat(world, local_owner_id)) {
    new_state = Engine::Core::AmbientState::COMBAT;
  } else if (entity_cache.enemy_barracks_alive &&
             entity_cache.player_barracks_alive) {
    new_state = Engine::Core::AmbientState::TENSE;
  }

  if (new_state != m_current_ambient_state) {
    Engine::Core::AmbientState const previous_state = m_current_ambient_state;
    m_current_ambient_state = new_state;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(new_state, previous_state));

    qInfo() << "Ambient state changed from" << static_cast<int>(previous_state)
            << "to" << static_cast<int>(new_state);
  }
}

auto AmbientStateManager::is_player_in_combat(Engine::Core::World *world,
                                              int local_owner_id) const
    -> bool {
  if (!world) {
    return false;
  }

  auto units = world->get_entities_with<Engine::Core::UnitComponent>();
  const float combat_check_radius = 15.0F;

  for (auto *entity : units) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id ||
        unit->health <= 0) {
      continue;
    }

    if (entity->has_component<Engine::Core::AttackTargetComponent>()) {
      return true;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    for (auto *other_entity : units) {
      auto *other_unit =
          other_entity->get_component<Engine::Core::UnitComponent>();
      if ((other_unit == nullptr) || other_unit->owner_id == local_owner_id ||
          other_unit->health <= 0) {
        continue;
      }

      auto *other_transform =
          other_entity->get_component<Engine::Core::TransformComponent>();
      if (other_transform == nullptr) {
        continue;
      }

      float const dx = transform->position.x - other_transform->position.x;
      float const dz = transform->position.z - other_transform->position.z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq < combat_check_radius * combat_check_radius) {
        return true;
      }
    }
  }

  return false;
}
