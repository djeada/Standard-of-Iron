#pragma once

#include "../core/system.h"

namespace Game::Systems {

class HomeSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static constexpr float k_update_interval = 2.0F;
  static constexpr float k_max_search_radius = 50.0F;
};

} // namespace Game::Systems
