#include "icon_art.h"

#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QRectF>
#include <QStringView>
#include <QVariantMap>

#include <algorithm>

namespace Ui::IconArt {

namespace {

using S = Stroke;

auto fill(QString path, Tone tone) -> Stroke {
  return Stroke{std::move(path), tone, true, 0.0F};
}

auto line(QString path, Tone tone, float width) -> Stroke {
  return Stroke{std::move(path), tone, false, width};
}

auto pick_head() -> Stroke {
  return fill(QStringLiteral("M9.4 2.8 Q15.2 1.4 21 5.6 L19.2 8.4 Q14.6 4.8 10.4 5.8 "
                             "Z"),
              Tone::Metal);
}

auto pick_haft() -> Stroke {
  return line(QStringLiteral("M13.8 5.2 L6.4 19.8"), Tone::Metal, 2.4F);
}

auto boulder_body(Tone tone) -> Stroke {
  return fill(QStringLiteral("M2.6 21.6 L5.2 16.2 L10.4 14.4 L16.4 16.8 L18.2 21.6 Z"),
              tone);
}

auto build_catalog() -> std::vector<Art> {
  std::vector<Art> catalog;

  const auto add = [&catalog](const char* id, std::vector<Stroke> strokes) {
    catalog.push_back(Art{QString::fromLatin1(id), std::move(strokes)});
  };

  add("idle",
      {line(QStringLiteral("M12 4 L17.6 6.4 L20 12 L17.6 17.6 L12 20 L6.4 17.6 L4 12 "
                           "L6.4 6.4 Z"),
            Tone::Metal,
            2.0F),
       fill(QStringLiteral("M12 9.6 L14.4 12 L12 14.4 L9.6 12 Z"), Tone::Ink)});

  add("move",
      {line(QStringLiteral("M4 12 L13 12"), Tone::Metal, 3.0F),
       fill(QStringLiteral("M12.4 6.4 L20.4 12 L12.4 17.6 Z"), Tone::Metal),
       line(QStringLiteral("M6 12 L11 12"), Tone::Ink, 1.0F)});

  add("attack",
      {line(QStringLiteral("M4.8 19.2 L19.2 4.8"), Tone::Metal, 3.0F),
       line(QStringLiteral("M19.2 19.2 L4.8 4.8"), Tone::Metal, 3.0F),
       line(QStringLiteral("M7.6 16.4 L16.4 7.6"), Tone::Ink, 1.1F),
       fill(QStringLiteral("M12 9.4 L14.6 12 L12 14.6 L9.4 12 Z"), Tone::Ember)});

  add("patrol",
      {line(QStringLiteral("M5 8.4 L17.4 8.4"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M16.2 5.2 L21 8.4 L16.2 11.6 Z"), Tone::Metal),
       line(QStringLiteral("M19 15.6 L6.6 15.6"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M7.8 12.4 L3 15.6 L7.8 18.8 Z"), Tone::Metal)});

  add("guard",
      {fill(QStringLiteral("M12 2.8 L20 6 L20 12.4 Q20 18.4 12 21.6 Q4 18.4 4 12.4 "
                           "L4 6 Z"),
            Tone::Metal),
       line(QStringLiteral("M12 6 L12 18.4"), Tone::Ink, 1.2F),
       fill(QStringLiteral("M12 9.6 L14.4 12 L12 14.4 L9.6 12 Z"), Tone::Ember)});

  add("hold",
      {line(QStringLiteral("M12 3.6 L12 17"), Tone::Metal, 2.8F),
       line(QStringLiteral("M7 8.4 L17 8.4"), Tone::Metal, 2.4F),
       line(QStringLiteral("M4.6 20.4 L19.4 20.4"), Tone::Metal, 3.0F),
       fill(QStringLiteral("M12 1.6 L14.2 3.8 L12 6 L9.8 3.8 Z"), Tone::Ember)});

  add("construct",
      {line(QStringLiteral("M4 20.4 L4 8 L12 3.4 L20 8 L20 20.4"), Tone::Metal, 2.4F),
       line(QStringLiteral("M4 13.2 L20 13.2"), Tone::Metal, 2.0F),
       line(QStringLiteral("M12 5.4 L12 13.2"), Tone::Ink, 1.1F),
       fill(QStringLiteral("M12 15.4 L14.2 17.6 L12 19.8 L9.8 17.6 Z"), Tone::Ember)});

  add("repair",
      {fill(QStringLiteral("M2.8 12.4 L21.2 12.4 L18.6 16.4 L14 16.4 L14 20.6 L10 "
                           "20.6 L10 16.4 L5.4 16.4 Z"),
            Tone::Metal),
       fill(QStringLiteral("M6.6 20.2 L17.4 20.2 L17.4 22.2 L6.6 22.2 Z"), Tone::Metal),
       line(QStringLiteral("M4.4 11.8 L11.2 6.6"), Tone::Metal, 2.6F),
       fill(QStringLiteral("M11 2.2 L19.4 6.4 L17.2 10.4 L8.8 6.2 Z"), Tone::Metal),
       fill(QStringLiteral("M16.4 9 L18.6 11.2 L16.4 13.4 L14.2 11.2 Z"),
            Tone::Ember)});

  add("dismantle",
      {fill(QStringLiteral("M4 12.4 L20 12.4 L20 20.4 L4 20.4 Z"), Tone::Metal),
       fill(QStringLiteral("M9 14.6 L15 14.6 L15 18.2 L9 18.2 Z"), Tone::Ink),
       line(QStringLiteral("M12 2 L12 6.6"), Tone::Ember, 2.2F),
       fill(QStringLiteral("M8.4 5.8 L15.6 5.8 L12 10.4 Z"), Tone::Ember)});

  add("chop_wood",
      {fill(QStringLiteral("M2.6 16.4 L14.8 16.4 L14.8 21.6 L2.6 21.6 Z"),
            Tone::Timber),
       line(QStringLiteral("M4.8 19 L12.6 19"), Tone::Ink, 1.1F),
       line(QStringLiteral("M5.4 20.2 L13.6 6.6"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M13 3 L17.6 5.6 Q22.2 9 17.6 12.4 L13 9.8 Q15.2 6.4 13 3 "
                           "Z"),
            Tone::Metal)});

  add("mine_stone",
      {pick_head(),
       pick_haft(),
       boulder_body(Tone::Stone),
       line(QStringLiteral("M7.6 21.6 L9 17.6 L13.6 16.4"), Tone::Ink, 1.1F)});

  add("mine_iron",
      {pick_head(),
       pick_haft(),
       boulder_body(Tone::Iron),
       fill(QStringLiteral("M7.4 17.8 L8.9 19.3 L7.4 20.8 L5.9 19.3 Z"), Tone::Ember),
       fill(QStringLiteral("M13.6 17.6 L15 19 L13.6 20.4 L12.2 19 Z"), Tone::Ember)});

  add("auto_gather",
      {pick_head(),
       pick_haft(),
       line(QStringLiteral("M4.4 12 Q4.4 4.8 11.6 4.8 Q16.4 4.8 18.6 8.4"),
            Tone::Ember,
            2.0F),
       fill(QStringLiteral("M15.4 3.2 L20.6 6.2 L15.4 9.2 Z"), Tone::Ember)});

  add("deliver",
      {fill(QStringLiteral("M4.4 10.2 L12.4 10.2 L12.4 19.6 L4.4 19.6 Z"), Tone::Metal),
       line(QStringLiteral("M4.4 13.6 L12.4 13.6"), Tone::Ink, 1.1F),
       line(QStringLiteral("M13.8 14.9 L17.6 14.9"), Tone::Ember, 2.2F),
       fill(QStringLiteral("M16.6 11.4 L21.2 14.9 L16.6 18.4 Z"), Tone::Ember)});

  add("heal",
      {fill(QStringLiteral("M9.6 3.8 L14.4 3.8 L14.4 9.6 L20.2 9.6 L20.2 14.4 L14.4 "
                           "14.4 L14.4 20.2 L9.6 20.2 L9.6 14.4 L3.8 14.4 L3.8 9.6 "
                           "L9.6 9.6 Z"),
            Tone::Metal),
       fill(QStringLiteral("M12 9.4 L14.6 12 L12 14.6 L9.4 12 Z"), Tone::Ember)});

  add("train",
      {fill(QStringLiteral("M4.4 14.6 Q4.4 4 12 4 Q19.6 4 19.6 14.6 Z"), Tone::Metal),
       fill(QStringLiteral("M2.8 14.4 L21.2 14.4 L21.2 17.4 L2.8 17.4 Z"), Tone::Metal),
       fill(QStringLiteral("M5.8 17.4 L9.8 17.4 L9.8 21.6 L5.8 21.6 Z"), Tone::Metal),
       fill(QStringLiteral("M14.2 17.4 L18.2 17.4 L18.2 21.6 L14.2 21.6 Z"),
            Tone::Metal),
       fill(QStringLiteral("M10.2 1.4 L13.8 1.4 L13.8 4.6 L10.2 4.6 Z"), Tone::Ember),
       line(QStringLiteral("M12 6.2 L12 14.2"), Tone::Ink, 1.4F)});

  add("blocked",
      {line(QStringLiteral("M8 3.8 L16 3.8 L20.2 8 L20.2 16 L16 20.2 L8 20.2 L3.8 16 "
                           "L3.8 8 Z"),
            Tone::Metal,
            2.4F),
       line(QStringLiteral("M7.2 16.8 L16.8 7.2"), Tone::Ember, 2.6F)});

  add("collect",
      {pick_head(),
       pick_haft(),
       fill(QStringLiteral("M1.8 21.6 L6 15.4 L9.4 21.6 Z"), Tone::Timber),
       fill(QStringLiteral("M9 21.6 L13.8 15 L18.8 21.6 Z"), Tone::Stone)});

  add("formation",
      {fill(QStringLiteral("M4 5.4 L20 5.4 L20 8.8 L4 8.8 Z"), Tone::Metal),
       fill(QStringLiteral("M4 10.4 L20 10.4 L20 13.8 L4 13.8 Z"), Tone::Metal),
       fill(QStringLiteral("M4 15.4 L20 15.4 L20 18.8 L4 18.8 Z"), Tone::Metal),
       line(QStringLiteral("M9.4 5.4 L9.4 18.8"), Tone::Ink, 1.0F),
       line(QStringLiteral("M14.6 5.4 L14.6 18.8"), Tone::Ink, 1.0F)});

  add("rally",
      {line(QStringLiteral("M7 2.8 L7 21.4"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M7.6 4 L19.4 7.2 L7.6 11.2 Z"), Tone::Ember),
       fill(QStringLiteral("M4 20 L10 20 L10 22.2 L4 22.2 Z"), Tone::Metal)});

  add("stop",
      {fill(QStringLiteral("M6 6 L18 6 L18 18 L6 18 Z"), Tone::Metal),
       line(QStringLiteral("M9 9 L15 15"), Tone::Ink, 1.2F),
       line(QStringLiteral("M15 9 L9 15"), Tone::Ink, 1.2F)});

  add("run",
      {fill(QStringLiteral("M3.4 4.8 L10.4 12 L3.4 19.2 L7.4 19.2 L14.4 12 L7.4 4.8 Z"),
            Tone::Metal),
       fill(QStringLiteral("M10.6 4.8 L17.6 12 L10.6 19.2 L14.6 19.2 L21.6 12 L14.6 "
                           "4.8 Z"),
            Tone::Ember)});

  add("aura",
      {line(QStringLiteral("M12 2.6 L18.6 5.4 L21.4 12 L18.6 18.6 L12 21.4 L5.4 18.6 "
                           "L2.6 12 L5.4 5.4 Z"),
            Tone::Ember,
            2.0F),
       line(QStringLiteral("M12 7.4 L15.2 8.8 L16.6 12 L15.2 15.2 L12 16.6 L8.8 15.2 "
                           "L7.4 12 L8.8 8.8 Z"),
            Tone::Metal,
            1.8F),
       fill(QStringLiteral("M12 10.4 L13.6 12 L12 13.6 L10.4 12 Z"), Tone::Ember)});

  add("gate",
      {fill(QStringLiteral("M3.8 20.4 L3.8 9.2 Q12 2.2 20.2 9.2 L20.2 20.4 Z"),
            Tone::Metal),
       fill(QStringLiteral("M8 20.4 L8 12.2 Q12 8.6 16 12.2 L16 20.4 Z"), Tone::Ink),
       line(QStringLiteral("M12 10 L12 20.4"), Tone::Metal, 1.4F)});

  add("wood",
      {fill(QStringLiteral("M3.4 8.6 L20.6 8.6 L20.6 15.4 L3.4 15.4 Z"), Tone::Timber),
       line(QStringLiteral("M7 12 L17 12"), Tone::Ink, 1.1F)});

  add("stone",
      {fill(QStringLiteral("M4 19.4 L7 10.2 L13 8 L20 12.2 L20 19.4 Z"), Tone::Stone),
       line(QStringLiteral("M9.4 19.4 L11 13 L16.4 11.6"), Tone::Ink, 1.0F)});

  add("iron",
      {fill(QStringLiteral("M4 19.4 L7 10.2 L13 8 L20 12.2 L20 19.4 Z"), Tone::Iron),
       fill(QStringLiteral("M9 14.2 L10.6 15.8 L9 17.4 L7.4 15.8 Z"), Tone::Ember),
       fill(QStringLiteral("M14.8 13.2 L16.2 14.6 L14.8 16 L13.4 14.6 Z"),
            Tone::Ember)});

  add("gold",
      {fill(QStringLiteral("M4.4 12.6 L19.6 12.6 L19.6 18.4 L4.4 18.4 Z"), Tone::Gold),
       fill(QStringLiteral("M7.2 6.6 L16.8 6.6 L16.8 12.2 L7.2 12.2 Z"), Tone::Gold),
       line(QStringLiteral("M4.4 15.4 L19.6 15.4"), Tone::Ink, 1.0F)});

  return catalog;
}

auto catalog() -> const std::vector<Art>& {
  static const std::vector<Art> k_catalog = build_catalog();
  return k_catalog;
}

auto index() -> const QHash<QString, const Art*>& {
  static const QHash<QString, const Art*> k_index = [] {
    QHash<QString, const Art*> map;
    for (const Art& art : catalog()) {
      map.insert(art.id, &art);
    }
    return map;
  }();
  return k_index;
}

auto aliases() -> const QHash<QString, QString>& {
  static const QHash<QString, QString> k_aliases = {
      {QStringLiteral("build"), QStringLiteral("construct")},
      {QStringLiteral("defense"), QStringLiteral("guard")},
      {QStringLiteral("cut_tree"), QStringLiteral("chop_wood")},
      {QStringLiteral("collect_stone"), QStringLiteral("mine_stone")},
      {QStringLiteral("collect_iron_ore"), QStringLiteral("mine_iron")},
      {QStringLiteral("repair_structure"), QStringLiteral("repair")},
      {QStringLiteral("dismantle_structure"), QStringLiteral("dismantle")},
      {QStringLiteral("unavailable"), QStringLiteral("blocked")},
  };
  return k_aliases;
}

struct PathCursor {
  QStringView text;
  int position{0};

  auto skip_separators() -> void {
    while (position < text.size() &&
           (text.at(position).isSpace() || text.at(position) == QLatin1Char(','))) {
      ++position;
    }
  }

  auto at_end() -> bool {
    skip_separators();
    return position >= text.size();
  }

  auto next_command() -> QChar {
    skip_separators();
    if (position >= text.size()) {
      return {};
    }
    return text.at(position++);
  }

  auto next_number(bool& ok) -> qreal {
    skip_separators();
    const int start = position;
    if (position < text.size() && (text.at(position) == QLatin1Char('-') ||
                                   text.at(position) == QLatin1Char('+'))) {
      ++position;
    }
    while (position < text.size() &&
           (text.at(position).isDigit() || text.at(position) == QLatin1Char('.'))) {
      ++position;
    }
    if (position == start) {
      ok = false;
      return 0.0;
    }
    return text.mid(start, position - start).toDouble(&ok);
  }
};

} // namespace

auto Palette::color_for(Tone tone) const -> QColor {
  switch (tone) {
  case Tone::Ink:
    return ink;
  case Tone::Metal:
    return metal;
  case Tone::Edge:
    return edge;
  case Tone::Ember:
    return ember;
  case Tone::Timber:
    return timber;
  case Tone::Stone:
    return stone;
  case Tone::Iron:
    return iron;
  case Tone::Gold:
    return gold;
  }
  return metal;
}

auto tone_id(Tone tone) -> QString {
  switch (tone) {
  case Tone::Ink:
    return QStringLiteral("ink");
  case Tone::Metal:
    return QStringLiteral("metal");
  case Tone::Edge:
    return QStringLiteral("edge");
  case Tone::Ember:
    return QStringLiteral("ember");
  case Tone::Timber:
    return QStringLiteral("timber");
  case Tone::Stone:
    return QStringLiteral("stone");
  case Tone::Iron:
    return QStringLiteral("iron");
  case Tone::Gold:
    return QStringLiteral("gold");
  }
  return QStringLiteral("metal");
}

auto tone_from_id(const QString& id) -> Tone {
  if (id == QStringLiteral("ink")) {
    return Tone::Ink;
  }
  if (id == QStringLiteral("edge")) {
    return Tone::Edge;
  }
  if (id == QStringLiteral("ember")) {
    return Tone::Ember;
  }
  if (id == QStringLiteral("timber")) {
    return Tone::Timber;
  }
  if (id == QStringLiteral("stone")) {
    return Tone::Stone;
  }
  if (id == QStringLiteral("iron")) {
    return Tone::Iron;
  }
  if (id == QStringLiteral("gold")) {
    return Tone::Gold;
  }
  return Tone::Metal;
}

auto resolve_id(const QString& id) -> QString {
  const auto alias = aliases().constFind(id);
  return alias != aliases().constEnd() ? alias.value() : id;
}

auto find(const QString& id) -> const Art* {
  const auto resolved = resolve_id(id);
  const auto match = index().constFind(resolved);
  return match != index().constEnd() ? match.value() : nullptr;
}

auto ids() -> QStringList {
  QStringList result;
  result.reserve(static_cast<int>(catalog().size()));
  for (const Art& art : catalog()) {
    result.append(art.id);
  }
  return result;
}

auto build_path(const QString& path_data,
                qreal scale,
                qreal offset_x,
                qreal offset_y) -> QPainterPath {
  QPainterPath path;
  PathCursor cursor{QStringView(path_data), 0};
  const auto point = [&](qreal x, qreal y) {
    return QPointF(offset_x + x * scale, offset_y + y * scale);
  };

  while (!cursor.at_end()) {
    const QChar command = cursor.next_command();
    bool ok = true;
    if (command == QLatin1Char('M') || command == QLatin1Char('L')) {
      const qreal x = cursor.next_number(ok);
      const qreal y = cursor.next_number(ok);
      if (!ok) {
        break;
      }
      if (command == QLatin1Char('M')) {
        path.moveTo(point(x, y));
      } else {
        path.lineTo(point(x, y));
      }
    } else if (command == QLatin1Char('Q')) {
      const qreal cx = cursor.next_number(ok);
      const qreal cy = cursor.next_number(ok);
      const qreal x = cursor.next_number(ok);
      const qreal y = cursor.next_number(ok);
      if (!ok) {
        break;
      }
      path.quadTo(point(cx, cy), point(x, y));
    } else if (command == QLatin1Char('Z') || command == QLatin1Char('z')) {
      path.closeSubpath();
    } else {
      break;
    }
  }
  return path;
}

auto default_palette() -> Palette {
  Palette palette;
  palette.ink = QColor(QStringLiteral("#140f0a"));
  palette.metal = QColor(QStringLiteral("#d7cbb2"));
  palette.edge = QColor(QStringLiteral("#fff1cf"));
  palette.ember = QColor(QStringLiteral("#e0a542"));
  palette.timber = QColor(QStringLiteral("#8a5a32"));
  palette.stone = QColor(QStringLiteral("#8e8b86"));
  palette.iron = QColor(QStringLiteral("#7f8a96"));
  palette.gold = QColor(QStringLiteral("#d9a441"));
  return palette;
}

void paint(QPainter& painter,
           const QString& id,
           const QRectF& rect,
           const Palette& palette,
           qreal opacity) {
  const Art* art = find(id);
  if (art == nullptr || rect.isEmpty()) {
    return;
  }

  const qreal size = std::min(rect.width(), rect.height());
  const qreal scale = size / static_cast<qreal>(k_design_grid);
  const qreal offset_x = rect.x() + (rect.width() - size) * 0.5;
  const qreal offset_y = rect.y() + (rect.height() - size) * 0.5;

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setOpacity(painter.opacity() * opacity);
  for (const Stroke& stroke : art->strokes) {
    const QPainterPath path = build_path(stroke.path, scale, offset_x, offset_y);
    const QColor color = palette.color_for(stroke.tone);
    if (stroke.filled) {
      painter.setPen(Qt::NoPen);
      painter.fillPath(path, color);
    } else {
      QPen pen(color);
      pen.setWidthF(std::max(1.0, static_cast<qreal>(stroke.width) * scale));
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);
    }
  }
  painter.restore();
}

} // namespace Ui::IconArt

IconArtLibrary::IconArtLibrary(QObject* parent)
    : QObject(parent) {
}

bool IconArtLibrary::has(const QString& id) {
  return Ui::IconArt::find(id) != nullptr;
}

QStringList IconArtLibrary::ids() {
  return Ui::IconArt::ids();
}

QString IconArtLibrary::resolve(const QString& id) {
  return Ui::IconArt::resolve_id(id);
}

QVariantList IconArtLibrary::strokes(const QString& id) {
  QVariantList result;
  const Ui::IconArt::Art* art = Ui::IconArt::find(id);
  if (art == nullptr) {
    return result;
  }

  constexpr qreal k_flatten_scale = 256.0;
  for (const Ui::IconArt::Stroke& stroke : art->strokes) {
    const QPainterPath path = Ui::IconArt::build_path(
        stroke.path, k_flatten_scale / Ui::IconArt::k_design_grid, 0.0, 0.0);
    QVariantList subpaths;
    for (const QPolygonF& polygon : path.toSubpathPolygons()) {
      QVariantList points;
      points.reserve(polygon.size() * 2);
      for (const QPointF& vertex : polygon) {
        points.append(vertex.x() / k_flatten_scale);
        points.append(vertex.y() / k_flatten_scale);
      }
      if (points.size() >= 4) {
        subpaths.append(QVariant(points));
      }
    }
    if (subpaths.isEmpty()) {
      continue;
    }

    QVariantMap entry;
    entry[QStringLiteral("tone")] = Ui::IconArt::tone_id(stroke.tone);
    entry[QStringLiteral("filled")] = stroke.filled;
    entry[QStringLiteral("width")] = static_cast<double>(stroke.width) /
                                     static_cast<double>(Ui::IconArt::k_design_grid);
    entry[QStringLiteral("subpaths")] = subpaths;
    result.append(entry);
  }
  return result;
}

auto IconArtLibrary::create(QQmlEngine* engine,
                            QJSEngine* script_engine) -> IconArtLibrary* {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)
  auto* library = new IconArtLibrary();
  QQmlEngine::setObjectOwnership(library, QQmlEngine::CppOwnership);
  return library;
}
