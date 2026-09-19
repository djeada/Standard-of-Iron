#include "resource_types.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>

namespace Game::Systems {

auto resource_display_name(ResourceType type) -> QString {
  switch (type) {
  case ResourceType::Gold:
    return QCoreApplication::translate("Resources", "Gold");
  case ResourceType::Food:
    return QCoreApplication::translate("Resources", "Food");
  case ResourceType::Wood:
    return QCoreApplication::translate("Resources", "Timber");
  case ResourceType::Stone:
    return QCoreApplication::translate("Resources", "Stone");
  case ResourceType::Iron:
    return QCoreApplication::translate("Resources", "Iron");
  case ResourceType::Count:
    break;
  }
  return {};
}

auto resource_tally(const ResourceAmounts& carried,
                    const ResourceAmounts& needed) -> ResourceTally {
  ResourceTally tally;
  QStringList parts;
  QStringList numbers;
  long long carried_total = 0;
  long long needed_total = 0;
  for (const auto type : k_all_resource_types) {
    const int required = needed.get(type);
    if (required <= 0) {
      continue;
    }
    ++tally.kinds;
    const int have = carried.get(type);
    if (have >= required) {
      ++tally.met;
    }
    const int shown = std::clamp(have, 0, required);
    parts.append(QCoreApplication::translate("Resources", "%1 %2/%3")
                     .arg(resource_display_name(type))
                     .arg(shown)
                     .arg(required));
    numbers.append(QStringLiteral("%1/%2").arg(shown).arg(required));
    carried_total += shown;
    needed_total += required;
  }
  tally.text = parts.join(QStringLiteral(" · "));
  tally.numbers = numbers.join(QStringLiteral(" · "));
  tally.fraction = needed_total > 0 ? static_cast<double>(carried_total) /
                                          static_cast<double>(needed_total)
                                    : 0.0;
  return tally;
}

} // namespace Game::Systems
