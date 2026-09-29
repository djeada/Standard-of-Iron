#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void publish_formation_presentation(Engine::Core::World& world, float delta_time);

}
