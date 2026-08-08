#pragma once

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <optional>

#include "map_data.h"

namespace MapEditor::WallGeometry {

inline constexpr float k_lattice = 2.0F;

inline constexpr float k_gate_span = 6.0F;

inline constexpr float k_gate_depth = 2.0F;

inline constexpr float k_gate_snap_reach = 4.0F;

[[nodiscard]] inline auto snap(float value) -> float {
  return std::round(value / k_lattice) * k_lattice;
}

[[nodiscard]] inline auto is_wall(const LinearElement& elem) -> bool {
  return elem.type == QStringLiteral("wall");
}

[[nodiscard]] inline auto is_horizontal(const LinearElement& elem) -> bool {
  return std::abs(elem.end.x() - elem.start.x()) >=
         std::abs(elem.end.y() - elem.start.y());
}

struct WallHit {
  int index = -1;
  float distance = 0.0F;
  bool horizontal = true;
  QVector2D foot;
};

[[nodiscard]] inline auto nearest_wall(const QVector<LinearElement>& elements,
                                       const QPointF& point,
                                       float reach = k_gate_snap_reach) -> WallHit {
  WallHit best;
  best.distance = reach;
  const QVector2D probe(static_cast<float>(point.x()), static_cast<float>(point.y()));
  for (int index = 0; index < elements.size(); ++index) {
    const LinearElement& elem = elements[index];
    if (!is_wall(elem)) {
      continue;
    }
    const QVector2D span = elem.end - elem.start;
    const float length_sq = span.lengthSquared();
    const float along =
        length_sq < 1.0e-6F
            ? 0.0F
            : std::clamp(QVector2D::dotProduct(probe - elem.start, span) / length_sq,
                         0.0F,
                         1.0F);
    const QVector2D foot = elem.start + span * along;
    const float distance = (probe - foot).length();
    if (distance <= best.distance) {
      best.index = index;
      best.distance = distance;
      best.horizontal = is_horizontal(elem);
      best.foot = foot;
    }
  }
  return best;
}

[[nodiscard]] inline auto
trim_run_to_gate(const LinearElement& run,
                 bool horizontal,
                 float gate_centre,
                 bool keep_low) -> std::optional<LinearElement> {
  const float edge = keep_low ? gate_centre - k_gate_span * 0.5F - 1.0F
                              : gate_centre + k_gate_span * 0.5F + 1.0F;
  const float low = horizontal ? std::min(run.start.x(), run.end.x())
                               : std::min(run.start.y(), run.end.y());
  const float high = horizontal ? std::max(run.start.x(), run.end.x())
                                : std::max(run.start.y(), run.end.y());

  const float from = keep_low ? low : std::max(edge, low);
  const float to = keep_low ? std::min(edge, high) : high;
  if (to - from < k_lattice * 0.5F) {
    return std::nullopt;
  }

  LinearElement piece = run;
  const float cross = horizontal ? run.start.y() : run.start.x();
  if (horizontal) {
    piece.start = QVector2D(snap(from), cross);
    piece.end = QVector2D(snap(to), cross);
  } else {
    piece.start = QVector2D(cross, snap(from));
    piece.end = QVector2D(cross, snap(to));
  }
  return piece;
}

struct GatePlacement {
  float x = 0.0F;
  float z = 0.0F;
  float rotation = 0.0F;
  int wall_index = -1;
  bool horizontal = true;
  float centre = 0.0F;
};

[[nodiscard]] inline auto plan_gate(const QVector<LinearElement>& elements,
                                    const QPointF& point) -> GatePlacement {
  GatePlacement plan;
  const WallHit hit = nearest_wall(elements, point);
  if (hit.index < 0) {
    plan.x = snap(static_cast<float>(point.x()));
    plan.z = snap(static_cast<float>(point.y()));
    return plan;
  }

  const LinearElement& run = elements[hit.index];
  plan.wall_index = hit.index;
  plan.horizontal = hit.horizontal;
  plan.centre = snap(hit.horizontal ? hit.foot.x() : hit.foot.y());
  plan.x = hit.horizontal ? plan.centre : run.start.x();
  plan.z = hit.horizontal ? run.start.y() : plan.centre;
  plan.rotation = hit.horizontal ? 0.0F : 90.0F;
  return plan;
}

} // namespace MapEditor::WallGeometry
