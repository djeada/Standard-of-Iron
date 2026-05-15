#pragma once

#include <QColor>
#include <QIcon>
#include <QRectF>
#include <QString>

class QPainter;

namespace MapEditor {

[[nodiscard]] auto player_color_for_editor(int player_id) -> QColor;
[[nodiscard]] auto troop_tool_icon(const QString& type) -> QIcon;
void paint_troop_badge(QPainter& painter,
                       const QRectF& bounds,
                       const QString& type,
                       const QColor& accent_color);

} // namespace MapEditor
