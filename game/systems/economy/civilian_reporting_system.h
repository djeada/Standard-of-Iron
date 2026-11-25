#pragma once

#include "core/system.h"

namespace Game::Systems {

class CivilianReportingSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 1.0F;

  static constexpr float k_home_reach = 20.0F;

  static constexpr float k_barracks_reach = 12.0F;

  void update(Engine::Core::World* world, float delta_time) override;

private:
  float m_think_cooldown{0.0F};
};

} // namespace Game::Systems
