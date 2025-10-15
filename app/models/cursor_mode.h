#pragma once

#include <QString>

enum class CursorMode { Normal, Patrol, Attack };

namespace CursorModeUtils {

inline QString toString(CursorMode mode) {
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

inline CursorMode fromString(const QString &str) {
  if (str == "patrol")
    return CursorMode::Patrol;
  if (str == "attack")
    return CursorMode::Attack;
  return CursorMode::Normal;
}

inline int toInt(CursorMode mode) { return static_cast<int>(mode); }

inline CursorMode fromInt(int value) {
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

}
