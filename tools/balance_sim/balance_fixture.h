#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

#include "game/systems/nation_id.h"
#include "game/units/troop_type.h"

namespace Balance {

// How a side behaves once the clock starts. Stances are deliberately coarse:
// they map onto the orders a player can actually issue, so a fixture result is
// reproducible by hand in the real game.
enum class Stance : std::uint8_t {
  Attack, // walk into the enemy and engage
  Hold,   // hold-mode in place, let the enemy come
  Stand,  // no orders at all, rely on auto-engagement
  Charge, // mounted charge intent + attack (cavalry only)
};

struct FixtureGroup {
  Game::Units::TroopType troop{Game::Units::TroopType::Swordsman};
  int count{1};
};

struct FixtureSide {
  QString label;
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};
  Stance stance{Stance::Attack};
  std::vector<FixtureGroup> groups;
};

// Expectations are optional. When present the runner turns them into pass/fail
// verdicts so the matrix doubles as a regression suite.
struct FixtureExpectation {
  std::optional<float> a_win_rate_min;
  std::optional<float> a_win_rate_max;
  std::optional<float> max_timeout_rate;
  // Mirror matchups should not favour either spawn side.
  std::optional<float> max_spawn_side_bias;
};

struct Fixture {
  QString id;
  QString label;
  QString description;

  float duration_seconds{120.0F};
  float timestep{1.0F / 30.0F};
  int seeds{8};
  // Re-run every seed with the two sides swapped so spawn-side effects cancel.
  bool mirror_sides{true};

  int grid_width{96};
  int grid_height{96};
  // Distance between the two armies' centres at spawn.
  float separation{24.0F};
  // Per-seed positional jitter, in world units.
  float spawn_jitter{1.5F};

  FixtureSide side_a;
  FixtureSide side_b;
  FixtureExpectation expect;
};

struct FixtureLoadError {
  QString field;
  QString message;
};

// Parses one fixture file. Returns nullopt and fills `errors` on failure.
auto load_fixture_file(const QString& path,
                       std::vector<FixtureLoadError>& errors) -> std::optional<Fixture>;

// Loads every *.json in a directory, sorted by filename for stable ordering.
auto load_fixture_directory(const QString& directory,
                            std::vector<FixtureLoadError>& errors)
    -> std::vector<Fixture>;

auto stance_name(Stance stance) -> QString;

} // namespace Balance
