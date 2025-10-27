#pragma once

#include <QString>

enum class CursorMode { Normal, Patrol, Attack };

namespace CursorModeUtils {

inline auto toString(CursorMode mode) -> QString {
  switch (mode) {
  case CursorMode::Normal:
    return "normal";
  case CursorMode::Patrol:
    return "patrol";
  case CursorMode::Attack:
    return "attack";
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
  default:
    return CursorMode::Normal;
  }
}

} // namespace CursorModeUtils
