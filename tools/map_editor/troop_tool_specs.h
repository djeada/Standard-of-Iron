#pragma once

#include <QString>

#include <array>

#include "tool_panel.h"

namespace MapEditor {

struct TroopToolSpec {
  ToolType tool;
  const char* type;
  const char* name;
  const char* description;
  const char* section;
};

inline constexpr std::array<TroopToolSpec, 21> k_troop_tool_specs{{
    {ToolType::TroopArcher,
     "archer",
     "Archer",
     "Place archer troop spawns.",
     "Infantry & Support"},
    {ToolType::TroopSwordsman,
     "swordsman",
     "Swordsman",
     "Place swordsman troop spawns.",
     "Infantry & Support"},
    {ToolType::TroopSpearman,
     "spearman",
     "Spearman",
     "Place spearman troop spawns.",
     "Infantry & Support"},
    {ToolType::TroopHealer,
     "healer",
     "Healer",
     "Place healer troop spawns.",
     "Infantry & Support"},
    {ToolType::TroopCivilian,
     "civilian",
     "Civilian",
     "Place civilian spawns.",
     "Infantry & Support"},
    {ToolType::TroopBuilder,
     "builder",
     "Builder",
     "Place builder spawns.",
     "Infantry & Support"},
    {ToolType::TroopHorseSwordsman,
     "horse_swordsman",
     "Horse Swordsman",
     "Place mounted swordsman troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopHorseArcher,
     "horse_archer",
     "Horse Archer",
     "Place mounted archer troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopHorseSpearman,
     "horse_spearman",
     "Horse Spearman",
     "Place mounted spearman troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopElephant,
     "elephant",
     "Elephant",
     "Place elephant troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopCatapult,
     "catapult",
     "Catapult",
     "Place catapult troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopBallista,
     "ballista",
     "Ballista",
     "Place ballista troop spawns.",
     "Mounted & Siege"},
    {ToolType::TroopRomanLegionOrganizer,
     "roman_legion_organizer",
     "Fabius Maximus",
     "Place Quintus Fabius Maximus (Roman legion organizer) spawns.",
     "Command"},
    {ToolType::TroopRomanVeteranConsul,
     "roman_veteran_consul",
     "Scipio Africanus",
     "Place Publius Cornelius Scipio (Roman veteran consul) spawns.",
     "Command"},
    {ToolType::TroopRomanFieldCommander,
     "roman_field_commander",
     "Marcellus",
     "Place Marcus Claudius Marcellus (Roman field commander) spawns.",
     "Command"},
    {ToolType::TroopCarthageMercenaryBroker,
     "carthage_mercenary_broker",
     "Hanno the Great",
     "Place Hanno the Great (Carthaginian mercenary broker) spawns.",
     "Command"},
    {ToolType::TroopCarthageCavalryPatron,
     "carthage_cavalry_patron",
     "Hasdrubal Barca",
     "Place Hasdrubal Barca (Carthaginian cavalry patron) spawns.",
     "Command"},
    {ToolType::TroopCarthageElephantMaster,
     "carthage_elephant_master",
     "Hannibal Barca",
     "Place Hannibal Barca (Carthaginian elephant master) spawns.",
     "Command"},
    {ToolType::TroopSkeletonSwordsman,
     "skeleton_swordsman",
     "Skeleton Swordsman",
     "Place Iron Sepulcher skeleton swordsman spawns.",
     "Sepulcher"},
    {ToolType::TroopSkeletonArcher,
     "skeleton_archer",
     "Skeleton Archer",
     "Place Iron Sepulcher skeleton archer spawns.",
     "Sepulcher"},
    {ToolType::TroopGravePriest,
     "grave_priest",
     "Grave Priest",
     "Place Iron Sepulcher grave priest spawns.",
     "Sepulcher"},
}};

[[nodiscard]] inline auto troop_tool_spec(ToolType tool) -> const TroopToolSpec* {
  for (const auto& spec : k_troop_tool_specs) {
    if (spec.tool == tool) {
      return &spec;
    }
  }
  return nullptr;
}

[[nodiscard]] inline auto troop_type_for_tool(ToolType tool) -> QString {
  if (const auto* spec = troop_tool_spec(tool)) {
    return QString::fromLatin1(spec->type);
  }
  return {};
}

[[nodiscard]] inline auto troop_name_for_tool(ToolType tool) -> QString {
  if (const auto* spec = troop_tool_spec(tool)) {
    return QString::fromLatin1(spec->name);
  }
  return {};
}

[[nodiscard]] inline auto is_troop_tool(ToolType tool) -> bool {
  return troop_tool_spec(tool) != nullptr;
}

} // namespace MapEditor
