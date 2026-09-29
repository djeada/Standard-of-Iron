#pragma once

#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void apply_rts_commander_root_motion(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float delta_time);

}
