#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems {

void register_runtime_systems(Engine::Core::World& world);

}
