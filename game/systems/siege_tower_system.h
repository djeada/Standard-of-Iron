#pragma once

#include "../core/system.h"

namespace Game::Systems {

class SiegeTowerSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

class WallWalkSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  // How often computer-held towns look over their walls for an assault.
  static constexpr float k_garrison_check_seconds = 2.0F;
  float m_garrison_timer{0.0F};
};

} // namespace Game::Systems
