#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::ProductionTasks {

void advance_builders(Engine::Core::World& world, float delta_time);

} // namespace Game::Systems::ProductionTasks
