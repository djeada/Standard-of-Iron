#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::ProductionTasks {

void sync_site_ghosts(Engine::Core::World& world);

} // namespace Game::Systems::ProductionTasks
