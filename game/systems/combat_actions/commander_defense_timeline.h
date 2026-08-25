#pragma once

namespace Game::Systems::CombatActions {

struct GuardTimeline {
  float raise_seconds{0.06F};
  float perfect_window_seconds{0.16F};
  float release_seconds{0.08F};
  float break_recovery_seconds{1.0F};
};

struct DodgeTimeline {
  float startup_seconds{0.0F};
  float invulnerable_start_seconds{0.0F};
  float invulnerable_end_seconds{0.12F};
  float roll_seconds{0.22F};
  float recovery_seconds{0.18F};

  [[nodiscard]] constexpr auto invulnerable_seconds() const noexcept -> float {
    return invulnerable_end_seconds - invulnerable_start_seconds;
  }
};

inline constexpr GuardTimeline k_commander_guard_timeline{};
inline constexpr DodgeTimeline k_commander_dodge_timeline{};

} // namespace Game::Systems::CombatActions
