#pragma once

#include <QString>

#include <cstdint>
#include <vector>

#include "balance_fixture.h"

namespace Balance {

enum class Outcome : std::uint8_t {
  SideAWins,
  SideBWins,
  Timeout, // both sides still standing when the clock ran out
  MutualElimination,
};

// Damage split by the attack family that delivered it, so a matchup report can
// show whether a win came from bows or from the melee line.
struct DamageBreakdown {
  double ranged{0.0};
  double melee{0.0};

  [[nodiscard]] auto total() const -> double { return ranged + melee; }
};

// Behaviours that should never happen regardless of how the numbers are tuned.
// These are counted, not thresholded, so the report can surface them even when
// the win rate looks healthy.
struct InvalidBehaviourCounts {
  std::uint32_t friendly_fire_hits{0};
  std::uint32_t ranged_shots_while_melee_locked{0};
  // Unit-seconds spent idle (no target, not moving) with a live enemy in vision.
  double idle_unit_seconds_in_contact{0.0};
};

struct SideResult {
  int starting_units{0};
  int surviving_units{0};
  int starting_cost{0};
  double starting_health{0.0};
  double surviving_health{0.0};
  DamageBreakdown damage_dealt;
  // Mean RMS spread of the side's units around their own centroid while both
  // sides were alive. Rising spread means the formation is coming apart.
  double mean_cohesion_radius{0.0};
};

struct BattleResult {
  std::uint32_t seed{0};
  // False when the fixture's sides were swapped for this run.
  bool sides_swapped{false};
  Outcome outcome{Outcome::Timeout};
  float elapsed_seconds{0.0F};
  // Centroid separation at the first damaging hit; negative if never contacted.
  float first_contact_distance{-1.0F};
  float first_contact_time{-1.0F};
  SideResult side_a;
  SideResult side_b;
  InvalidBehaviourCounts invalid;
};

// Loads troop/nation data and registers unit factories. Safe to call repeatedly;
// only the first call does work. Requires a live QCoreApplication.
void initialize_simulation_environment();

// One line per sampled second, for diagnosing why a fixture behaves oddly.
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

// Runs one deterministic battle. The same fixture + seed + swap flag always
// produces an identical BattleResult. Pass `trace` to collect per-second state.
auto run_battle(const Fixture& fixture,
                std::uint32_t seed,
                bool swap_sides,
                std::vector<TraceSample>* trace = nullptr) -> BattleResult;

auto outcome_name(Outcome outcome) -> QString;

} // namespace Balance
