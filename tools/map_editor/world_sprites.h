#pragma once

#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QString>

class QPainter;

namespace MapEditor {

[[nodiscard]] auto has_structure_sprite(const QString& type) -> bool;

[[nodiscard]] auto structure_sprite_radius_cells(const QString& type) -> float;

void paint_structure_sprite(QPainter& painter,
                            const QString& type,
                            const QPointF& centre,
                            float cell_px,
                            float rotation_deg,
                            const QColor& owner,
                            unsigned seed);

[[nodiscard]] auto has_prop_sprite(const QString& type) -> bool;

void paint_prop_sprite(QPainter& painter,
                       const QString& type,
                       const QPointF& centre,
                       float cell_px,
                       float scale,
                       float rotation_deg,
                       float glow_radius_cells,
                       unsigned seed,
                       double seconds,
                       float night);

void paint_palisade(QPainter& painter,
                    const QPolygonF& path,
                    float cell_px,
                    const QColor& owner);

void paint_gatehouse(QPainter& painter,
                     const QPointF& centre,
                     float cell_px,
                     float rotation_deg,
                     float span_cells,
                     float depth_cells,
                     const QColor& owner);

} // namespace MapEditor
