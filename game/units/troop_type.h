#pragma once

#include <optional>
#include <string>

namespace Game {
namespace Units {

enum class TroopType { Archer, Knight, Spearman, MountedKnight };

inline std::string troopTypeToString(TroopType type) {
  switch (type) {
  case TroopType::Archer:
    return "archer";
  case TroopType::Knight:
    return "knight";
  case TroopType::Spearman:
    return "spearman";
  case TroopType::MountedKnight:
    return "mounted_knight";
  }
  return "";
}

inline TroopType troopTypeFromString(const std::string &str) {
  if (str == "archer")
    return TroopType::Archer;
  if (str == "knight")
    return TroopType::Knight;
  if (str == "spearman")
    return TroopType::Spearman;
  if (str == "mounted_knight")
    return TroopType::MountedKnight;
  return TroopType::Archer;
}

inline std::optional<TroopType> tryParseTroopType(const std::string &str) {
  if (str == "archer")
    return TroopType::Archer;
  if (str == "knight")
    return TroopType::Knight;
  if (str == "spearman")
    return TroopType::Spearman;
  if (str == "mounted_knight")
    return TroopType::MountedKnight;
  return std::nullopt;
}

} // namespace Units
} // namespace Game

namespace std {
template <> struct hash<Game::Units::TroopType> {
  size_t operator()(Game::Units::TroopType type) const {
    return hash<int>()(static_cast<int>(type));
  }
};
} // namespace std
