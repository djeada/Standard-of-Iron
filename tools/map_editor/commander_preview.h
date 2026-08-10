#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "map_data.h"

namespace MapEditor {

inline constexpr int k_local_owner_id = 1;
inline constexpr int k_first_ai_owner_id = 2;

struct DerivedCommander {
  int owner_id = k_local_owner_id;
  QString troop_type;
  QString suggested_troop_type;
  QString label;
  QPointF position;
  bool authored_in_map = false;
};

[[nodiscard]] auto resolve_commander_troop(const QString& nation,
                                           const QString& configured) -> QString;

[[nodiscard]] auto
derive_mission_commanders(const MapData& map,
                          const QJsonObject& mission_root) -> QVector<DerivedCommander>;

} // namespace MapEditor
