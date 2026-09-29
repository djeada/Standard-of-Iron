#pragma once

#include "game/command/command.h"

namespace Engine::Core {
class World;
}

namespace App::Orders {

void submit_local_command(Engine::Core::World* world, Game::Command::Payload payload);

} // namespace App::Orders
