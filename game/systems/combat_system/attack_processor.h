#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void process_attacks(Engine::Core::World *world, float delta_time);

}
