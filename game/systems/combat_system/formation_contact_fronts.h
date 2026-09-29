#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void publish_formation_contacts(Engine::Core::World& world);

}
