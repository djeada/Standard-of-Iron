#pragma once

#include "../core/system.h"

namespace Game::Systems {

class TownGrowthSystem : public Engine::Core::System {
public:
  static constexpr int k_town_homes = 10;
  static constexpr int k_city_homes = 20;

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
  static constexpr float k_update_interval = 4.0F;

  float m_cooldown{0.0F};
};

} // namespace Game::Systems
