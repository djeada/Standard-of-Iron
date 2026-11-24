#include "town_block.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

#include "game/systems/building_collision_registry.h"
#include "game/visuals/building_asset_key.h"
#include "map_json_keys.h"
#include "wall_geometry.h"

namespace MapEditor::TownBlock {

namespace {

constexpr float k_deg_to_rad = 0.01745329251994329577F;
constexpr float k_shortest_road = 3.0F;
constexpr float k_strip_overlap = 1.2F;

auto point_of(const QJsonValue& value, QPointF& out) -> bool {
  const QJsonArray pair = value.toArray();
  if (pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble()) {
    return false;
  }
  out = QPointF(pair[0].toDouble(), pair[1].toDouble());
  return true;
}

void read_owner(const QJsonObject& op, Owner& owner) {
  owner.player_id = op.value(QStringLiteral("player_id")).toInt(owner.player_id);
  owner.nation = op.value(QStringLiteral("nation")).toString(owner.nation);
  owner.max_population =
      op.value(QStringLiteral("max_population")).toInt(owner.max_population);
}

auto read_style(const QJsonObject& op,
                const QString& type,
                QString& style,
                QString& error) -> bool {
  if (op.contains(QStringLiteral("style"))) {
    style = op.value(QStringLiteral("style")).toString();
  }
  if (!style.isEmpty() && !Game::Visuals::is_building_visual_style(
                              type.toStdString(), style.toStdString())) {
    error = QStringLiteral("'%1' is not a style of %2").arg(style, type);
    return false;
  }
  return true;
}

auto house(const QString& type,
           const QString& style,
           const QString& id,
           int number,
           QPointF at,
           float facing,
           const Owner& owner) -> StructureElement {
  StructureElement element;
  element.type = type;
  element.x = static_cast<float>(at.x());
  element.z = static_cast<float>(at.y());
  element.rotation = facing;
  element.player_id = owner.player_id;
  element.max_population = owner.max_population;
  element.nation = owner.nation;
  if (!style.isEmpty()) {
    element.extra_fields[QStringLiteral("style")] = style;
  }
  if (!id.isEmpty()) {
    element.extra_fields[QStringLiteral("id")] =
        QStringLiteral("%1_%2").arg(id).arg(number);
    element.extra_fields[QStringLiteral("group")] = id;
  }
  return element;
}

auto normalised(float degrees) -> float {
  float turn = std::fmod(degrees, 360.0F);
  if (turn < 0.0F) {
    turn += 360.0F;
  }
  return turn;
}

} // namespace

auto lot_of(const QString& building_type) -> Lot {
  const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(
      building_type.toStdString());
  return {size.width, size.depth};
}

auto terrace(const TerraceSpec& spec) -> QVector<StructureElement> {
  QVector<StructureElement> out;
  const Lot lot = lot_of(spec.type);
  const QPointF span = spec.to - spec.from;
  const float length = static_cast<float>(std::hypot(span.x(), span.y()));
  const int count = static_cast<int>(std::floor((length + 0.01F) / lot.width));
  if (count <= 0) {
    return out;
  }
  const QPointF step =
      span / static_cast<qreal>(length) * static_cast<qreal>(lot.width);
  const QPointF first =
      spec.from +
      (span / static_cast<qreal>(length)) *
          static_cast<qreal>((length - (static_cast<float>(count) * lot.width)) * 0.5F +
                             lot.width * 0.5F);
  int number = 0;
  for (int index = 0; index < count; ++index) {
    if (spec.skip.contains(index)) {
      continue;
    }
    out.append(house(spec.type,
                     spec.style,
                     spec.id,
                     number++,
                     first + (step * static_cast<qreal>(index)),
                     normalised(spec.facing),
                     spec.owner));
  }
  return out;
}

auto block_extent(const BlockSpec& spec) -> BlockExtent {
  const Lot lot = lot_of(spec.type);
  return {static_cast<float>(spec.lots_wide) * lot.width,
          (2.0F * lot.depth) + (static_cast<float>(spec.lots_deep) * lot.width)};
}

auto block(const BlockSpec& spec) -> QVector<StructureElement> {
  const Lot lot = lot_of(spec.type);
  const BlockExtent extent = block_extent(spec);
  const float turn = spec.rotation * k_deg_to_rad;
  const float cosine = std::cos(turn);
  const float sine = std::sin(turn);
  // Block-local (u along the first terrace, v into the block) to the map.
  const auto place = [&](float u, float v) {
    return QPointF(spec.origin.x() + (cosine * u) + (sine * v),
                   spec.origin.y() - (sine * u) + (cosine * v));
  };

  QVector<StructureElement> out;
  const auto lay = [&](const char* suffix,
                       QPointF from,
                       QPointF to,
                       float facing,
                       QVector<int> skip) {
    TerraceSpec row;
    row.from = from;
    row.to = to;
    row.facing = facing + spec.rotation;
    row.type = spec.type;
    row.style = spec.style;
    row.id = spec.id.isEmpty() ? QString() : spec.id + QString::fromLatin1(suffix);
    row.skip = std::move(skip);
    row.owner = spec.owner;
    out += terrace(row);
  };

  const float half_depth = lot.depth * 0.5F;
  QVector<int> alley;
  if (spec.alley_lot >= 0 && spec.alley_lot < spec.lots_wide) {
    alley.append(spec.alley_lot);
  }
  lay("_n", place(0.0F, half_depth), place(extent.width, half_depth), 180.0F, alley);
  lay("_s",
      place(0.0F, extent.depth - half_depth),
      place(extent.width, extent.depth - half_depth),
      0.0F,
      {});
  lay("_w",
      place(half_depth, lot.depth),
      place(half_depth, extent.depth - lot.depth),
      270.0F,
      {});
  lay("_e",
      place(extent.width - half_depth, lot.depth),
      place(extent.width - half_depth, extent.depth - lot.depth),
      90.0F,
      {});
  return out;
}

auto paving(const PavingSpec& spec) -> QVector<LinearElement> {
  QVector<LinearElement> out;
  const float x0 = static_cast<float>(std::min(spec.min.x(), spec.max.x()));
  const float x1 = static_cast<float>(std::max(spec.min.x(), spec.max.x()));
  const float z0 = static_cast<float>(std::min(spec.min.y(), spec.max.y()));
  const float z1 = static_cast<float>(std::max(spec.min.y(), spec.max.y()));
  const bool along_x = (x1 - x0) >= (z1 - z0);
  const float along = along_x ? x1 - x0 : z1 - z0;
  const float across = along_x ? z1 - z0 : x1 - x0;
  if (along < k_shortest_road || across <= 0.0F) {
    return out;
  }
  const float strip = std::clamp(spec.strip, 2.0F, std::max(across, 2.0F));
  const int strips = std::max(
      1, static_cast<int>(std::ceil(across / std::max(strip - k_strip_overlap, 1.0F))));
  const float width = std::min(strip, across);
  const float pitch =
      strips > 1 ? (across - width) / static_cast<float>(strips - 1) : 0.0F;
  for (int index = 0; index < strips; ++index) {
    const float offset = (width * 0.5F) + (pitch * static_cast<float>(index));
    LinearElement road;
    road.type = QStringLiteral("road");
    road.width = width;
    road.style = spec.style;
    if (along_x) {
      road.start = QVector2D(x0, z0 + offset);
      road.end = QVector2D(x1, z0 + offset);
    } else {
      road.start = QVector2D(x0 + offset, z0);
      road.end = QVector2D(x0 + offset, z1);
    }
    out.append(road);
  }
  if (strips > 1) {
    // One strip across the others binds them: a square is one paved surface,
    // and one road network, not a row of lanes that never meet.
    const float reach = (across - width) * 0.5F;
    LinearElement tie;
    tie.type = QStringLiteral("road");
    tie.width = std::min(width, along);
    tie.style = spec.style;
    const float mid_x = (x0 + x1) * 0.5F;
    const float mid_z = (z0 + z1) * 0.5F;
    if (along_x) {
      tie.start = QVector2D(mid_x, mid_z - reach);
      tie.end = QVector2D(mid_x, mid_z + reach);
    } else {
      tie.start = QVector2D(mid_x - reach, mid_z);
      tie.end = QVector2D(mid_x + reach, mid_z);
    }
    if ((tie.end - tie.start).length() >= k_shortest_road) {
      out.append(tie);
    }
  }
  return out;
}

auto wall(const WallSpec& spec, QString& error) -> WallPlan {
  namespace WG = WallGeometry;
  WallPlan plan;
  QVector<QPointF> corners;
  for (const QPointF& point : spec.points) {
    corners.append(QPointF(WG::snap(static_cast<float>(point.x())),
                           WG::snap(static_cast<float>(point.y()))));
  }
  if (corners.size() < 2) {
    error = QStringLiteral("a wall needs at least two points");
    return plan;
  }
  const int legs = spec.closed ? corners.size() : corners.size() - 1;
  const bool towers = spec.tower_spacing > 0.0F;
  constexpr float k_tower_clear = 2.0F;

  int tower_number = 0;
  int run_number = 0;
  const auto add_tower = [&](QPointF at) {
    StructureElement tower;
    tower.type = QStringLiteral("defense_tower");
    tower.x = static_cast<float>(at.x());
    tower.z = static_cast<float>(at.y());
    tower.player_id = spec.owner.player_id;
    tower.nation = spec.owner.nation;
    tower.max_population = spec.owner.max_population;
    if (!spec.id.isEmpty()) {
      const QString name = QStringLiteral("%1_tower_%2").arg(spec.id).arg(tower_number);
      tower.extra_fields[QStringLiteral("id")] = name;
      tower.extra_fields[QStringLiteral("group")] = name;
    }
    ++tower_number;
    plan.towers.append(tower);
  };
  const auto add_run = [&](QPointF from, QPointF to) {
    if (std::hypot(to.x() - from.x(), to.y() - from.y()) < WG::k_lattice) {
      return;
    }
    LinearElement run;
    run.type = QStringLiteral("wall");
    run.start = QVector2D(static_cast<float>(from.x()), static_cast<float>(from.y()));
    run.end = QVector2D(static_cast<float>(to.x()), static_cast<float>(to.y()));
    run.width = WG::k_lattice;
    run.player_id = spec.owner.player_id;
    run.nation = spec.owner.nation;
    if (!spec.style.isEmpty()) {
      run.extra_fields[QStringLiteral("style")] = spec.style;
    }
    if (!spec.id.isEmpty()) {
      const QString name = QStringLiteral("%1_wall_%2").arg(spec.id).arg(run_number);
      run.extra_fields[QStringLiteral("id")] = name;
      run.extra_fields[QStringLiteral("group")] = name;
    }
    ++run_number;
    plan.runs.append(run);
  };

  for (int leg = 0; leg < legs; ++leg) {
    const QPointF from = corners[leg];
    const QPointF to = corners[(leg + 1) % corners.size()];
    // Legs along x or z join the wall lattice; a leg at an angle is laid in
    // free pieces along its true line (gates belong on the lattice legs).
    if (std::hypot(to.x() - from.x(), to.y() - from.y()) < WG::k_lattice) {
      error = QStringLiteral("leg %1 (%2,%3 to %4,%5) is shorter than a wall piece")
                  .arg(leg)
                  .arg(from.x())
                  .arg(from.y())
                  .arg(to.x())
                  .arg(to.y());
      return {};
    }
    const float length =
        static_cast<float>(std::hypot(to.x() - from.x(), to.y() - from.y()));
    const QPointF dir = (to - from) / static_cast<qreal>(length);
    if (!towers) {
      add_run(from, to);
      continue;
    }
    // A tower at the leg's start; the next leg supplies the far corner, or
    // this one does when the wall is open and ends here.
    add_tower(from);
    const int spans =
        std::max(1, static_cast<int>(std::ceil(length / spec.tower_spacing)));
    QPointF previous = from;
    for (int span = 1; span <= spans; ++span) {
      const QPointF stop =
          span == spans ? to
                        : from + dir * static_cast<qreal>(
                                           WG::snap(length * static_cast<float>(span) /
                                                    static_cast<float>(spans)));
      add_run(previous + dir * static_cast<qreal>(k_tower_clear),
              stop - dir * static_cast<qreal>(k_tower_clear));
      if (span < spans) {
        add_tower(stop);
      }
      previous = stop;
    }
    if (!spec.closed && leg + 1 == legs) {
      add_tower(to);
    }
  }
  return plan;
}

auto wall_from_json(const QJsonObject& op, WallSpec& out, QString& error) -> bool {
  for (const QJsonValue value : op.value(QStringLiteral("points")).toArray()) {
    QPointF point;
    if (!point_of(value, point)) {
      error = QStringLiteral("'points' must be [x, z] pairs");
      return false;
    }
    out.points.append(point);
  }
  out.closed = op.value(QStringLiteral("closed")).toBool(out.closed);
  out.tower_spacing = static_cast<float>(
      op.value(QStringLiteral("tower_spacing")).toDouble(out.tower_spacing));
  out.id = op.value(QStringLiteral("id")).toString();
  read_owner(op, out.owner);
  return read_style(op, QStringLiteral("wall_segment"), out.style, error);
}

auto terrace_from_json(const QJsonObject& op,
                       TerraceSpec& out,
                       QString& error) -> bool {
  if (!point_of(op.value(QStringLiteral("from")), out.from) ||
      !point_of(op.value(QStringLiteral("to")), out.to)) {
    error = QStringLiteral("needs 'from': [x, z] and 'to': [x, z]");
    return false;
  }
  out.facing =
      static_cast<float>(op.value(QStringLiteral("facing")).toDouble(out.facing));
  out.type = op.value(QStringLiteral("type")).toString(out.type);
  out.id = op.value(QStringLiteral("id")).toString();
  for (const QJsonValue value : op.value(QStringLiteral("skip")).toArray()) {
    out.skip.append(value.toInt(-1));
  }
  read_owner(op, out.owner);
  return read_style(op, out.type, out.style, error);
}

auto block_from_json(const QJsonObject& op, BlockSpec& out, QString& error) -> bool {
  if (!point_of(op.value(QStringLiteral("origin")), out.origin)) {
    error = QStringLiteral("needs 'origin': [x, z], the block's low corner");
    return false;
  }
  const QJsonArray lots = op.value(QStringLiteral("lots")).toArray();
  if (lots.size() == 2) {
    out.lots_wide = lots[0].toInt(out.lots_wide);
    out.lots_deep = lots[1].toInt(out.lots_deep);
  }
  if (out.lots_wide < 2 || out.lots_deep < 0) {
    error = QStringLiteral("'lots': [wide, deep] needs at least 2 wide and 0 deep");
    return false;
  }
  out.rotation =
      static_cast<float>(op.value(QStringLiteral("rotation")).toDouble(out.rotation));
  out.alley_lot = op.value(QStringLiteral("alley")).toInt(out.alley_lot);
  out.type = op.value(QStringLiteral("type")).toString(out.type);
  out.id = op.value(QStringLiteral("id")).toString();
  read_owner(op, out.owner);
  return read_style(op, out.type, out.style, error);
}

auto paving_from_json(const QJsonObject& op, PavingSpec& out, QString& error) -> bool {
  const QJsonArray rect = op.value(QStringLiteral("rect")).toArray();
  if (rect.size() != 4) {
    error = QStringLiteral("needs 'rect': [x0, z0, x1, z1]");
    return false;
  }
  out.min = QPointF(rect[0].toDouble(), rect[1].toDouble());
  out.max = QPointF(rect[2].toDouble(), rect[3].toDouble());
  out.style = op.value(QStringLiteral("style")).toString(out.style);
  out.strip = static_cast<float>(op.value(QStringLiteral("strip")).toDouble(out.strip));
  return true;
}

} // namespace MapEditor::TownBlock
