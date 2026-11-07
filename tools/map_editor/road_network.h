#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include "map_data.h"

namespace MapEditor {

inline constexpr double k_road_join_tolerance = 2.5;
inline constexpr double k_road_snap_reach = 10.0;

struct RoadNearMiss {
  int element = -1;
  bool at_start = true;
  QPointF end;
  QPointF target;
  int target_element = -1;
  double gap = 0.0;
};

struct RoadNetworkReport {
  int road_count = 0;
  int connector_count = 0;
  QVector<QVector<int>> groups;
  QVector<RoadNearMiss> near_misses;

  [[nodiscard]] auto connected() const -> bool { return groups.size() <= 1; }
  [[nodiscard]] auto summary() const -> QString;
};

[[nodiscard]] auto is_road_network_element(const LinearElement& element) -> bool;

[[nodiscard]] auto
analyse_road_network(const QVector<LinearElement>& elements,
                     double tolerance = k_road_join_tolerance,
                     double snap_reach = k_road_snap_reach) -> RoadNetworkReport;

[[nodiscard]] auto snapped(const LinearElement& element,
                           const RoadNearMiss& miss) -> LinearElement;

} // namespace MapEditor
