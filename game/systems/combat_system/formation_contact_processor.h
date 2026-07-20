#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void update_formation_contacts(Engine::Core::World* world);

}
