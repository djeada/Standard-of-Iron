#pragma once

#include "../core/system.h"

namespace Game::Systems {

class CivilianDeliverySystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static constexpr float k_delivery_radius = 2.5F;
};

} // namespace Game::Systems
