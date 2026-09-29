#pragma once

#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void resolve_rts_melee_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Engine::Core::Entity& target);

void deal_rts_melee_contact_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition);

} // namespace Game::Systems::Combat
