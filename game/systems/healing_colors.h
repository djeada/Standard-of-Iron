#pragma once

#include "nation_id.h"
#include <QVector3D>
#include <cmath>

namespace Game::Systems {

inline auto get_roman_healing_color() -> QVector3D {
  return QVector3D(0.3F, 0.6F, 1.0F);
}

inline auto get_carthage_healing_color() -> QVector3D {
  return QVector3D(0.4F, 1.0F, 0.5F);
}

inline auto get_healing_color(NationID nation_id) -> QVector3D {
  switch (nation_id) {
  case NationID::RomanRepublic:
    return get_roman_healing_color();
  case NationID::Carthage:
    return get_carthage_healing_color();
  default:
    return get_carthage_healing_color();
  }
}

inline auto is_roman_healing_color(const QVector3D &color) -> bool {
  constexpr float tolerance = 0.1F;
  QVector3D roman_color = get_roman_healing_color();
  return (std::abs(color.x() - roman_color.x()) < tolerance &&
          std::abs(color.y() - roman_color.y()) < tolerance &&
          std::abs(color.z() - roman_color.z()) < tolerance);
}

} 
