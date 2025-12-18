#pragma once

#include "nation_id.h"
#include <QVector3D>

namespace Game::Systems {

inline auto get_healing_color(NationID nation_id) -> QVector3D {
  switch (nation_id) {
  case NationID::RomanRepublic:
    return QVector3D(0.3F, 0.6F, 1.0F);
  case NationID::Carthage:
    return QVector3D(0.4F, 1.0F, 0.5F);
  default:
    return QVector3D(0.4F, 1.0F, 0.5F);
  }
}

// Returns true if the nation uses healing beams (Roman)
inline auto uses_healing_beams(NationID nation_id) -> bool {
  return nation_id == NationID::RomanRepublic;
}

// Returns true if the nation uses healing aura (Carthage)
inline auto uses_healing_aura(NationID nation_id) -> bool {
  return nation_id == NationID::Carthage;
}

} // namespace Game::Systems
