#pragma once

#include <QString>

#include <cstdint>
#include <vector>

#include "balance_fixture.h"

namespace Balance {

enum class Outcome : std::uint8_t {
  SideAWins,
  SideBWins,
  Timeout,
  MutualElimination,
};

struct DamageBreakdown {
  double ranged{0.0};
  double melee{0.0};

  [[nodiscard]] auto total() const -> double { return ranged + melee; }
};

struct InvalidBehaviourCounts {
  std::uint32_t friendly_fire_hits{0};
  std::uint32_t ranged_shots_while_melee_locked{0};

  double idle_unit_seconds_in_contact{0.0};
};

struct SideResult {
  int starting_units{0};
  int surviving_units{0};
  int starting_cost{0};
  double starting_health{0.0};
  double surviving_health{0.0};
  DamageBreakdown damage_dealt;

  double mean_cohesion_radius{0.0};
};

struct BattleResult {
  std::uint32_t seed{0};

  bool sides_swapped{false};
  Outcome outcome{Outcome::Timeout};
  float elapsed_seconds{0.0F};

  float first_contact_distance{-1.0F};
  float first_contact_time{-1.0F};
  SideResult side_a;
  SideResult side_b;
  InvalidBehaviourCounts invalid;
};

void initialize_simulation_environment();

struct TraceSample {
  float time_seconds{0.0F};
  int alive_a{0};
  int alive_b{0};
  double health_a{0.0};
  double health_b{0.0};
  float centroid_gap{0.0F};
  int units_with_target{0};
  int units_in_melee_lock{0};
  int units_moving{0};
};

auto run_battle(const Fixture& fixture,
                std::uint32_t seed,
                bool swap_sides,
                std::vector<TraceSample>* trace = nullptr) -> BattleResult;

auto outcome_name(Outcome outcome) -> QString;

} // namespace Balance
