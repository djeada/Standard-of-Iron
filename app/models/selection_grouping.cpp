#include <QRegularExpression>
#include <qvariant.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "selected_units_model.h"

namespace App::Models {

auto group_selection_by_type(const QVariantList& units) -> std::vector<SelectionGroup> {
  std::vector<SelectionGroup> groups;

  std::vector<double> health_sums;
  std::vector<double> stamina_sums;
  std::vector<int> stamina_counts;
  // Per group: how many members report each activity+state pair.
  std::vector<std::map<std::pair<QString, QString>, int>> activity_tallies;

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
    const bool can_run = unit.value(QStringLiteral("can_run")).toBool();
    const QVariant stamina_value = unit.value(QStringLiteral("stamina_ratio"));
    const double stamina =
        stamina_value.isValid() ? std::clamp(stamina_value.toDouble(), 0.0, 1.0) : 1.0;

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
      stamina_counts.push_back(0);
      activity_tallies.emplace_back();
      match = std::prev(groups.end());
    }

    const auto offset = static_cast<std::size_t>(std::distance(groups.begin(), match));
    match->count += 1;
    health_sums[offset] += health;
    if (can_run) {
      match->can_run = true;
      stamina_sums[offset] += stamina;
      stamina_counts[offset] += 1;
    }

    if (health < 1.0) {
      match->wounded_count += 1;
    }

    const QString activity =
        unit.value(QStringLiteral("activity"), QStringLiteral("idle")).toString();
    const QString activity_state =
        unit.value(QStringLiteral("activity_state"), QStringLiteral("active"))
            .toString();
    activity_tallies[offset][{activity.isEmpty() ? QStringLiteral("idle") : activity,
                              activity_state.isEmpty() ? QStringLiteral("active")
                                                       : activity_state}] += 1;
  }

  for (std::size_t i = 0; i < groups.size(); ++i) {
    groups[i].health = groups[i].count > 0
                           ? health_sums[i] / static_cast<double>(groups[i].count)
                           : 0.0;
    groups[i].stamina = stamina_counts[i] > 0
                            ? stamina_sums[i] / static_cast<double>(stamina_counts[i])
                            : 1.0;

    const auto& tally = activity_tallies[i];
    auto dominant = tally.begin();
    for (auto it = tally.begin(); it != tally.end(); ++it) {
      if (it->second > dominant->second) {
        dominant = it;
      }
    }
    if (dominant != tally.end()) {
      groups[i].activity = dominant->first.first;
      groups[i].activity_state = dominant->first.second;
      groups[i].activity_count = dominant->second;
      groups[i].mixed_activity = tally.size() > 1;
    }
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
    entry[QStringLiteral("canRun")] = group.can_run;
    entry[QStringLiteral("activity")] = group.activity;
    entry[QStringLiteral("activityState")] = group.activity_state;
    entry[QStringLiteral("activityCount")] = group.activity_count;
    entry[QStringLiteral("mixedActivity")] = group.mixed_activity;
    result.append(entry);
  }
  return result;
}

} // namespace App::Models
