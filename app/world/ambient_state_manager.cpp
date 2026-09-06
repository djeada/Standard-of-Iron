#include "app/world/ambient_state_manager.h"

#include <QDebug>

#include "app/core/game_engine.h"
#include "game/core/component_combat.h"
#include "game/core/world.h"

AmbientStateManager::AmbientStateManager() = default;

namespace {

constexpr float k_check_interval_seconds = 2.0F;

constexpr float k_combat_release_seconds = 10.0F;

constexpr float k_state_settle_seconds = 4.0F;

} // namespace

auto AmbientStateManager::settle_seconds(Engine::Core::AmbientState from,
                                         Engine::Core::AmbientState to) -> float {

  if (to == Engine::Core::AmbientState::VICTORY ||
      to == Engine::Core::AmbientState::DEFEAT ||
      to == Engine::Core::AmbientState::COMBAT) {
    return 0.0F;
  }
  if (from == Engine::Core::AmbientState::COMBAT) {
    return k_combat_release_seconds;
  }
  return k_state_settle_seconds;
}

auto AmbientStateManager::observe_state(Engine::Core::World* world,
                                        int local_owner_id,
                                        const EntityCache& entity_cache,
                                        const QString& victory_state) const
    -> Engine::Core::AmbientState {
  if (!victory_state.isEmpty()) {
    if (victory_state == "victory") {
      return Engine::Core::AmbientState::VICTORY;
    }
    if (victory_state == "defeat") {
      return Engine::Core::AmbientState::DEFEAT;
    }
  }
  if (is_player_in_combat(world, local_owner_id)) {
    return Engine::Core::AmbientState::COMBAT;
  }
  if (entity_cache.enemy_barracks_alive && entity_cache.player_barracks_alive) {
    return Engine::Core::AmbientState::TENSE;
  }
  return Engine::Core::AmbientState::PEACEFUL;
}

void AmbientStateManager::update(float dt,
                                 Engine::Core::World* world,
                                 int local_owner_id,
                                 const EntityCache& entity_cache,
                                 const QString& victory_state) {
  m_ambient_check_timer += dt;

  if (m_ambient_check_timer < k_check_interval_seconds) {
    return;
  }
  const float elapsed = m_ambient_check_timer;
  m_ambient_check_timer = 0.0F;

  const Engine::Core::AmbientState observed =
      observe_state(world, local_owner_id, entity_cache, victory_state);

  if (observed == m_current_ambient_state) {
    m_candidate_state = observed;
    m_candidate_seconds = 0.0F;
    return;
  }

  if (observed != m_candidate_state) {
    m_candidate_state = observed;
    m_candidate_seconds = 0.0F;
  }
  m_candidate_seconds += elapsed;

  if (m_candidate_seconds < settle_seconds(m_current_ambient_state, observed)) {
    return;
  }

  Engine::Core::AmbientState const previous_state = m_current_ambient_state;
  m_current_ambient_state = observed;
  m_candidate_seconds = 0.0F;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(observed, previous_state));

  qInfo() << "Ambient state changed from" << static_cast<int>(previous_state) << "to"
          << static_cast<int>(observed);
}

auto AmbientStateManager::is_player_in_combat(Engine::Core::World* world,
                                              int local_owner_id) const -> bool {
  if (!world) {
    return false;
  }

  auto units = world->collect_entities_with<Engine::Core::UnitComponent>();
  const float combat_check_radius = 15.0F;

  for (auto* entity : units) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id || unit->health <= 0) {
      continue;
    }

    if (entity->has_component<Engine::Core::AttackTargetComponent>()) {
      return true;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    for (auto* other_entity : units) {
      auto* other_unit = other_entity->get_component<Engine::Core::UnitComponent>();
      if ((other_unit == nullptr) || other_unit->owner_id == local_owner_id ||
          other_unit->health <= 0) {
        continue;
      }

      auto* other_transform =
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
