#pragma once

#include "game/units/troop_type.h"

#include <QString>
#include <optional>

namespace Arena::UnitSpawnOptions {

enum class Kind { SingleHorse, SingleElephant };

struct SpecialUnitOption {
  Kind kind;
  QString id;
  QString label;
  Game::Units::TroopType troop_type;
  int individuals_per_unit = 1;
  bool render_rider = true;
};

inline auto
build_special_unit_id(Kind kind, Game::Units::TroopType troop_type) -> QString {
  QString const prefix = kind == Kind::SingleHorse
                             ? QStringLiteral("arena_single_horse:")
                             : QStringLiteral("arena_single_elephant:");
  return prefix + Game::Units::troop_typeToQString(troop_type);
}

inline auto make_special_unit_option(
    Kind kind, Game::Units::TroopType troop_type) -> SpecialUnitOption {
  SpecialUnitOption option;
  option.kind = kind;
  option.id = build_special_unit_id(kind, troop_type);
  option.troop_type = troop_type;
  option.individuals_per_unit = 1;
  option.render_rider = kind != Kind::SingleHorse;
  option.label = kind == Kind::SingleHorse ? QStringLiteral("Single Horse")
                                           : QStringLiteral("Single Elephant");
  return option;
}

inline auto parse_special_unit_option(const QString &value)
    -> std::optional<SpecialUnitOption> {
  struct PrefixEntry {
    QString prefix;
    Kind kind;
  };
  static const PrefixEntry k_prefixes[] = {
      {QStringLiteral("arena_single_horse:"), Kind::SingleHorse},
      {QStringLiteral("arena_single_elephant:"), Kind::SingleElephant},
  };

  for (const PrefixEntry &entry : k_prefixes) {
    if (!value.startsWith(entry.prefix)) {
      continue;
    }

    Game::Units::TroopType troop_type{};
    QString const troop_id = value.mid(entry.prefix.size());
    if (!Game::Units::try_parse_troop_type(troop_id, troop_type)) {
      return std::nullopt;
    }
    return make_special_unit_option(entry.kind, troop_type);
  }

  return std::nullopt;
}

} // namespace Arena::UnitSpawnOptions
