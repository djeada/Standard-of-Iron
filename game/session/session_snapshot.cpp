#include "session_snapshot.h"

#include <QJsonArray>

#include <algorithm>
#include <mutex>

#include "../core/world.h"
#include "../systems/cursed_gold_vein_system.h"
#include "../systems/undead_awakening_system.h"
#include "../wildlife/wildlife_system.h"

namespace Game::Session {

namespace {

struct Registry {
  std::mutex mutex;
  std::vector<SnapshotContributor> contributors;
};

auto registry() -> Registry& {
  static Registry state;
  return state;
}

template <typename System>
auto system_of(const SnapshotScope& scope) -> System* {
  return scope.world != nullptr ? scope.world->get_system<System>() : nullptr;
}

} // namespace

void SessionSnapshot::register_contributor(SnapshotContributor contributor) {
  if (contributor.key.empty()) {
    return;
  }
  auto& state = registry();
  const std::lock_guard<std::mutex> lock(state.mutex);
  const auto existing =
      std::find_if(state.contributors.begin(),
                   state.contributors.end(),
                   [&contributor](const SnapshotContributor& candidate) {
                     return candidate.key == contributor.key;
                   });
  if (existing != state.contributors.end()) {
    *existing = std::move(contributor);
    return;
  }
  state.contributors.push_back(std::move(contributor));
}

void SessionSnapshot::forget_contributor(const std::string& key) {
  auto& state = registry();
  const std::lock_guard<std::mutex> lock(state.mutex);
  state.contributors.erase(std::remove_if(state.contributors.begin(),
                                          state.contributors.end(),
                                          [&key](const SnapshotContributor& candidate) {
                                            return candidate.key == key;
                                          }),
                           state.contributors.end());
}

auto SessionSnapshot::contributor_keys() -> std::vector<std::string> {
  auto& state = registry();
  const std::lock_guard<std::mutex> lock(state.mutex);
  std::vector<std::string> keys;
  keys.reserve(state.contributors.size());
  for (const auto& contributor : state.contributors) {
    keys.push_back(contributor.key);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

auto SessionSnapshot::capture(const SnapshotScope& scope) -> QJsonObject {
  std::vector<SnapshotContributor> snapshot_of_registry;
  {
    auto& state = registry();
    const std::lock_guard<std::mutex> lock(state.mutex);
    snapshot_of_registry = state.contributors;
  }

  QJsonObject parts;
  for (const auto& contributor : snapshot_of_registry) {
    if (!contributor.capture) {
      continue;
    }
    const QJsonValue value = contributor.capture(scope);
    if (value.isUndefined() || value.isNull()) {
      continue;
    }
    parts[QString::fromStdString(contributor.key)] = value;
  }

  QJsonObject snapshot;
  snapshot[QStringLiteral("version")] = k_session_snapshot_version;
  snapshot[QStringLiteral("parts")] = parts;
  return snapshot;
}

auto SessionSnapshot::restore(const SnapshotScope& scope,
                              const QJsonObject& snapshot) -> SnapshotRestoreReport {
  SnapshotRestoreReport report;
  report.version = snapshot.value(QStringLiteral("version")).toInt(0);
  const QJsonObject parts = snapshot.value(QStringLiteral("parts")).toObject();

  std::vector<SnapshotContributor> snapshot_of_registry;
  {
    auto& state = registry();
    const std::lock_guard<std::mutex> lock(state.mutex);
    snapshot_of_registry = state.contributors;
  }

  std::vector<std::string> claimed;
  for (const auto& contributor : snapshot_of_registry) {
    const QString key = QString::fromStdString(contributor.key);
    if (!parts.contains(key)) {
      report.missing_from_save.push_back(contributor.key);
      continue;
    }
    claimed.push_back(contributor.key);
    if (contributor.restore) {
      contributor.restore(scope, parts.value(key));
      report.restored.push_back(contributor.key);
    }
  }

  for (const QString& key : parts.keys()) {
    const std::string name = key.toStdString();
    if (std::find(claimed.begin(), claimed.end(), name) == claimed.end()) {
      report.unclaimed_in_save.push_back(name);
    }
  }

  std::sort(report.restored.begin(), report.restored.end());
  std::sort(report.missing_from_save.begin(), report.missing_from_save.end());
  std::sort(report.unclaimed_in_save.begin(), report.unclaimed_in_save.end());
  return report;
}

void register_built_in_snapshot_contributors() {
  SessionSnapshot::register_contributor(
      {.key = "undead_zones",
       .capture = [](const SnapshotScope& scope) -> QJsonValue {
         auto* system = system_of<Game::Systems::UndeadAwakeningSystem>(scope);
         return system != nullptr ? QJsonValue(system->serialize_state())
                                  : QJsonValue();
       },
       .restore =
           [](const SnapshotScope& scope, const QJsonValue& value) {
             auto* system = system_of<Game::Systems::UndeadAwakeningSystem>(scope);
             if (system == nullptr) {
               return;
             }
             if (scope.map != nullptr) {
               system->configure(*scope.map);
             }
             system->restore_state(value.toArray());
           }});

  SessionSnapshot::register_contributor(
      {.key = "cursed_gold_veins",
       .capture = [](const SnapshotScope& scope) -> QJsonValue {
         auto* system = system_of<Game::Systems::CursedGoldVeinSystem>(scope);
         return system != nullptr ? QJsonValue(system->serialize_state())
                                  : QJsonValue();
       },
       .restore =
           [](const SnapshotScope& scope, const QJsonValue& value) {
             auto* system = system_of<Game::Systems::CursedGoldVeinSystem>(scope);
             if (system == nullptr) {
               return;
             }
             if (scope.map != nullptr) {
               system->configure(*scope.map);
             }
             system->restore_state(value.toArray());
           }});

  SessionSnapshot::register_contributor(
      {.key = "wildlife",
       .capture = [](const SnapshotScope& scope) -> QJsonValue {
         auto* system = system_of<Game::Wildlife::WildlifeSystem>(scope);
         return system != nullptr ? QJsonValue(system->serialize_state())
                                  : QJsonValue();
       },
       .restore =
           [](const SnapshotScope& scope, const QJsonValue& value) {
             auto* system = system_of<Game::Wildlife::WildlifeSystem>(scope);
             if (system == nullptr) {
               return;
             }
             if (scope.map != nullptr) {
               system->configure(*scope.map);
             }
             system->restore_state(value.toObject());
           }});
}

} // namespace Game::Session
