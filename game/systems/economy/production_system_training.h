#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::ProductionTasks {

void tick_completion_effects(Engine::Core::World& world, float delta_time);

void advance_unit_training(Engine::Core::World& world, float delta_time);

} // namespace Game::Systems::ProductionTasks
