#pragma once

#include "core/system.h"

namespace Game::Systems {

// Homes that report to the barracks (the default) raise their civilians on their
// own while food allows, and every civilian they raise walks to the nearest
// barracks with room for its household. A civilian the player has given an order
// of their own is left alone. See docs/FOOD_AND_FARMS.md.
class CivilianReportingSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 1.0F;
  // A civilian this close to a reporting home is one of its household.
  static constexpr float k_home_reach = 20.0F;
  // A civilian turned away at a full barracks waits this close to it.
  static constexpr float k_barracks_reach = 12.0F;

  void update(Engine::Core::World* world, float delta_time) override;

private:
  float m_think_cooldown{0.0F};
};

} // namespace Game::Systems
