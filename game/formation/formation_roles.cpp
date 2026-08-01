#include "formation_roles.h"

#include <array>
#include <utility>

namespace Game::Formation {

namespace {

struct RoleTagName {
  RoleTag tag;
  const char* name;
};

constexpr std::array<RoleTagName, 18> k_role_tag_names = {{
    {RoleTag::LineInfantry, "line_infantry"},
    {RoleTag::HeavyInfantry, "heavy_infantry"},
    {RoleTag::SpearInfantry, "spear_infantry"},
    {RoleTag::Ranged, "ranged"},
    {RoleTag::Skirmisher, "skirmisher"},
    {RoleTag::Cavalry, "cavalry"},
    {RoleTag::Elephant, "elephant"},
    {RoleTag::Siege, "siege"},
    {RoleTag::Command, "command"},
    {RoleTag::Support, "support"},
    {RoleTag::Worker, "worker"},
    {RoleTag::Civilian, "civilian"},
    {RoleTag::Caster, "caster"},
    {RoleTag::Shielded, "shielded"},
    {RoleTag::Mounted, "mounted"},
    {RoleTag::Expendable, "expendable"},
    {RoleTag::Awakened, "awakened"},
    {RoleTag::Elite, "elite"},
}};

struct ArmyRoleName {
  ArmyRole role;
  const char* name;
};

constexpr std::array<ArmyRoleName, 9> k_army_role_names = {{
    {ArmyRole::Centre, "centre"},
    {ArmyRole::Vanguard, "vanguard"},
    {ArmyRole::LeftFlank, "left_flank"},
    {ArmyRole::RightFlank, "right_flank"},
    {ArmyRole::Ranged, "ranged"},
    {ArmyRole::Siege, "siege"},
    {ArmyRole::Command, "command"},
    {ArmyRole::Reserve, "reserve"},
    {ArmyRole::Screen, "screen"},
}};

} // namespace

auto role_tag_to_string(RoleTag tag) -> const char* {
  for (const auto& entry : k_role_tag_names) {
    if (entry.tag == tag) {
      return entry.name;
    }
  }
  return "line_infantry";
}

auto try_parse_role_tag(const QString& value) -> std::optional<RoleTag> {
  const QString lowered = value.trimmed().toLower();
  for (const auto& entry : k_role_tag_names) {
    if (lowered == QLatin1String(entry.name)) {
      return entry.tag;
    }
  }
  return std::nullopt;
}

auto parse_role_tag_set(const std::vector<std::string>& tags) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (const auto& tag : tags) {
    if (auto parsed = try_parse_role_tag(QString::fromStdString(tag))) {
      set |= to_mask(*parsed);
    }
  }
  return set;
}

auto role_tag_set_to_strings(RoleTagSet set) -> std::vector<std::string> {
  std::vector<std::string> out;
  for (const auto& entry : k_role_tag_names) {
    if (has_role(set, entry.tag)) {
      out.emplace_back(entry.name);
    }
  }
  return out;
}

auto army_role_to_string(ArmyRole role) -> const char* {
  for (const auto& entry : k_army_role_names) {
    if (entry.role == role) {
      return entry.name;
    }
  }
  return "centre";
}

auto try_parse_army_role(const QString& value) -> std::optional<ArmyRole> {
  const QString lowered = value.trimmed().toLower();
  for (const auto& entry : k_army_role_names) {
    if (lowered == QLatin1String(entry.name)) {
      return entry.role;
    }
  }
  return std::nullopt;
}

} // namespace Game::Formation
