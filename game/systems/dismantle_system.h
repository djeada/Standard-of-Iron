#pragma once

#include "../core/system.h"

namespace Game::Systems {

class DismantleSystem : public Engine::Core::System {
public:
  static constexpr int k_max_crew = 3;

  DismantleSystem() = default;
  ~DismantleSystem() override = default;

  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
