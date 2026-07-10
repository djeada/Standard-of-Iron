#pragma once

#include <span>

#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class Entity;
class World;
enum class MountedChargeCancelReason : std::uint8_t;
enum class MountedChargeIntentSource : std::uint8_t;
} // namespace Engine::Core

namespace Game::Systems::Combat {

[[nodiscard]] auto
request_mounted_charge(Engine::Core::Entity& entity,
                       Engine::Core::MountedChargeIntentSource source) -> bool;

[[nodiscard]] auto
cancel_mounted_charge(Engine::Core::Entity& entity,
                      Engine::Core::MountedChargeCancelReason reason) -> bool;

void process_mounted_charge_intents(Engine::Core::World* world, float delta_time);

void handle_mounted_charge_action_events(
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    std::span<const Game::Systems::CombatActions::CombatActionEvent> events);

} // namespace Game::Systems::Combat
