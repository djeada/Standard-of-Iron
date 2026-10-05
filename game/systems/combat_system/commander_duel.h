#pragma once

#include <optional>

#include "../../core/component_commander.h"
#include "../combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CombatQueryContext;

void process_commander_duels(Engine::Core::World* world,
                             const CombatQueryContext& query_context,
                             float delta_time);

[[nodiscard]] auto duelling_with(const Engine::Core::Entity& entity,
                                 const Engine::Core::Entity& other) -> bool;

[[nodiscard]] auto duel_permits_attack(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto duel_forces_swing(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto duel_permits_signature(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto claim_duel_link(Engine::Core::Entity& attacker,
                                   Engine::Core::Entity& target)
    -> std::optional<Game::Systems::CombatActions::CombatActionId>;

[[nodiscard]] auto duel_swing_cadence(const Engine::Core::Entity& attacker,
                                      float cooldown,
                                      float link_length) -> std::optional<float>;

struct DuelContact {
  Engine::Core::CommanderDuelOutcome outcome{Engine::Core::CommanderDuelOutcome::None};
  int damage{0};
};

[[nodiscard]] auto resolve_duel_contact(Engine::Core::Entity& attacker,
                                        Engine::Core::Entity& target,
                                        int base_damage) -> std::optional<DuelContact>;

void present_duel_contact(Engine::Core::World& world,
                          Engine::Core::Entity& attacker,
                          Engine::Core::Entity& target,
                          const Engine::Core::TransformComponent& attacker_transform,
                          const Engine::Core::TransformComponent& target_transform,
                          const DuelContact& contact);

} // namespace Game::Systems::Combat
