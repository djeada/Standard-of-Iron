#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems {

[[nodiscard]] auto troop_count_for(const Engine::Core::World& world,
                                   int owner_id) -> int;

} // namespace Game::Systems
