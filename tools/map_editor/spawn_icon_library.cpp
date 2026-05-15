#include "spawn_icon_library.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace MapEditor {

namespace {

void draw_horse_head(QPainter& painter) {
  QPainterPath horse;
  horse.moveTo(6.0, 14.0);
  horse.cubicTo(6.0, 10.0, 8.0, 7.0, 11.0, 7.0);
  horse.cubicTo(13.0, 6.0, 15.0, 7.0, 17.0, 9.0);
  horse.lineTo(15.0, 11.0);
  painter.drawPath(horse);
}

void draw_elephant_head(QPainter& painter) {
  QPainterPath elephant;
  elephant.moveTo(7.0, 14.0);
  elephant.cubicTo(7.0, 9.0, 10.0, 6.0, 14.0, 6.0);
  elephant.cubicTo(17.0, 6.0, 19.0, 8.0, 19.0, 12.0);
  elephant.cubicTo(19.0, 15.0, 17.0, 17.0, 14.0, 17.0);
  elephant.cubicTo(12.0, 17.0, 11.0, 16.0, 10.0, 15.0);
  elephant.cubicTo(10.0, 18.0, 9.0, 20.0, 7.0, 20.0);
  painter.drawPath(elephant);
}

void draw_glyph(QPainter& painter, const QRectF& bounds, const QString& type) {
  painter.save();
  painter.translate(bounds.x(), bounds.y());
  painter.scale(bounds.width() / 24.0, bounds.height() / 24.0);
  painter.setPen(
      QPen(QColor(248, 250, 255), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  if (type == QStringLiteral("archer")) {
    QPainterPath bow;
    bow.moveTo(8.0, 4.0);
    bow.cubicTo(5.0, 7.0, 5.0, 17.0, 8.0, 20.0);
    painter.drawPath(bow);
    painter.drawLine(QPointF(8.0, 4.0), QPointF(8.0, 20.0));
    painter.drawLine(QPointF(11.0, 12.0), QPointF(19.0, 12.0));
    painter.drawPolyline(
        QPolygonF{QPointF(16.0, 9.0), QPointF(19.0, 12.0), QPointF(16.0, 15.0)});
  } else if (type == QStringLiteral("swordsman")) {
    painter.drawLine(QPointF(7.0, 18.0), QPointF(17.0, 8.0));
    painter.drawPolyline(
        QPolygonF{QPointF(17.0, 8.0), QPointF(19.0, 6.0), QPointF(18.0, 10.0)});
    painter.drawLine(QPointF(9.0, 16.0), QPointF(5.0, 20.0));
    painter.drawLine(QPointF(13.0, 12.0), QPointF(17.0, 16.0));
  } else if (type == QStringLiteral("spearman")) {
    painter.drawLine(QPointF(12.0, 4.0), QPointF(12.0, 20.0));
    painter.setBrush(QColor(248, 250, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(
        QPolygonF{QPointF(12.0, 3.0), QPointF(15.0, 8.0), QPointF(9.0, 8.0)});
    painter.setPen(
        QPen(QColor(248, 250, 255), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(8.0, 14.0), 2.5, 2.5);
  } else if (type == QStringLiteral("horse_swordsman")) {
    draw_horse_head(painter);
    painter.drawLine(QPointF(14.0, 12.0), QPointF(19.0, 7.0));
    painter.drawPolyline(
        QPolygonF{QPointF(19.0, 7.0), QPointF(20.0, 5.0), QPointF(18.0, 8.0)});
  } else if (type == QStringLiteral("horse_archer")) {
    draw_horse_head(painter);
    QPainterPath bow;
    bow.moveTo(16.0, 6.0);
    bow.cubicTo(14.0, 8.0, 14.0, 14.0, 16.0, 17.0);
    painter.drawPath(bow);
    painter.drawLine(QPointF(16.0, 6.0), QPointF(16.0, 17.0));
  } else if (type == QStringLiteral("horse_spearman")) {
    draw_horse_head(painter);
    painter.drawLine(QPointF(18.0, 4.0), QPointF(18.0, 18.0));
    painter.setBrush(QColor(248, 250, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(
        QPolygonF{QPointF(18.0, 3.0), QPointF(20.5, 7.0), QPointF(15.5, 7.0)});
  } else if (type == QStringLiteral("healer")) {
    painter.setPen(
        QPen(QColor(248, 250, 255), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawEllipse(QPointF(12.0, 12.0), 7.0, 7.0);
    painter.drawLine(QPointF(12.0, 8.0), QPointF(12.0, 16.0));
    painter.drawLine(QPointF(8.0, 12.0), QPointF(16.0, 12.0));
  } else if (type == QStringLiteral("catapult")) {
    painter.drawEllipse(QPointF(8.0, 17.0), 2.2, 2.2);
    painter.drawEllipse(QPointF(16.0, 17.0), 2.2, 2.2);
    painter.drawLine(QPointF(6.0, 15.0), QPointF(18.0, 15.0));
    painter.drawLine(QPointF(10.0, 15.0), QPointF(16.0, 7.0));
    painter.drawLine(QPointF(14.0, 8.0), QPointF(18.0, 10.0));
  } else if (type == QStringLiteral("ballista")) {
    painter.drawLine(QPointF(6.0, 8.0), QPointF(18.0, 16.0));
    painter.drawLine(QPointF(18.0, 8.0), QPointF(6.0, 16.0));
    painter.drawLine(QPointF(12.0, 16.0), QPointF(12.0, 20.0));
    painter.drawLine(QPointF(8.0, 20.0), QPointF(16.0, 20.0));
  } else if (type == QStringLiteral("elephant")) {
    draw_elephant_head(painter);
    painter.drawLine(QPointF(14.0, 10.0), QPointF(16.0, 10.0));
  } else if (type == QStringLiteral("roman_legion_organizer")) {
    QPainterPath helmet;
    helmet.moveTo(7.0, 16.0);
    helmet.cubicTo(7.0, 10.0, 10.0, 7.0, 13.0, 7.0);
    helmet.cubicTo(15.0, 7.0, 17.0, 8.0, 18.0, 10.0);
    painter.drawPath(helmet);
    QPainterPath crown;
    crown.moveTo(10.0, 7.0);
    crown.cubicTo(10.0, 5.0, 11.0, 4.0, 13.0, 4.0);
    crown.cubicTo(15.0, 4.0, 16.0, 5.0, 16.0, 7.0);
    painter.drawPath(crown);
    painter.drawLine(QPointF(18.0, 7.0), QPointF(18.0, 15.0));
    painter.drawPolyline(
        QPolygonF{QPointF(18.0, 7.0), QPointF(21.0, 8.5), QPointF(18.0, 10.0)});
  } else if (type == QStringLiteral("roman_veteran_consul")) {
    QPainterPath left;
    left.moveTo(8.0, 16.0);
    left.cubicTo(6.0, 13.0, 6.0, 10.0, 8.0, 7.0);
    painter.drawPath(left);
    QPainterPath right;
    right.moveTo(16.0, 16.0);
    right.cubicTo(18.0, 13.0, 18.0, 10.0, 16.0, 7.0);
    painter.drawPath(right);
    painter.setBrush(QColor(248, 250, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(QPolygonF{QPointF(12.0, 6.0),
                                  QPointF(13.6, 9.5),
                                  QPointF(17.5, 9.8),
                                  QPointF(14.4, 12.2),
                                  QPointF(15.5, 16.0),
                                  QPointF(12.0, 13.8),
                                  QPointF(8.5, 16.0),
                                  QPointF(9.6, 12.2),
                                  QPointF(6.5, 9.8),
                                  QPointF(10.4, 9.5)});
  } else if (type == QStringLiteral("roman_field_commander")) {
    QPainterPath helmet;
    helmet.moveTo(7.0, 16.0);
    helmet.cubicTo(7.0, 10.0, 10.0, 7.0, 14.0, 7.0);
    helmet.cubicTo(16.0, 7.0, 18.0, 8.0, 19.0, 10.0);
    painter.drawPath(helmet);
    QPainterPath crown;
    crown.moveTo(10.0, 7.0);
    crown.cubicTo(10.0, 5.0, 11.0, 4.0, 13.0, 4.0);
    crown.cubicTo(15.0, 4.0, 16.0, 5.0, 16.0, 7.0);
    painter.drawPath(crown);
    painter.drawLine(QPointF(9.0, 4.0), QPointF(7.0, 2.0));
    painter.drawLine(QPointF(11.0, 3.5), QPointF(9.5, 1.5));
    painter.drawLine(QPointF(13.0, 3.5), QPointF(12.5, 1.3));
  } else if (type == QStringLiteral("carthage_mercenary_broker")) {
    painter.drawEllipse(QPointF(10.0, 10.0), 4.0, 4.0);
    painter.drawLine(QPointF(10.0, 8.0), QPointF(10.0, 12.0));
    painter.drawLine(QPointF(8.0, 10.0), QPointF(12.0, 10.0));
    painter.drawLine(QPointF(16.0, 6.0), QPointF(19.0, 18.0));
    painter.setBrush(QColor(248, 250, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(
        QPolygonF{QPointF(16.0, 5.0), QPointF(18.5, 8.5), QPointF(14.5, 8.5)});
  } else if (type == QStringLiteral("carthage_cavalry_patron")) {
    draw_horse_head(painter);
    painter.drawEllipse(QPointF(18.0, 8.0), 2.5, 2.5);
    painter.drawLine(QPointF(18.0, 5.0), QPointF(18.0, 11.0));
    painter.drawLine(QPointF(15.0, 8.0), QPointF(21.0, 8.0));
  } else if (type == QStringLiteral("carthage_elephant_master")) {
    draw_elephant_head(painter);
    painter.drawPolyline(QPolygonF{QPointF(11.0, 4.0),
                                   QPointF(13.0, 2.0),
                                   QPointF(15.0, 4.0),
                                   QPointF(17.0, 2.0)});
  } else if (type == QStringLiteral("civilian")) {
    painter.drawEllipse(QPointF(12.0, 8.0), 3.0, 3.0);
    QPainterPath shoulders;
    shoulders.moveTo(8.0, 19.0);
    shoulders.cubicTo(8.0, 14.0, 10.0, 12.0, 12.0, 12.0);
    shoulders.cubicTo(14.0, 12.0, 16.0, 14.0, 16.0, 19.0);
    painter.drawPath(shoulders);
  } else if (type == QStringLiteral("builder")) {
    painter.drawLine(QPointF(8.0, 18.0), QPointF(16.0, 10.0));
    painter.drawLine(QPointF(12.0, 6.0), QPointF(18.0, 12.0));
    QPainterPath hammer;
    hammer.moveTo(9.0, 5.0);
    hammer.lineTo(13.0, 9.0);
    hammer.lineTo(10.0, 12.0);
    hammer.lineTo(6.0, 8.0);
    hammer.closeSubpath();
    painter.drawPath(hammer);
  } else {
    painter.drawEllipse(QPointF(12.0, 12.0), 7.0, 7.0);
  }

  painter.restore();
}

auto cached_icon_pixmap(const QString& type,
                        int pixel_size,
                        const QColor& accent_color) -> QPixmap {
  static QHash<QString, QPixmap> cache;
  const QString cache_key = type + QLatin1Char(':') + QString::number(pixel_size) +
                            QLatin1Char(':') + accent_color.name(QColor::HexArgb);
  auto it = cache.find(cache_key);
  if (it != cache.end()) {
    return it.value();
  }

  QPixmap pixmap(pixel_size, pixel_size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  if (accent_color.alpha() > 0) {
    painter.setPen(QPen(QColor(16, 22, 30, 220), 1.2));
    painter.setBrush(accent_color);
    painter.drawEllipse(QRectF(1.0, 1.0, pixel_size - 2.0, pixel_size - 2.0));
    draw_glyph(painter, QRectF(3.0, 3.0, pixel_size - 6.0, pixel_size - 6.0), type);
  } else {
    draw_glyph(painter, QRectF(0.0, 0.0, pixel_size, pixel_size), type);
  }
  painter.end();

  cache.insert(cache_key, pixmap);
  return pixmap;
}

} // namespace

auto player_color_for_editor(int player_id) -> QColor {
  switch (player_id) {
  case 1:
    return {82, 154, 255};
  case 2:
    return {236, 96, 96};
  case 3:
    return {92, 214, 122};
  case 4:
    return {235, 181, 69};
  case 0:
    return {156, 164, 173};
  default:
    return {140, 120, 200};
  }
}

auto troop_tool_icon(const QString& type) -> QIcon {
  return QIcon(cached_icon_pixmap(type, 28, QColor(69, 94, 122)));
}

void paint_troop_badge(QPainter& painter,
                       const QRectF& bounds,
                       const QString& type,
                       const QColor& accent_color) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(QColor(16, 22, 30, 220), 1.4));
  painter.setBrush(accent_color);
  painter.drawEllipse(bounds);

  const int icon_size = std::max(8, static_cast<int>(bounds.width() - 6.0));
  const QPixmap icon = cached_icon_pixmap(type, icon_size, QColor(0, 0, 0, 0));
  const QRectF icon_rect(bounds.center().x() - icon.width() * 0.5,
                         bounds.center().y() - icon.height() * 0.5,
                         icon.width(),
                         icon.height());
  painter.drawPixmap(icon_rect.topLeft(), icon);
  painter.restore();
}

} // namespace MapEditor
