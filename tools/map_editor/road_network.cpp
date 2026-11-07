#include "road_network.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace MapEditor {

namespace {

auto nearest_on_segment(QPointF p, QPointF a, QPointF b) -> QPointF {
  const QPointF d = b - a;
  const double len2 = QPointF::dotProduct(d, d);
  const double t =
      len2 > 0.0 ? std::clamp(QPointF::dotProduct(p - a, d) / len2, 0.0, 1.0) : 0.0;
  return a + (d * t);
}

auto nearest_on_polyline(QPointF p, const QVector<QPointF>& line) -> QPointF {
  QPointF best = line.isEmpty() ? p : line.first();
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 1; i < line.size(); ++i) {
    const QPointF q = nearest_on_segment(p, line[i - 1], line[i]);
    const double d = std::hypot(q.x() - p.x(), q.y() - p.y());
    if (d < best_d) {
      best_d = d;
      best = q;
    }
  }
  return best;
}

auto distance(QPointF a, QPointF b) -> double {
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

auto cross(QPointF o, QPointF a, QPointF b) -> double {
  return ((a.x() - o.x()) * (b.y() - o.y())) - ((a.y() - o.y()) * (b.x() - o.x()));
}

auto segments_cross(QPointF a, QPointF b, QPointF c, QPointF d) -> bool {
  const double d1 = cross(c, d, a);
  const double d2 = cross(c, d, b);
  const double d3 = cross(a, b, c);
  const double d4 = cross(a, b, d);
  return ((d1 > 0.0) != (d2 > 0.0)) && ((d3 > 0.0) != (d4 > 0.0));
}

auto polylines_cross(const QVector<QPointF>& a, const QVector<QPointF>& b) -> bool {
  for (int i = 1; i < a.size(); ++i) {
    for (int j = 1; j < b.size(); ++j) {
      if (segments_cross(a[i - 1], a[i], b[j - 1], b[j])) {
        return true;
      }
    }
  }
  return false;
}

auto find(QVector<int>& parent, int x) -> int {
  while (parent[x] != x) {
    parent[x] = parent[parent[x]];
    x = parent[x];
  }
  return x;
}

} // namespace

auto RoadNetworkReport::summary() const -> QString {
  if (road_count == 0) {
    return QStringLiteral("The map has no roads.");
  }
  if (connected()) {
    return QStringLiteral("All %1 road(s) form one connected network (%2 bridge/ford "
                          "connector(s)).")
        .arg(road_count)
        .arg(connector_count);
  }
  return QStringLiteral(
             "%1 road(s) form %2 separate networks; %3 of the breaks are near "
             "misses that can be snapped closed.")
      .arg(road_count)
      .arg(groups.size())
      .arg(near_misses.size());
}

auto is_road_network_element(const LinearElement& element) -> bool {
  return element.type == QLatin1String("road") ||
         element.type == QLatin1String("bridge") ||
         element.type == QLatin1String("ford");
}

auto analyse_road_network(const QVector<LinearElement>& elements,
                          double tolerance,
                          double snap_reach) -> RoadNetworkReport {
  RoadNetworkReport report;
  QVector<int> members;
  QVector<QVector<QPointF>> lines;
  for (int i = 0; i < elements.size(); ++i) {
    if (!is_road_network_element(elements[i])) {
      continue;
    }
    members.append(i);
    lines.append(linear_polyline(elements[i]));
    if (elements[i].type == QLatin1String("road")) {
      ++report.road_count;
    } else {
      ++report.connector_count;
    }
  }
  const int n = static_cast<int>(members.size());
  QVector<int> parent(n);
  std::iota(parent.begin(), parent.end(), 0);
  const auto touches = [&](int a, int b) {
    for (const QPointF& end : {lines[a].first(), lines[a].last()}) {
      if (distance(end, nearest_on_polyline(end, lines[b])) <= tolerance) {
        return true;
      }
    }
    return false;
  };
  for (int a = 0; a < n; ++a) {
    for (int b = a + 1; b < n; ++b) {
      if (touches(a, b) || touches(b, a) || polylines_cross(lines[a], lines[b])) {
        parent[find(parent, a)] = find(parent, b);
      }
    }
  }
  QHash<int, QVector<int>> grouped;
  for (int i = 0; i < n; ++i) {
    grouped[find(parent, i)].append(i);
  }
  QVector<QVector<int>> local_groups;
  for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
    const bool carries_a_road =
        std::any_of(it.value().begin(), it.value().end(), [&](int local) {
          return elements[members[local]].type == QLatin1String("road");
        });
    if (carries_a_road) {
      local_groups.append(it.value());
    }
  }
  std::sort(local_groups.begin(),
            local_groups.end(),
            [](const QVector<int>& a, const QVector<int>& b) {
              return a.size() != b.size() ? a.size() > b.size() : a.first() < b.first();
            });
  QVector<int> group_of(n, -1);
  for (int g = 0; g < local_groups.size(); ++g) {
    QVector<int> indices;
    for (const int local : local_groups[g]) {
      group_of[local] = g;
      indices.append(members[local]);
    }
    std::sort(indices.begin(), indices.end());
    report.groups.append(indices);
  }
  for (int g = 1; g < local_groups.size(); ++g) {
    RoadNearMiss best;
    best.gap = std::numeric_limits<double>::infinity();
    for (int local = 0; local < n; ++local) {
      if (group_of[local] < 0 ||
          elements[members[local]].type != QLatin1String("road")) {
        continue;
      }
      const bool inside = group_of[local] == g;
      for (const bool at_start : {true, false}) {
        const QPointF end = at_start ? lines[local].first() : lines[local].last();
        for (int other = 0; other < n; ++other) {
          if (group_of[other] < 0 || (group_of[other] == g) == inside) {
            continue;
          }
          const QPointF target = nearest_on_polyline(end, lines[other]);
          const double gap = distance(end, target);
          if (gap > tolerance && gap <= snap_reach && gap < best.gap) {
            best = {members[local], at_start, end, target, members[other], gap};
          }
        }
      }
    }
    if (best.element >= 0) {
      report.near_misses.append(best);
    }
  }
  return report;
}

auto snapped(const LinearElement& element, const RoadNearMiss& miss) -> LinearElement {
  LinearElement out = element;
  const QVector2D target(static_cast<float>(miss.target.x()),
                         static_cast<float>(miss.target.y()));
  if (out.waypoints.isEmpty()) {
    out.waypoints = {QPointF(out.start.x(), out.start.y()),
                     QPointF(out.end.x(), out.end.y())};
  }
  if (miss.at_start) {
    out.waypoints.prepend(miss.target);
    out.start = target;
  } else {
    out.waypoints.append(miss.target);
    out.end = target;
  }
  return out;
}

} // namespace MapEditor
