#include <QRegularExpression>
#include <qvariant.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "selected_units_model.h"

namespace App::Models {

auto group_selection_by_type(const QVariantList& units) -> std::vector<SelectionGroup> {
  std::vector<SelectionGroup> groups;

  std::vector<double> health_sums;
  std::vector<double> stamina_sums;

  for (const QVariant& entry : units) {
    const QVariantMap unit = entry.toMap();
    QString type_key = unit.value(QStringLiteral("unit_type")).toString();
    const QString name = unit.value(QStringLiteral("name")).toString();
    if (type_key.isEmpty()) {
      type_key = name.trimmed().toLower().replace(QRegularExpression("[^a-z0-9]+"),
                                                  QStringLiteral("_"));
    }
    if (type_key.isEmpty()) {
      continue;
    }

    const double health =
        std::clamp(unit.value(QStringLiteral("health_ratio")).toDouble(), 0.0, 1.0);
    const double stamina =
        std::clamp(unit.value(QStringLiteral("stamina_ratio")).toDouble(), 0.0, 1.0);

    auto match = std::find_if(
        groups.begin(), groups.end(), [&type_key](const SelectionGroup& group) {
          return group.type_key == type_key;
        });
    if (match == groups.end()) {
      SelectionGroup group;
      group.type_key = type_key;
      group.name = name.isEmpty() ? type_key : name;
      group.nation = unit.value(QStringLiteral("nation")).toString();
      groups.push_back(group);
      health_sums.push_back(0.0);
      stamina_sums.push_back(0.0);
      match = std::prev(groups.end());
    }

    const auto offset = static_cast<std::size_t>(std::distance(groups.begin(), match));
    match->count += 1;
    health_sums[offset] += health;
    stamina_sums[offset] += stamina;

    if (health < 1.0) {
      match->wounded_count += 1;
    }
  }

  for (std::size_t i = 0; i < groups.size(); ++i) {
    groups[i].health = groups[i].count > 0
                           ? health_sums[i] / static_cast<double>(groups[i].count)
                           : 0.0;
    groups[i].stamina = groups[i].count > 0
                            ? stamina_sums[i] / static_cast<double>(groups[i].count)
                            : 1.0;
  }
  return groups;
}

auto selection_groups_to_variant(const std::vector<SelectionGroup>& groups)
    -> QVariantList {
  QVariantList result;
  result.reserve(static_cast<int>(groups.size()));
  for (const SelectionGroup& group : groups) {
    QVariantMap entry;
    entry[QStringLiteral("typeKey")] = group.type_key;
    entry[QStringLiteral("name")] = group.name;
    entry[QStringLiteral("nation")] = group.nation;
    entry[QStringLiteral("count")] = group.count;
    entry[QStringLiteral("woundedCount")] = group.wounded_count;
    entry[QStringLiteral("health")] = group.health;
    entry[QStringLiteral("stamina")] = group.stamina;
    result.append(entry);
  }
  return result;
}

} // namespace App::Models
