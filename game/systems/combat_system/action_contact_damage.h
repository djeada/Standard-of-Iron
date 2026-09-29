#pragma once

#include <QVector3D>

#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void deal_radial_action_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const QVector3D& impact_point,
    float contact_speed);

void deal_weapon_trace_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CombatStateComponent* presentation_state,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition);

void deal_mount_body_impact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition);

} // namespace Game::Systems::Combat
