#pragma once

#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

[[nodiscard]] auto commander_strike_form(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::CommanderComponent* commander)
    -> Engine::Core::CommanderSignatureForm;

[[nodiscard]] auto commander_strike_intensity(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    bool signature_strike) -> float;

void record_signature_contact(
    Engine::Core::Entity& attacker,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    Engine::Core::CommanderSignatureForm form,
    float intensity = 1.0F,
    float reach = 1.8F);

void record_commander_swing(
    Engine::Core::Entity& attacker,
    const Engine::Core::TransformComponent& attacker_transform,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::CommanderComponent* commander,
    float reach);

void record_swing_for_weapon_trace_start(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Game::Systems::CombatActions::CombatActionId action_id);

void apply_commander_signature_effects(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CommanderComponent& commander,
    Engine::Core::Entity& primary_target,
    const Engine::Core::TransformComponent& attacker_transform,
    float reach,
    int damage);

void apply_signature_shot_effects(Engine::Core::World& world,
                                  Engine::Core::Entity& shooter,
                                  Engine::Core::EntityID target_id);

} // namespace Game::Systems::Combat
