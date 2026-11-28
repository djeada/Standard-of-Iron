#pragma once

#include <cstdint>
#include <optional>
#include <source_location>

#include "game/systems/combat_actions/combat_action_definition.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class UnitComponent;
class CommanderComponent;
class MovementComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Commander {

struct CommanderHandles {
  Engine::Core::Entity& entity;
  Engine::Core::TransformComponent& transform;
  Engine::Core::UnitComponent& unit;
  Engine::Core::CommanderComponent* commander_data;
  Engine::Core::MovementComponent* movement;
};

[[nodiscard]] auto controlled_commander(Engine::Core::World& world,
                                        Engine::Core::EntityID commander_id,
                                        int local_owner_id) -> Engine::Core::Entity*;

[[nodiscard]] auto body_allows_now(const Engine::Core::Entity& commander)
    -> Game::Systems::CombatActions::MeleeInterruption;

void cancel_current_attack(Engine::Core::Entity& commander);
void cancel_held_bow_draw(Engine::Core::Entity& commander);
void restore_view_preferences(Engine::Core::World& world,
                              Engine::Core::EntityID commander_id,
                              std::optional<bool> close_camera,
                              std::optional<bool> bow_stance);
void publish_cue(
    int owner,
    const char* cue,
    const std::source_location& location = std::source_location::current());
void publish_cue(
    const Engine::Core::Entity& commander,
    const char* cue,
    const std::source_location& location = std::source_location::current());

} // namespace Game::Commander
