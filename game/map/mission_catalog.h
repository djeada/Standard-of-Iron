#pragma once

#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace Game::Map {

class MissionCatalog {
public:
  [[nodiscard]] static auto resolve_mission_file(const QString& mission_id) -> QString;

  [[nodiscard]] static auto campaign_mission_ids() -> QSet<QString>;

  [[nodiscard]] static auto standalone_missions() -> QVariantList;

  [[nodiscard]] static auto mission_map_paths() -> QSet<QString>;
};

} // namespace Game::Map
