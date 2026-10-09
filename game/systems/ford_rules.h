#pragma once

#include <algorithm>

#include "../core/component_gameplay.h"

// How wading a river ford changes a unit. The per-ford numbers (depth, speed,
// cold, exposure) are authored on the map; the constants here are the shared
// rules that turn them into speed, damage and stamina effects.
namespace Game::Systems::FordRules {

// Missiles find men in the water easier still: on top of the ford's exposure.
inline constexpr float k_ranged_exposure_bonus = 1.10F;

// A unit fighting from the water cannot set its feet: its blows land softer.
inline constexpr float k_wading_attack_multiplier = 0.85F;

// Icy water (cold = 1) fills the chill in about twelve seconds of wading and
// it takes a minute on dry ground to wear off.
inline constexpr float k_chill_gain_per_second = 0.08F;
inline constexpr float k_chill_decay_per_second = 1.0F / 60.0F;

// At full chill a unit strikes this much weaker and marches this much slower.
inline constexpr float k_chill_attack_penalty = 0.15F;
inline constexpr float k_chill_speed_penalty = 0.10F;

// Stamina spent per second in water of cold = 1.
inline constexpr float k_cold_stamina_drain_per_second = 8.0F;

[[nodiscard]] inline auto
speed_multiplier(const Engine::Core::WadingComponent* wading) noexcept -> float {
  if (wading == nullptr) {
    return 1.0F;
  }
  float multiplier = 1.0F - (k_chill_speed_penalty * std::clamp(wading->chill, 0.0F, 1.0F));
  if (wading->in_water()) {
    multiplier *= std::clamp(wading->speed, 0.05F, 1.0F);
  }
  return multiplier;
}

// Running is impossible with the water at your waist.
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
    multiplier *= 1.0F - (k_chill_attack_penalty * std::clamp(attacker->chill, 0.0F, 1.0F));
  }
  if (target != nullptr && target->in_water()) {
    multiplier *= std::max(1.0F, target->exposure);
    if (ranged) {
      multiplier *= k_ranged_exposure_bonus;
    }
  }
  return multiplier;
}

// A spear wall cannot be set in a river.
[[nodiscard]] inline auto
can_brace(const Engine::Core::WadingComponent* wading) noexcept -> bool {
  return wading == nullptr || !wading->in_water();
}

[[nodiscard]] inline auto next_chill(float chill,
                                     bool in_water,
                                     float cold,
                                     float delta_time) noexcept -> float {
  float const dt = std::max(0.0F, delta_time);
  if (in_water && cold > 0.0F) {
    return std::min(1.0F, chill + (k_chill_gain_per_second * cold * dt));
  }
  return std::max(0.0F, chill - (k_chill_decay_per_second * dt));
}

} // namespace Game::Systems::FordRules
