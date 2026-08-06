#pragma once

#include <vector>

#include "../core/system.h"

namespace Game::Systems {

class SettlementLifeSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 0.35F;
  static constexpr float k_arrival_radius = 1.35F;
  static constexpr float k_min_errand_distance = 2.5F;

  static constexpr float k_adoption_interval = 2.0F;
  static constexpr float k_adoption_radius = 20.0F;
  static constexpr float k_adopted_roam_radius = 14.0F;

  static constexpr float k_alarm_interval = 0.3F;
  static constexpr float k_alarm_radius = 11.0F;
  static constexpr float k_all_clear_radius = 15.0F;
  static constexpr float k_flee_distance = 11.0F;
  static constexpr float k_flee_leg_seconds = 2.5F;

  struct ArmedUnit {
    float x{0.0F};
    float z{0.0F};
    int owner_id{0};
  };

  SettlementLifeSystem() = default;
  ~SettlementLifeSystem() override = default;

  void update(Engine::Core::World* world, float delta_time) override;

private:
  float m_adoption_cooldown{k_adoption_interval};
  float m_alarm_cooldown{0.0F};
  std::vector<ArmedUnit> m_armed_units;
};

} // namespace Game::Systems
