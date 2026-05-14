#pragma once

#include "../core/component.h"
#include "../core/entity.h"

namespace Game::Systems::CombatRules {

[[nodiscard]] inline auto
uses_rpg_combat_rules(const Engine::Core::Entity *entity) -> bool {
  if (entity == nullptr) {
    return false;
  }

  auto const *commander =
      entity->get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && commander->fpv_controlled) {
    return true;
  }

  auto const *rpg = entity->get_component<Engine::Core::RpgHealthComponent>();
  return (rpg != nullptr) && rpg->active;
}

[[nodiscard]] inline auto
participates_in_rts_melee_lock(const Engine::Core::Entity *entity) -> bool {
  return !uses_rpg_combat_rules(entity);
}

inline void clear_rts_melee_lock(Engine::Core::Entity *entity) {
  if (entity == nullptr) {
    return;
  }

  auto *attack = entity->get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr) {
    return;
  }

  attack->in_melee_lock = false;
  attack->melee_lock_target_id = 0;
}

inline void clear_rts_combat_tracking(Engine::Core::Entity *entity) {
  if (entity == nullptr) {
    return;
  }

  clear_rts_melee_lock(entity);
  entity->remove_component<Engine::Core::AttackTargetComponent>();
}

} // namespace Game::Systems::CombatRules
