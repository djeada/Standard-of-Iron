#pragma once

#include "../core/system.h"

namespace Game::Systems {

// A town shows its size (PLAY_BIG_SOON 6.10): once an owner has enough homes,
// the houses at the heart of the settlement are rebuilt one at a time as
// multi-storey town housing - the Roman insula, the Punic tall house. It is
// presentation only: the "tenement" building style swaps the renderer and
// leaves footprint, collision and economy alone, so nothing about the match
// changes but what the player sees.
class TownGrowthSystem : public Engine::Core::System {
public:
  // Homes an owner needs before the first house is rebuilt, and the count at
  // which the town is dense enough for most of it to stand tall.
  static constexpr int k_town_homes = 10;
  static constexpr int k_city_homes = 20;

  // How many of an owner's homes should stand as town housing.
  [[nodiscard]] static constexpr auto tall_home_target(int homes) noexcept -> int {
    if (homes >= k_city_homes) {
      return (homes * 2) / 3;
    }
    if (homes >= k_town_homes) {
      return homes / 3;
    }
    return 0;
  }

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

private:
  // One house per owner per pass, so a growing town is seen to grow.
  static constexpr float k_update_interval = 4.0F;

  float m_cooldown{0.0F};
};

} // namespace Game::Systems
