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
    parts.append(QCoreApplication::translate("Resources", "%1 %2/%3")
                     .arg(resource_display_name(type))
                     .arg(std::min(have, required))
                     .arg(required));
  }
  tally.text = parts.join(QStringLiteral(" · "));
  return tally;
}

} // namespace Game::Systems
