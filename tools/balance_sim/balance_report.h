#pragma once

#include <QString>

#include <vector>

#include "balance_fixture.h"
#include "battle_simulation.h"

namespace Balance {

struct SideSummary {
  QString label;
  int starting_units{0};
  int starting_cost{0};
  double median_survivors{0.0};
  double mean_survivor_fraction{0.0};
  double mean_damage_ranged{0.0};
  double mean_damage_melee{0.0};
  double mean_cohesion_radius{0.0};
};

struct FixtureSummary {
  QString id;
  QString label;
  int battles{0};

  double a_win_rate{0.0};
  double b_win_rate{0.0};
  double draw_rate{0.0};
  double timeout_rate{0.0};

  double median_victory_seconds{0.0};
  double mean_first_contact_distance{0.0};
  int battles_without_contact{0};

  double left_spawn_win_rate{0.0};
  double spawn_side_bias{0.0};

  SideSummary side_a;
  SideSummary side_b;
  InvalidBehaviourCounts invalid;

  std::vector<QString> expectation_failures;
  [[nodiscard]] auto passed() const -> bool { return expectation_failures.empty(); }

  std::vector<BattleResult> battle_results;
};

auto summarize(const Fixture& fixture,
               std::vector<BattleResult> results) -> FixtureSummary;

auto render_text_report(const std::vector<FixtureSummary>& summaries) -> QString;
auto render_json_report(const std::vector<FixtureSummary>& summaries) -> QByteArray;
auto render_csv_report(const std::vector<FixtureSummary>& summaries) -> QByteArray;

} // namespace Balance
