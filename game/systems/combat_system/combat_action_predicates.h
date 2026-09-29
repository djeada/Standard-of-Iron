#pragma once

#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

[[nodiscard]] auto
is_rts_melee_action(Game::Systems::CombatActions::CombatActionId id) -> bool;

[[nodiscard]] auto
is_rts_attack_action(Game::Systems::CombatActions::CombatActionId id) -> bool;

[[nodiscard]] auto action_swings_a_traced_weapon(
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool;

[[nodiscard]] auto is_advanced_rts_commander_melee(
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool;

[[nodiscard]] auto commander_swings_under_player_control(
    const Engine::Core::CommanderComponent* commander,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool;

[[nodiscard]] auto target_uses_rpg_combat(Engine::Core::World& world,
                                          Engine::Core::EntityID target_id) -> bool;

[[nodiscard]] auto
rts_melee_reach(const Engine::Core::AttackComponent* attack,
                const Engine::Core::CommanderComponent* commander,
                const Game::Systems::CombatActions::CombatActionDefinition& definition,
                bool advanced_commander_melee) -> float;

[[nodiscard]] auto rts_melee_target_still_stands(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool;

[[nodiscard]] auto melee_contact_comes_from_the_sweep(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool;

void halt_action(Engine::Core::RpgCommanderActionComponent& action);

} // namespace Game::Systems::Combat
