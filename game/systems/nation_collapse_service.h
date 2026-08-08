#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems {

namespace NationCollapse {

[[nodiscard]] auto has_living_commander(Engine::Core::World& world,
                                        int owner_id) -> bool;

auto collapse_owner(Engine::Core::World& world, int owner_id) -> bool;

} // namespace NationCollapse

} // namespace Game::Systems
