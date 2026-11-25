#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

struct TenementBlock {
  QString name;
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};
  int owner{1};

  QVector3D origin;
  int lots_wide{8};
  int lots_deep{3};

  int alley_lot{-1};
};

struct TenementBlockExtent {
  float width;
  float depth;
};

[[nodiscard]] auto tenement_block_extent(int lots_wide,
                                         int lots_deep) -> TenementBlockExtent;

auto lay_tenement_block(ArenaScenarioDefinition& scenario,
                        const TenementBlock& block) -> std::vector<QString>;

[[nodiscard]] auto
build_city_block_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
