#pragma once

#include <vector>

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems {

[[nodiscard]] auto allied_units(const Engine::Core::World& world,
                                int owner_id) -> std::vector<Engine::Core::Entity*>;

[[nodiscard]] auto enemy_units(const Engine::Core::World& world,
                               int owner_id) -> std::vector<Engine::Core::Entity*>;

[[nodiscard]] auto troop_count_for(const Engine::Core::World& world,
                                   int owner_id) -> int;

} // namespace Game::Systems
