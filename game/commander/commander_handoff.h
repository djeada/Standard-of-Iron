#pragma once

#include "game/core/entity_id.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Commander {

[[nodiscard]] auto find_commander(Engine::Core::World& world,
                                  int owner) -> Engine::Core::Entity*;
void begin_control(Engine::Core::Entity& commander);
void end_control(Engine::Core::World* world, Engine::Core::EntityID commander_id);
void resume_control(Engine::Core::Entity& commander);

} // namespace Game::Commander
