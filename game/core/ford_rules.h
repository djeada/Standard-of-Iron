#pragma once

#include <algorithm>

#include "component_gameplay.h"

namespace Game::Systems::FordRules {

inline constexpr float k_ranged_exposure_bonus = 1.10F;

inline constexpr float k_wading_attack_multiplier = 0.85F;

inline constexpr float k_chill_gain_per_second = 0.08F;
inline constexpr float k_chill_decay_per_second = 1.0F / 60.0F;

inline constexpr float k_chill_attack_penalty = 0.15F;
inline constexpr float k_chill_speed_penalty = 0.10F;

inline constexpr float k_cold_stamina_drain_per_second = 8.0F;

[[nodiscard]] inline auto
speed_multiplier(const Engine::Core::WadingComponent* wading) noexcept -> float {
  if (wading == nullptr) {
    return 1.0F;
  }
  float multiplier =
      1.0F - (k_chill_speed_penalty * std::clamp(wading->chill, 0.0F, 1.0F));
  if (wading->in_water()) {
    multiplier *= std::clamp(wading->speed, 0.05F, 1.0F);
  }
  return multiplier;
}

[[nodiscard]] inline auto
can_run(const Engine::Core::WadingComponent* wading) noexcept -> bool {
  return wading == nullptr || !wading->in_water();
}

[[nodiscard]] inline auto
damage_multiplier(const Engine::Core::WadingComponent* attacker,
                  const Engine::Core::WadingComponent* target,
                  bool ranged) noexcept -> float {
  float multiplier = 1.0F;
  if (attacker != nullptr) {
    if (attacker->in_water()) {
      multiplier *= k_wading_attack_multiplier;
    }
    multiplier *=
        1.0F - (k_chill_attack_penalty * std::clamp(attacker->chill, 0.0F, 1.0F));
  }
  if (target != nullptr && target->in_water()) {
    multiplier *= std::max(1.0F, target->exposure);
    if (ranged) {
      multiplier *= k_ranged_exposure_bonus;
    }
  }
  return multiplier;
}

[[nodiscard]] inline auto
can_brace(const Engine::Core::WadingComponent* wading) noexcept -> bool {
  return wading == nullptr || !wading->in_water();
}

[[nodiscard]] inline auto
next_chill(float chill, bool in_water, float cold, float delta_time) noexcept -> float {
  float const dt = std::max(0.0F, delta_time);
  if (in_water && cold > 0.0F) {
    return std::min(1.0F, chill + (k_chill_gain_per_second * cold * dt));
  }
  return std::max(0.0F, chill - (k_chill_decay_per_second * dt));
}

} // namespace Game::Systems::FordRules
