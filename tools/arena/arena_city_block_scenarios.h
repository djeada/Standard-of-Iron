#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// A perimeter block of town housing: terraces of tenements ("style":
// "tenement" homes) standing wall to wall on all four sides of a court, their
// fronts on the streets. Lots are laid on the homes' own navigation footprint,
// so the block is exactly as solid to path-finding as it looks.
struct TenementBlock {
  QString name;
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};
  int owner{1};
  // North-west corner of the block's lots.
  QVector3D origin;
  int lots_wide{8};
  int lots_deep{3};
  // Lot of the north terrace left open as the alley into the court, or -1.
  int alley_lot{-1};
};

struct TenementBlockExtent {
  float width;
  float depth;
};

[[nodiscard]] auto tenement_block_extent(int lots_wide,
                                         int lots_deep) -> TenementBlockExtent;

// Adds the block's terraces to `scenario` and returns the group names it made.
auto lay_tenement_block(ArenaScenarioDefinition& scenario,
                        const TenementBlock& block) -> std::vector<QString>;

[[nodiscard]] auto
build_city_block_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
