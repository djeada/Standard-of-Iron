#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "troop_type.h"

namespace Game::Units {

enum class SpawnType : std::uint8_t {
  Archer,
  Knight,
  Spearman,
  SkeletonSwordsman,
  SkeletonArcher,
  GravePriest,
  MountedKnight,
  HorseArcher,
  HorseSpearman,
  Healer,
  Catapult,
  Ballista,
  Elephant,
  RomanLegionOrganizer,
  RomanVeteranConsul,
  RomanFieldCommander,
  CarthageSpearCommander,
  CarthageBowCommander,
  CarthageSwordCommander,
  Civilian,
  Builder,
  Barracks,
  DefenseTower,
  Home,
  WallSegment,
  Marketplace,
  WallGate,
  Temple,
  Sheep,
  Wolf,
  Farm
};

inline auto spawn_typeToQString(SpawnType type) -> QString {
  switch (type) {
  case SpawnType::Archer:
    return QStringLiteral("archer");
  case SpawnType::Knight:
    return QStringLiteral("swordsman");
  case SpawnType::Spearman:
    return QStringLiteral("spearman");
  case SpawnType::SkeletonSwordsman:
    return QStringLiteral("skeleton_swordsman");
  case SpawnType::SkeletonArcher:
    return QStringLiteral("skeleton_archer");
  case SpawnType::GravePriest:
    return QStringLiteral("grave_priest");
  case SpawnType::MountedKnight:
    return QStringLiteral("horse_swordsman");
  case SpawnType::HorseArcher:
    return QStringLiteral("horse_archer");
  case SpawnType::HorseSpearman:
    return QStringLiteral("horse_spearman");
  case SpawnType::Healer:
    return QStringLiteral("healer");
  case SpawnType::Catapult:
    return QStringLiteral("catapult");
  case SpawnType::Ballista:
    return QStringLiteral("ballista");
  case SpawnType::Elephant:
    return QStringLiteral("elephant");
  case SpawnType::RomanLegionOrganizer:
    return QStringLiteral("roman_legion_organizer");
  case SpawnType::RomanVeteranConsul:
    return QStringLiteral("roman_veteran_consul");
  case SpawnType::RomanFieldCommander:
    return QStringLiteral("roman_field_commander");
  case SpawnType::CarthageSpearCommander:
    return QStringLiteral("carthage_spear_commander");
  case SpawnType::CarthageBowCommander:
    return QStringLiteral("carthage_bow_commander");
  case SpawnType::CarthageSwordCommander:
    return QStringLiteral("carthage_sword_commander");
  case SpawnType::Civilian:
    return QStringLiteral("civilian");
  case SpawnType::Builder:
    return QStringLiteral("builder");
  case SpawnType::Barracks:
    return QStringLiteral("barracks");
  case SpawnType::DefenseTower:
    return QStringLiteral("defense_tower");
  case SpawnType::Home:
    return QStringLiteral("home");
  case SpawnType::WallSegment:
    return QStringLiteral("wall_segment");
  case SpawnType::Marketplace:
    return QStringLiteral("marketplace");
  case SpawnType::WallGate:
    return QStringLiteral("wall_gate");
  case SpawnType::Temple:
    return QStringLiteral("temple");
  case SpawnType::Sheep:
    return QStringLiteral("sheep");
  case SpawnType::Wolf:
    return QStringLiteral("wolf");
  case SpawnType::Farm:
    return QStringLiteral("farm");
  }
  return QStringLiteral("archer");
}

inline auto spawn_typeToString(SpawnType type) -> std::string {
  return spawn_typeToQString(type).toStdString();
}

inline auto try_parse_spawn_type(const QString& value, SpawnType& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("archer")) {
    out = SpawnType::Archer;
    return true;
  }
  if (lowered == QStringLiteral("swordsman")) {
    out = SpawnType::Knight;
    return true;
  }
  if (lowered == QStringLiteral("spearman")) {
    out = SpawnType::Spearman;
    return true;
  }
  if (lowered == QStringLiteral("skeleton_swordsman")) {
    out = SpawnType::SkeletonSwordsman;
    return true;
  }
  if (lowered == QStringLiteral("skeleton_archer")) {
    out = SpawnType::SkeletonArcher;
    return true;
  }
  if (lowered == QStringLiteral("grave_priest")) {
    out = SpawnType::GravePriest;
    return true;
  }
  if (lowered == QStringLiteral("horse_swordsman")) {
    out = SpawnType::MountedKnight;
    return true;
  }
  if (lowered == QStringLiteral("horse_archer")) {
    out = SpawnType::HorseArcher;
    return true;
  }
  if (lowered == QStringLiteral("horse_spearman")) {
    out = SpawnType::HorseSpearman;
    return true;
  }
  if (lowered == QStringLiteral("healer")) {
    out = SpawnType::Healer;
    return true;
  }
  if (lowered == QStringLiteral("catapult")) {
    out = SpawnType::Catapult;
    return true;
  }
  if (lowered == QStringLiteral("ballista")) {
    out = SpawnType::Ballista;
    return true;
  }
  if (lowered == QStringLiteral("elephant")) {
    out = SpawnType::Elephant;
    return true;
  }
  if (lowered == QStringLiteral("roman_legion_organizer")) {
    out = SpawnType::RomanLegionOrganizer;
    return true;
  }
  if (lowered == QStringLiteral("roman_veteran_consul")) {
    out = SpawnType::RomanVeteranConsul;
    return true;
  }
  if (lowered == QStringLiteral("roman_field_commander")) {
    out = SpawnType::RomanFieldCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_spear_commander")) {
    out = SpawnType::CarthageSpearCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_bow_commander")) {
    out = SpawnType::CarthageBowCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_sword_commander")) {
    out = SpawnType::CarthageSwordCommander;
    return true;
  }
  if (lowered == QStringLiteral("civilian")) {
    out = SpawnType::Civilian;
    return true;
  }
  if (lowered == QStringLiteral("builder")) {
    out = SpawnType::Builder;
    return true;
  }
  if (lowered == QStringLiteral("sheep")) {
    out = SpawnType::Sheep;
    return true;
  }
  if (lowered == QStringLiteral("wolf")) {
    out = SpawnType::Wolf;
    return true;
  }
  if (lowered == QStringLiteral("barracks")) {
    out = SpawnType::Barracks;
    return true;
  }
  if (lowered == QStringLiteral("village")) {
    out = SpawnType::Barracks;
    return true;
  }
  if (lowered == QStringLiteral("defense_tower")) {
    out = SpawnType::DefenseTower;
    return true;
  }
  if (lowered == QStringLiteral("home")) {
    out = SpawnType::Home;
    return true;
  }
  if (lowered == QStringLiteral("wall_segment")) {
    out = SpawnType::WallSegment;
    return true;
  }
  if (lowered == QStringLiteral("marketplace")) {
    out = SpawnType::Marketplace;
    return true;
  }
  if (lowered == QStringLiteral("wall_gate")) {
    out = SpawnType::WallGate;
    return true;
  }
  if (lowered == QStringLiteral("temple")) {
    out = SpawnType::Temple;
    return true;
  }
  if (lowered == QStringLiteral("farm")) {
    out = SpawnType::Farm;
    return true;
  }
  return false;
}

inline auto spawn_typeFromString(const std::string& str) -> std::optional<SpawnType> {
  if (str == "archer") {
    return SpawnType::Archer;
  }
  if (str == "swordsman") {
    return SpawnType::Knight;
  }
  if (str == "spearman") {
    return SpawnType::Spearman;
  }
  if (str == "skeleton_swordsman") {
    return SpawnType::SkeletonSwordsman;
  }
  if (str == "skeleton_archer") {
    return SpawnType::SkeletonArcher;
  }
  if (str == "grave_priest") {
    return SpawnType::GravePriest;
  }
  if (str == "horse_swordsman") {
    return SpawnType::MountedKnight;
  }
  if (str == "horse_archer") {
    return SpawnType::HorseArcher;
  }
  if (str == "horse_spearman") {
    return SpawnType::HorseSpearman;
  }
  if (str == "healer") {
    return SpawnType::Healer;
  }
  if (str == "catapult") {
    return SpawnType::Catapult;
  }
  if (str == "ballista") {
    return SpawnType::Ballista;
  }
  if (str == "elephant") {
    return SpawnType::Elephant;
  }
  if (str == "roman_legion_organizer") {
    return SpawnType::RomanLegionOrganizer;
  }
  if (str == "roman_veteran_consul") {
    return SpawnType::RomanVeteranConsul;
  }
  if (str == "roman_field_commander") {
    return SpawnType::RomanFieldCommander;
  }
  if (str == "carthage_spear_commander") {
    return SpawnType::CarthageSpearCommander;
  }
  if (str == "carthage_bow_commander") {
    return SpawnType::CarthageBowCommander;
  }
  if (str == "carthage_sword_commander") {
    return SpawnType::CarthageSwordCommander;
  }
  if (str == "civilian") {
    return SpawnType::Civilian;
  }
  if (str == "builder") {
    return SpawnType::Builder;
  }
  if (str == "barracks") {
    return SpawnType::Barracks;
  }
  if (str == "village") {
    return SpawnType::Barracks;
  }
  if (str == "defense_tower") {
    return SpawnType::DefenseTower;
  }
  if (str == "home") {
    return SpawnType::Home;
  }
  if (str == "wall_segment") {
    return SpawnType::WallSegment;
  }
  if (str == "marketplace") {
    return SpawnType::Marketplace;
  }
  if (str == "wall_gate") {
    return SpawnType::WallGate;
  }
  if (str == "temple") {
    return SpawnType::Temple;
  }
  if (str == "sheep") {
    return SpawnType::Sheep;
  }
  if (str == "wolf") {
    return SpawnType::Wolf;
  }
  if (str == "farm") {
    return SpawnType::Farm;
  }
  return std::nullopt;
}

[[nodiscard]] inline auto is_wildlife_spawn(SpawnType type) noexcept -> bool {
  return type == SpawnType::Sheep || type == SpawnType::Wolf;
}

inline auto is_building_spawn(SpawnType type) -> bool {
  return type == SpawnType::Barracks || type == SpawnType::DefenseTower ||
         type == SpawnType::Home || type == SpawnType::WallSegment ||
         type == SpawnType::Marketplace || type == SpawnType::WallGate ||
         type == SpawnType::Temple || type == SpawnType::Farm;
}

inline auto is_troop_spawn(SpawnType type) -> bool {
  return !is_building_spawn(type) && !is_wildlife_spawn(type);
}

[[nodiscard]] inline auto is_recruitment_building(SpawnType type) noexcept -> bool {
  return type == SpawnType::Barracks || type == SpawnType::Temple;
}

inline auto is_wall_network_spawn(SpawnType type) -> bool {
  return type == SpawnType::WallSegment || type == SpawnType::WallGate;
}

[[nodiscard]] inline auto is_cavalry(SpawnType type) noexcept -> bool {
  return type == SpawnType::MountedKnight || type == SpawnType::HorseArcher ||
         type == SpawnType::HorseSpearman;
}

[[nodiscard]] inline auto can_enter_forest(SpawnType type) noexcept -> bool {
  switch (type) {
  case SpawnType::Archer:
  case SpawnType::Knight:
  case SpawnType::Healer:
  case SpawnType::Builder:
  case SpawnType::Civilian:
  case SpawnType::SkeletonSwordsman:
  case SpawnType::SkeletonArcher:
  case SpawnType::GravePriest:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageSpearCommander:
  case SpawnType::CarthageBowCommander:
  case SpawnType::CarthageSwordCommander:
  case SpawnType::Sheep:
  case SpawnType::Wolf:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] inline auto body_turn_speed_degrees(SpawnType type) noexcept -> float {
  switch (type) {
  case SpawnType::Elephant:
    return 110.0F;
  case SpawnType::Catapult:
  case SpawnType::Ballista:
    return 100.0F;
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
    return 300.0F;
  case SpawnType::Sheep:
    return 210.0F;
  case SpawnType::Wolf:
    return 340.0F;
  default:
    return 720.0F;
  }
}

[[nodiscard]] inline auto body_acceleration(SpawnType type) noexcept -> float {
  switch (type) {
  case SpawnType::Elephant:
    return 1.2F;
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
    return 5.0F;
  case SpawnType::Sheep:
    return 6.0F;
  case SpawnType::Wolf:
    return 8.0F;
  default:
    return 0.0F;
  }
}

inline auto can_use_attack_mode(SpawnType type) -> bool {
  return type != SpawnType::Healer && type != SpawnType::Builder &&
         !is_building_spawn(type) && !is_wildlife_spawn(type);
}

inline auto can_use_guard_mode(SpawnType type) -> bool {
  return !is_building_spawn(type) && !is_wildlife_spawn(type);
}

inline auto can_use_hold_mode(SpawnType type) -> bool {
  return type == SpawnType::Archer || type == SpawnType::Spearman;
}

inline auto can_use_patrol_mode(SpawnType type) -> bool {
  return !is_building_spawn(type) && !is_wildlife_spawn(type);
}

[[nodiscard]] inline auto can_use_run_mode(SpawnType type) noexcept -> bool {
  switch (type) {
  case SpawnType::Archer:
  case SpawnType::Knight:
  case SpawnType::Spearman:
  case SpawnType::Healer:
  case SpawnType::Civilian:
  case SpawnType::Builder:
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageSpearCommander:
  case SpawnType::CarthageBowCommander:
  case SpawnType::CarthageSwordCommander:
    return true;
  case SpawnType::SkeletonSwordsman:
  case SpawnType::SkeletonArcher:
  case SpawnType::GravePriest:
  case SpawnType::Catapult:
  case SpawnType::Ballista:
  case SpawnType::Barracks:
  case SpawnType::DefenseTower:
  case SpawnType::Home:
  case SpawnType::WallSegment:
  case SpawnType::Marketplace:
  case SpawnType::WallGate:
  case SpawnType::Temple:
  case SpawnType::Farm:
  case SpawnType::Sheep:
  case SpawnType::Wolf:
  case SpawnType::Elephant:
    return false;
  }
  return false;
}

inline auto spawn_typeToTroopType(SpawnType type) -> std::optional<TroopType> {
  switch (type) {
  case SpawnType::Archer:
    return TroopType::Archer;
  case SpawnType::Knight:
    return TroopType::Swordsman;
  case SpawnType::Spearman:
    return TroopType::Spearman;
  case SpawnType::SkeletonSwordsman:
    return TroopType::SkeletonSwordsman;
  case SpawnType::SkeletonArcher:
    return TroopType::SkeletonArcher;
  case SpawnType::GravePriest:
    return TroopType::GravePriest;
  case SpawnType::MountedKnight:
    return TroopType::MountedKnight;
  case SpawnType::HorseArcher:
    return TroopType::HorseArcher;
  case SpawnType::HorseSpearman:
    return TroopType::HorseSpearman;
  case SpawnType::Healer:
    return TroopType::Healer;
  case SpawnType::Catapult:
    return TroopType::Catapult;
  case SpawnType::Ballista:
    return TroopType::Ballista;
  case SpawnType::Elephant:
    return TroopType::Elephant;
  case SpawnType::RomanLegionOrganizer:
    return TroopType::RomanLegionOrganizer;
  case SpawnType::RomanVeteranConsul:
    return TroopType::RomanVeteranConsul;
  case SpawnType::RomanFieldCommander:
    return TroopType::RomanFieldCommander;
  case SpawnType::CarthageSpearCommander:
    return TroopType::CarthageSpearCommander;
  case SpawnType::CarthageBowCommander:
    return TroopType::CarthageBowCommander;
  case SpawnType::CarthageSwordCommander:
    return TroopType::CarthageSwordCommander;
  case SpawnType::Civilian:
    return TroopType::Civilian;
  case SpawnType::Builder:
    return TroopType::Builder;
  case SpawnType::Barracks:
    return std::nullopt;
  case SpawnType::DefenseTower:
    return std::nullopt;
  case SpawnType::Home:
    return std::nullopt;
  case SpawnType::WallSegment:
    return std::nullopt;
  case SpawnType::Marketplace:
    return std::nullopt;
  case SpawnType::WallGate:
    return std::nullopt;
  case SpawnType::Temple:
    return std::nullopt;
  case SpawnType::Farm:
    return std::nullopt;
  case SpawnType::Sheep:
    return TroopType::Sheep;
  case SpawnType::Wolf:
    return TroopType::Wolf;
  }
  return std::nullopt;
}

inline auto spawn_typeFromTroopType(TroopType type) -> SpawnType {
  switch (type) {
  case TroopType::Archer:
    return SpawnType::Archer;
  case TroopType::Swordsman:
    return SpawnType::Knight;
  case TroopType::Spearman:
    return SpawnType::Spearman;
  case TroopType::SkeletonSwordsman:
    return SpawnType::SkeletonSwordsman;
  case TroopType::SkeletonArcher:
    return SpawnType::SkeletonArcher;
  case TroopType::GravePriest:
    return SpawnType::GravePriest;
  case TroopType::MountedKnight:
    return SpawnType::MountedKnight;
  case TroopType::HorseArcher:
    return SpawnType::HorseArcher;
  case TroopType::HorseSpearman:
    return SpawnType::HorseSpearman;
  case TroopType::Healer:
    return SpawnType::Healer;
  case TroopType::Catapult:
    return SpawnType::Catapult;
  case TroopType::Ballista:
    return SpawnType::Ballista;
  case TroopType::Elephant:
    return SpawnType::Elephant;
  case TroopType::RomanLegionOrganizer:
    return SpawnType::RomanLegionOrganizer;
  case TroopType::RomanVeteranConsul:
    return SpawnType::RomanVeteranConsul;
  case TroopType::RomanFieldCommander:
    return SpawnType::RomanFieldCommander;
  case TroopType::CarthageSpearCommander:
    return SpawnType::CarthageSpearCommander;
  case TroopType::CarthageBowCommander:
    return SpawnType::CarthageBowCommander;
  case TroopType::CarthageSwordCommander:
    return SpawnType::CarthageSwordCommander;
  case TroopType::Civilian:
    return SpawnType::Civilian;
  case TroopType::Builder:
    return SpawnType::Builder;
  case TroopType::Sheep:
    return SpawnType::Sheep;
  case TroopType::Wolf:
    return SpawnType::Wolf;
  }
  return SpawnType::Archer;
}

} // namespace Game::Units

namespace std {
template <>
struct hash<Game::Units::SpawnType> {
  auto operator()(Game::Units::SpawnType type) const noexcept -> size_t {
    return hash<std::uint8_t>()(static_cast<std::uint8_t>(type));
  }
};
} // namespace std
