#pragma once

#include "../../core/entity.h"
#include "combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::CombatActions {

struct ProjectileReleaseResult {
  bool attempted{false};
  bool released{false};
  Engine::Core::EntityID target_id{0};
  int damage{0};
};

[[nodiscard]] auto release_projectile_for_action(
    Engine::Core::World* world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id) -> ProjectileReleaseResult;

} // namespace Game::Systems::CombatActions
