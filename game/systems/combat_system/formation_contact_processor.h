#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void update_formation_contacts(Engine::Core::World* world,
                               float delta_time = 1.0F / 60.0F);

}
