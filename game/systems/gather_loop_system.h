#pragma once

#include "../core/system.h"

namespace Game::Systems {

class GatherLoopSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 0.6F;

  static constexpr float k_search_radius = 22.0F;

  static constexpr int k_auto_gather_attempts = 6;

  GatherLoopSystem() = default;
  ~GatherLoopSystem() override = default;

  void update(Engine::Core::World* world, float delta_time) override;

private:
  float m_think_cooldown{0.0F};
};

} // namespace Game::Systems
