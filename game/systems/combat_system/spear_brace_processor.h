#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void process_spear_brace_state(Engine::Core::World* world, float delta_time);

}
