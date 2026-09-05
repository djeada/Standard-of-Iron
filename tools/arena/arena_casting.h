#pragma once

#include <QVector3D>

#include <vector>

#include "arena_scenario.h"

namespace Arena {

struct ArenaCastingSide {
  ArenaBattleSideResult census;
  QVector3D color{0.5F, 0.5F, 0.5F};
  int gold{0};
  int food{0};
  int wood{0};
  int stone{0};
  int iron{0};
};

struct ArenaCastingSnapshot {
  bool valid{false};
  float elapsed_seconds{0.0F};
  bool decided{false};
  std::vector<ArenaCastingSide> sides;
};

} // namespace Arena
