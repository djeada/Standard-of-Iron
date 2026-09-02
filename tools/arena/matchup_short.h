#pragma once

#include <QString>

#include <optional>

#include "arena_scenario.h"
#include "game/systems/nation_id.h"
#include "game/units/troop_type.h"
#include "promo_spec.h"

namespace Arena::Matchup {

struct Side {
  int count{3};
  Game::Units::TroopType troop{Game::Units::TroopType::Swordsman};
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};

  bool nation_named{false};
};

struct Matchup {
  Side attacker;
  Side defender;
  int seed{1337};

  float fight_seconds{45.0F};
  float report_seconds{4.0F};

  bool preview{false};
};

inline constexpr int k_max_side_count = 24;

inline constexpr char k_scenario_id[] = "matchup_short";

[[nodiscard]] auto parse(const QString& text, QString* error) -> std::optional<Matchup>;

[[nodiscard]] auto side_label(const Side& side) -> QString;

[[nodiscard]] auto title(const Matchup& matchup) -> QString;

[[nodiscard]] auto build_scenario(const Matchup& matchup) -> ArenaScenarioDefinition;

[[nodiscard]] auto build_spec(const Matchup& matchup) -> Promo::Spec;

} // namespace Arena::Matchup
