#pragma once

#include "command.h"

namespace Engine::Core {
class World;
}

namespace Game::Command {

void dispatch(Engine::Core::World& world, const Command& command);

} // namespace Game::Command
