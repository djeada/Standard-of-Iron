#pragma once

#include <QString>

enum class CursorMode { Normal, Patrol, Attack, Guard, PlaceBuilding, Heal, Build };

namespace CursorModeUtils {

inline auto toString(CursorMode mode) -> QString {
  switch (mode) {
  case CursorMode::Normal:
    return "normal";
  case CursorMode::Patrol:
    return "patrol";
  case CursorMode::Attack:
    return "attack";
  case CursorMode::Guard:
    return "guard";
  case CursorMode::PlaceBuilding:
    return "place_building";
  case CursorMode::Heal:
    return "heal";
  case CursorMode::Build:
    return "build";
  }
  return "normal";
}

inline auto fromString(const QString &str) -> CursorMode {
  if (str == "patrol") {
    return CursorMode::Patrol;
  }
  if (str == "attack") {
    return CursorMode::Attack;
  }
  if (str == "guard") {
    return CursorMode::Guard;
  }
  if (str == "place_building") {
    return CursorMode::PlaceBuilding;
  }
  if (str == "heal") {
    return CursorMode::Heal;
  }
  if (str == "build") {
    return CursorMode::Build;
  }
  return CursorMode::Normal;
}

inline auto toInt(CursorMode mode) -> int { return static_cast<int>(mode); }

inline auto fromInt(int value) -> CursorMode {
  switch (value) {
  case 0:
    return CursorMode::Normal;
  case 1:
    return CursorMode::Patrol;
  case 2:
    return CursorMode::Attack;
  case 3:
    return CursorMode::Guard;
  case 4:
    return CursorMode::PlaceBuilding;
  case 5:
    return CursorMode::Heal;
  case 6:
    return CursorMode::Build;
  default:
    return CursorMode::Normal;
  }
}

} // namespace CursorModeUtils
