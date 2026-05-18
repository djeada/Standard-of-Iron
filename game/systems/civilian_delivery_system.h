#pragma once

#include "../core/system.h"

namespace Game::Systems {

inline constexpr int k_civilian_delivery_population_grant = 50;

class CivilianDeliverySystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
