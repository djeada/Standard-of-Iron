#pragma once

#include "../core/system.h"

namespace Game::Systems {

class SettlementLifeSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 0.35F;
  static constexpr float k_arrival_radius = 1.35F;
  static constexpr float k_min_errand_distance = 2.5F;

  SettlementLifeSystem() = default;
  ~SettlementLifeSystem() override = default;

  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
