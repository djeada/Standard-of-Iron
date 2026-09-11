#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <functional>
#include <string>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Map {
struct MapDefinition;
}

namespace Game::Session {

inline constexpr int k_session_snapshot_version = 1;

struct SnapshotScope {
  Engine::Core::World* world = nullptr;
  const Game::Map::MapDefinition* map = nullptr;
};

using SnapshotCapture = std::function<QJsonValue(const SnapshotScope&)>;
using SnapshotRestore = std::function<void(const SnapshotScope&, const QJsonValue&)>;

struct SnapshotContributor {
  std::string key;
  SnapshotCapture capture;
  SnapshotRestore restore;
};

struct SnapshotRestoreReport {
  int version = 0;
  std::vector<std::string> restored;
  std::vector<std::string> missing_from_save;
  std::vector<std::string> unclaimed_in_save;

  [[nodiscard]] auto complete() const -> bool {
    return missing_from_save.empty() && unclaimed_in_save.empty();
  }
};

class SessionSnapshot {
public:
  static void register_contributor(SnapshotContributor contributor);

  static void forget_contributor(const std::string& key);

  [[nodiscard]] static auto contributor_keys() -> std::vector<std::string>;

  [[nodiscard]] static auto capture(const SnapshotScope& scope) -> QJsonObject;

  static auto restore(const SnapshotScope& scope,
                      const QJsonObject& snapshot) -> SnapshotRestoreReport;
};

void register_built_in_snapshot_contributors();

} // namespace Game::Session
