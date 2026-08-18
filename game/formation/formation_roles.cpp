#include "formation_roles.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1String>

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

struct RoleTagLabel {
  RoleTag tag;
  const char* label;
};

constexpr std::array<RoleTagLabel, 18> k_role_tag_labels = {{
    {RoleTag::LineInfantry, QT_TRANSLATE_NOOP("Units", "Line infantry")},
    {RoleTag::HeavyInfantry, QT_TRANSLATE_NOOP("Units", "Heavy infantry")},
    {RoleTag::SpearInfantry, QT_TRANSLATE_NOOP("Units", "Spear infantry")},
    {RoleTag::Ranged, QT_TRANSLATE_NOOP("Units", "Ranged")},
    {RoleTag::Skirmisher, QT_TRANSLATE_NOOP("Units", "Skirmisher")},
    {RoleTag::Cavalry, QT_TRANSLATE_NOOP("Units", "Cavalry")},
    {RoleTag::Elephant, QT_TRANSLATE_NOOP("Units", "War beast")},
    {RoleTag::Siege, QT_TRANSLATE_NOOP("Units", "Siege")},
    {RoleTag::Command, QT_TRANSLATE_NOOP("Units", "Command")},
    {RoleTag::Support, QT_TRANSLATE_NOOP("Units", "Support")},
    {RoleTag::Worker, QT_TRANSLATE_NOOP("Units", "Worker")},
    {RoleTag::Civilian, QT_TRANSLATE_NOOP("Units", "Civilian")},
    {RoleTag::Caster, QT_TRANSLATE_NOOP("Units", "Caster")},
    {RoleTag::Shielded, QT_TRANSLATE_NOOP("Units", "Shielded")},
    {RoleTag::Mounted, QT_TRANSLATE_NOOP("Units", "Mounted")},
    {RoleTag::Expendable, QT_TRANSLATE_NOOP("Units", "Expendable")},
    {RoleTag::Awakened, QT_TRANSLATE_NOOP("Units", "Awakened")},
    {RoleTag::Elite, QT_TRANSLATE_NOOP("Units", "Elite")},
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

auto role_tag_label(RoleTag tag) -> QString {
  for (const auto& entry : k_role_tag_labels) {
    if (entry.tag == tag) {
      return QCoreApplication::translate("Units", entry.label);
    }
  }
  return QCoreApplication::translate("Units", "Line infantry");
}

auto all_role_tags() -> std::vector<RoleTag> {
  std::vector<RoleTag> tags;
  tags.reserve(k_role_tag_names.size());
  for (const auto& entry : k_role_tag_names) {
    tags.push_back(entry.tag);
  }
  return tags;
}

auto role_tag_set_to_labels(RoleTagSet set) -> QStringList {
  QStringList labels;
  for (const auto& entry : k_role_tag_labels) {
    if (has_role(set, entry.tag)) {
      labels.append(QCoreApplication::translate("Units", entry.label));
    }
  }
  return labels;
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

auto parse_troop_formation_profile(const QJsonObject& formation_object,
                                   TroopFormationProfile& out) -> bool {
  bool touched = false;

  std::vector<std::string> roles;
  for (const auto entry : formation_object.value(QLatin1String("roles")).toArray()) {
    auto const text = entry.toString().trimmed();
    if (!text.isEmpty()) {
      roles.push_back(text.toLower().toStdString());
    }
  }
  if (!roles.empty()) {
    out.roles = parse_role_tag_set(roles);
    touched = true;
  }

  auto const army_roles = formation_object.value(QLatin1String("army_roles")).toArray();
  if (!army_roles.isEmpty()) {
    out.army_roles.clear();
    for (const auto entry : army_roles) {
      if (auto parsed = try_parse_army_role(entry.toString())) {
        out.army_roles.push_back(*parsed);
      }
    }
    touched = true;
  }

  auto assign = [&](const char* key, std::string& target) {
    auto const value = formation_object.value(QLatin1String(key)).toString().trimmed();
    if (!value.isEmpty()) {
      target = value.toStdString();
      touched = true;
    }
  };
  assign("unit_layout", out.unit_layout);
  assign("defensive_layout", out.defensive_layout);
  assign("marching_layout", out.marching_layout);

  return touched;
}

} // namespace Game::Formation
