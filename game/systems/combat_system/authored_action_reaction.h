#pragma once

#include <QVector3D>

#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"
#include "combat_hit_resolver.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

[[nodiscard]] auto authored_hit_stop_seconds(
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> float;

void apply_authored_action_reaction(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const CombatHitResult& result,
    const QVector3D& contact_point,
    float contact_speed);

} // namespace Game::Systems::Combat
