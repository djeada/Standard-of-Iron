#pragma once

#include "../core/system.h"

namespace Game::Command {

class CommandSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Command
