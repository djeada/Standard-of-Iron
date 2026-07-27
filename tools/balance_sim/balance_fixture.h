#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

#include "game/systems/nation_id.h"
#include "game/units/troop_type.h"

namespace Balance {

enum class Stance : std::uint8_t {
  Attack,
  Hold,
  Stand,
  Charge,
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

struct FixtureExpectation {
  std::optional<float> a_win_rate_min;
  std::optional<float> a_win_rate_max;
  std::optional<float> max_timeout_rate;

  std::optional<float> max_spawn_side_bias;
};

struct Fixture {
  QString id;
  QString label;
  QString description;

  float duration_seconds{120.0F};
  float timestep{1.0F / 30.0F};
  int seeds{8};

  bool mirror_sides{true};

  int grid_width{96};
  int grid_height{96};

  float separation{24.0F};

  float spawn_jitter{1.5F};

  FixtureSide side_a;
  FixtureSide side_b;
  FixtureExpectation expect;
};

struct FixtureLoadError {
  QString field;
  QString message;
};

auto load_fixture_file(const QString& path,
                       std::vector<FixtureLoadError>& errors) -> std::optional<Fixture>;

auto load_fixture_directory(const QString& directory,
                            std::vector<FixtureLoadError>& errors)
    -> std::vector<Fixture>;

auto stance_name(Stance stance) -> QString;

} // namespace Balance
