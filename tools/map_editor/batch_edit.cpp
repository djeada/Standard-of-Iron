#include "batch_edit.h"

#include <QJsonValue>
#include <QPointF>

#include <array>
#include <memory>
#include <utility>

#include "element_edit_json.h"
#include "town_block.h"
#include "wall_geometry.h"

namespace MapEditor::BatchEdit {

namespace {

struct KindName {
  int kind;
  const char* name;
};

constexpr std::array<KindName, k_element_kind_count> k_kind_names{{
    {static_cast<int>(ElementKind::Terrain), "terrain"},
    {static_cast<int>(ElementKind::WorldProp), "prop"},
    {static_cast<int>(ElementKind::Linear), "linear"},
    {static_cast<int>(ElementKind::Structure), "structure"},
    {static_cast<int>(ElementKind::TroopSpawn), "spawn"},
    {static_cast<int>(ElementKind::UndeadZone), "undead_zone"},
    {static_cast<int>(ElementKind::WildlifeArea), "wildlife"},
    {static_cast<int>(ElementKind::Forest), "forest"},
}};

auto blank_snapshot(int kind) -> ElementSnapshot {
  switch (static_cast<ElementKind>(kind)) {
  case ElementKind::Terrain:
    return TerrainElement{};
  case ElementKind::WorldProp:
    return WorldPropElement{};
  case ElementKind::Linear:
    return LinearElement{};
  case ElementKind::Structure:
    return StructureElement{};
  case ElementKind::TroopSpawn:
    return TroopSpawnElement{};
  case ElementKind::UndeadZone:
    return UndeadZoneElement{};
  case ElementKind::WildlifeArea:
    return WildlifeAreaElement{};
  case ElementKind::Forest:
    return ForestElement{};
  }
  return {};
}

auto point_of(const QJsonValue& value, QPointF& out) -> bool {
  const QJsonArray pair = value.toArray();
  if (pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble()) {
    return false;
  }
  out = QPointF(pair[0].toDouble(), pair[1].toDouble());
  return true;
}

auto label_of(int kind, int index, const ElementSnapshot& snapshot) -> QString {
  return QStringLiteral("%1[%2] %3")
      .arg(kind_name(kind))
      .arg(index)
      .arg(ElementOps::display_name(snapshot));
}

auto resolve_target(const MapData& data,
                    const QJsonObject& op,
                    int& kind,
                    int& index,
                    QString& error) -> bool {
  kind = kind_from_name(op.value(QStringLiteral("kind")).toString());
  if (!ElementOps::is_valid_kind(kind)) {
    error = QStringLiteral("unknown kind '%1'")
                .arg(op.value(QStringLiteral("kind")).toString());
    return false;
  }
  if (op.contains(QStringLiteral("id"))) {
    const QString id = op.value(QStringLiteral("id")).toString();
    index = find_by_id(data, kind, id);
    if (index < 0) {
      error = QStringLiteral("no %1 with id '%2'").arg(kind_name(kind), id);
      return false;
    }
    return true;
  }
  if (!op.value(QStringLiteral("index")).isDouble()) {
    error = QStringLiteral("operation names neither 'id' nor 'index'");
    return false;
  }
  index = op.value(QStringLiteral("index")).toInt(-1);
  if (!ElementOps::index_valid(data, kind, index)) {
    error = QStringLiteral("%1 index %2 is out of range (count %3)")
                .arg(kind_name(kind))
                .arg(index)
                .arg(ElementOps::count(data, kind));
    return false;
  }
  return true;
}

auto rebuilt(const MapData& data,
             const ElementSnapshot& before,
             const QJsonObject& json,
             QStringList& notes) -> ElementSnapshot {
  auto result = ElementEditJson::from_json(
      before, json, data.linear_elements(), HillProjection::map_context(data));
  notes = result.notes;
  return result.element;
}

auto merged(QJsonObject base, const QJsonObject& fields) -> QJsonObject {
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    if (it.value().isNull()) {
      base.remove(it.key());
    } else {
      base[it.key()] = it.value();
    }
  }
  return base;
}

void apply_map_settings(MapData& data, const QJsonObject& op, Report& report) {
  if (op.contains(QStringLiteral("name"))) {
    data.set_name(op.value(QStringLiteral("name")).toString());
  }
  if (op.contains(QStringLiteral("biome"))) {
    data.set_biome(merged(data.biome(), op.value(QStringLiteral("biome")).toObject()));
  }
  if (op.contains(QStringLiteral("rain"))) {
    data.set_rain(merged(data.rain(), op.value(QStringLiteral("rain")).toObject()));
  }
  if (op.contains(QStringLiteral("environment"))) {
    data.set_environment(
        merged(data.environment(), op.value(QStringLiteral("environment")).toObject()));
  }
  if (op.contains(QStringLiteral("camera"))) {
    data.set_camera(
        merged(data.camera(), op.value(QStringLiteral("camera")).toObject()));
  }
  if (op.contains(QStringLiteral("victory"))) {
    data.set_victory(
        merged(data.victory(), op.value(QStringLiteral("victory")).toObject()));
  }
  if (op.contains(QStringLiteral("description"))) {
    data.set_description(op.value(QStringLiteral("description")).toString());
  }
  if (op.contains(QStringLiteral("max_troops_per_player"))) {
    data.set_max_troops_per_player(
        op.value(QStringLiteral("max_troops_per_player")).toInt());
  }
  ++report.applied;
  report.log.append(QStringLiteral("map settings updated"));
}

auto apply_root_field(MapData& data, const QJsonObject& op, QString& error) -> bool {
  const QString key = op.value(QStringLiteral("key")).toString();
  if (key.isEmpty()) {
    error = QStringLiteral("a root operation needs a key");
    return false;
  }
  if (MapData::is_modelled_root_key(key)) {
    error = QStringLiteral("'%1' is edited through its own kind, not as a root field")
                .arg(key);
    return false;
  }
  const bool has_value = op.contains(QStringLiteral("value"));
  const bool has_append = op.contains(QStringLiteral("append"));
  const bool has_remove = op.contains(QStringLiteral("remove_ids"));
  if (int(has_value) + int(has_append) + int(has_remove) != 1) {
    error = QStringLiteral("give exactly one of value, append or remove_ids");
    return false;
  }
  if (has_value) {
    return data.set_extra_root_field(key, op.value(QStringLiteral("value")));
  }

  const QJsonValue current = data.extra_root_field(key);
  if (!current.isUndefined() && !current.isArray()) {
    error = QStringLiteral("'%1' is not an array").arg(key);
    return false;
  }
  QJsonArray entries = current.toArray();
  const auto id_of = [](const QJsonValue& entry) {
    return entry.toObject().value(QStringLiteral("id")).toString();
  };

  if (has_remove) {
    QStringList wanted;
    for (const QJsonValue id : op.value(QStringLiteral("remove_ids")).toArray()) {
      wanted.append(id.toString());
    }
    QJsonArray kept;
    for (const QJsonValue entry : std::as_const(entries)) {
      if (!wanted.removeOne(id_of(entry))) {
        kept.append(entry);
      }
    }
    if (!wanted.isEmpty()) {
      error = QStringLiteral("'%1' has no entry with id %2")
                  .arg(key, wanted.join(QLatin1String(", ")));
      return false;
    }
    return data.set_extra_root_field(key, kept);
  }

  const QJsonValue addition = op.value(QStringLiteral("append"));
  const QJsonArray additions =
      addition.isArray() ? addition.toArray() : QJsonArray{addition};
  for (const QJsonValue entry : additions) {
    const QString id = id_of(entry);
    if (!id.isEmpty()) {
      for (const QJsonValue existing : std::as_const(entries)) {
        if (id_of(existing) == id) {
          error = QStringLiteral("'%1' already has an entry with id %2").arg(key, id);
          return false;
        }
      }
    }
    entries.append(entry);
  }
  return data.set_extra_root_field(key, entries);
}

} // namespace

auto kind_name(int kind) -> QString {
  for (const auto& entry : k_kind_names) {
    if (entry.kind == kind) {
      return QString::fromLatin1(entry.name);
    }
  }
  return QStringLiteral("unknown");
}

auto kind_from_name(const QString& name) -> int {
  const QString wanted = name.trimmed().toLower();
  for (const auto& entry : k_kind_names) {
    if (wanted == QLatin1String(entry.name)) {
      return entry.kind;
    }
  }
  return -1;
}

auto element_json(const ElementSnapshot& snapshot) -> QJsonObject {
  const auto document = ElementEditJson::to_document(snapshot);
  return document.has_value() ? document->json : QJsonObject{};
}

auto inventory(const MapData& data, int only_kind, bool include_json) -> QJsonArray {
  QJsonArray rows;
  for (const auto& entry : k_kind_names) {
    if (only_kind >= 0 && entry.kind != only_kind) {
      continue;
    }
    const int total = ElementOps::count(data, entry.kind);
    for (int index = 0; index < total; ++index) {
      const ElementSnapshot snapshot = ElementOps::snapshot(data, entry.kind, index);
      const QJsonObject json = element_json(snapshot);
      QJsonObject row;
      row[QStringLiteral("kind")] = QString::fromLatin1(entry.name);
      row[QStringLiteral("index")] = index;
      row[QStringLiteral("type")] = ElementOps::type_name(snapshot);
      row[QStringLiteral("summary")] = ElementOps::summary(snapshot);
      if (json.contains(QStringLiteral("id"))) {
        row[QStringLiteral("id")] = json.value(QStringLiteral("id"));
      }
      if (const auto position = ElementOps::position(snapshot)) {
        row[QStringLiteral("x")] = position->x();
        row[QStringLiteral("z")] = position->y();
      }
      if (ElementOps::is_generated(snapshot)) {
        row[QStringLiteral("generated")] = true;
      }
      if (include_json) {
        row[QStringLiteral("json")] = json;
      }
      rows.append(row);
    }
  }
  return rows;
}

auto summary(const MapData& data) -> QJsonObject {
  QJsonObject out;
  out[QStringLiteral("name")] = data.name();
  out[QStringLiteral("width")] = data.grid().width;
  out[QStringLiteral("height")] = data.grid().height;
  out[QStringLiteral("ground_type")] =
      data.biome().value(QStringLiteral("ground_type")).toString();
  QJsonObject counts;
  for (const auto& entry : k_kind_names) {
    QJsonObject by_type;
    const int total = ElementOps::count(data, entry.kind);
    for (int index = 0; index < total; ++index) {
      const QString type =
          ElementOps::type_name(ElementOps::snapshot(data, entry.kind, index));
      by_type[type] = by_type.value(type).toInt() + 1;
    }
    by_type[QStringLiteral("_total")] = total;
    counts[QString::fromLatin1(entry.name)] = by_type;
  }
  out[QStringLiteral("elements")] = counts;
  return out;
}

auto find_by_id(const MapData& data, int kind, const QString& id) -> int {
  const int total = ElementOps::count(data, kind);
  for (int index = 0; index < total; ++index) {
    const QJsonObject json = element_json(ElementOps::snapshot(data, kind, index));
    if (json.value(QStringLiteral("id")).toString() == id) {
      return index;
    }
  }
  return -1;
}

auto apply(MapData& data, const QJsonArray& operations) -> Report {
  Report report;
  int step = 0;
  for (const QJsonValue value : operations) {
    ++step;
    const QJsonObject op = value.toObject();
    const QString verb = op.value(QStringLiteral("op")).toString();
    auto fail = [&](const QString& message) {
      report.errors.append(
          QStringLiteral("step %1 (%2): %3").arg(step).arg(verb, message));
    };

    if (verb == QLatin1String("map")) {
      apply_map_settings(data, op, report);
      continue;
    }

    if (verb == QLatin1String("root")) {
      QString error;
      if (apply_root_field(data, op, error)) {
        ++report.applied;
        report.log.append(QStringLiteral("root field %1 updated")
                              .arg(op.value(QStringLiteral("key")).toString()));
      } else {
        fail(error);
      }
      continue;
    }

    if (verb == QLatin1String("terrace") || verb == QLatin1String("block")) {
      QVector<StructureElement> houses;
      QString error;
      if (verb == QLatin1String("terrace")) {
        TownBlock::TerraceSpec spec;
        if (!TownBlock::terrace_from_json(op, spec, error)) {
          fail(error);
          continue;
        }
        houses = TownBlock::terrace(spec);
      } else {
        TownBlock::BlockSpec spec;
        if (!TownBlock::block_from_json(op, spec, error)) {
          fail(error);
          continue;
        }
        houses = TownBlock::block(spec);
      }
      if (houses.isEmpty()) {
        fail(QStringLiteral("the run is shorter than one lot"));
        continue;
      }
      for (const StructureElement& house : houses) {
        data.execute_command(std::make_unique<AddStructureCmd>(&data, house));
      }
      ++report.applied;
      report.log.append(QStringLiteral("%1 %2: %3 houses")
                            .arg(verb, op.value(QStringLiteral("id")).toString())
                            .arg(houses.size()));
      continue;
    }

    if (verb == QLatin1String("wall")) {
      TownBlock::WallSpec spec;
      QString error;
      if (!TownBlock::wall_from_json(op, spec, error)) {
        fail(error);
        continue;
      }
      const TownBlock::WallPlan plan = TownBlock::wall(spec, error);
      if (plan.runs.isEmpty()) {
        fail(error.isEmpty() ? QStringLiteral("the wall has no run") : error);
        continue;
      }
      for (const LinearElement& run : plan.runs) {
        data.execute_command(std::make_unique<AddLinearCmd>(&data, run));
      }
      for (const StructureElement& tower : plan.towers) {
        data.execute_command(std::make_unique<AddStructureCmd>(&data, tower));
      }
      ++report.applied;
      report.log.append(QStringLiteral("wall %1: %2 runs, %3 towers")
                            .arg(spec.id)
                            .arg(plan.runs.size())
                            .arg(plan.towers.size()));
      continue;
    }

    if (verb == QLatin1String("gate")) {

      QPointF at;
      if (!point_of(op.value(QStringLiteral("at")), at)) {
        fail(QStringLiteral("needs 'at': [x, z]"));
        continue;
      }
      namespace WG = WallGeometry;
      const WG::GatePlacement plan = WG::plan_gate(data.linear_elements(), at);
      if (plan.wall_index < 0) {
        fail(QStringLiteral("no wall run within %1 cells of %2, %3")
                 .arg(WG::k_gate_snap_reach)
                 .arg(at.x())
                 .arg(at.y()));
        continue;
      }
      const LinearElement run = data.linear_elements()[plan.wall_index];
      StructureElement gate;
      gate.type = QStringLiteral("wall_gate");
      gate.x = plan.x;
      gate.z = plan.z;
      gate.rotation = plan.rotation;
      gate.player_id = run.player_id;
      gate.nation = run.nation;
      gate.max_population = 100;

      const QJsonValue wall_style = run.extra_fields.value(QStringLiteral("style"));
      if (wall_style.isString() && !wall_style.toString().isEmpty()) {
        gate.extra_fields[QStringLiteral("style")] = wall_style;
      }
      if (const QString id = op.value(QStringLiteral("id")).toString(); !id.isEmpty()) {
        gate.extra_fields[QStringLiteral("id")] = id;
        gate.extra_fields[QStringLiteral("group")] = id;
      }
      data.execute_command(
          std::make_unique<RemoveLinearCmd>(&data, plan.wall_index, run));
      for (const bool keep_low : {true, false}) {
        if (auto piece =
                WG::trim_run_to_gate(run, plan.horizontal, plan.centre, keep_low)) {
          data.execute_command(std::make_unique<AddLinearCmd>(&data, *piece));
        }
      }
      data.execute_command(std::make_unique<AddStructureCmd>(&data, gate));
      ++report.applied;
      report.log.append(
          QStringLiteral("gate at %1, %2 across %3")
              .arg(gate.x)
              .arg(gate.z)
              .arg(plan.horizontal ? QStringLiteral("x") : QStringLiteral("z")));
      continue;
    }

    if (verb == QLatin1String("paving")) {
      TownBlock::PavingSpec spec;
      QString error;
      if (!TownBlock::paving_from_json(op, spec, error)) {
        fail(error);
        continue;
      }
      const QVector<LinearElement> roads = TownBlock::paving(spec);
      if (roads.isEmpty()) {
        fail(QStringLiteral("the rectangle is too small to pave"));
        continue;
      }
      for (const LinearElement& road : roads) {
        data.execute_command(std::make_unique<AddLinearCmd>(&data, road));
      }
      ++report.applied;
      report.log.append(QStringLiteral("paved in %1 strips").arg(roads.size()));
      continue;
    }

    if (verb == QLatin1String("add")) {
      const int kind = kind_from_name(op.value(QStringLiteral("kind")).toString());
      if (!ElementOps::is_valid_kind(kind)) {
        fail(QStringLiteral("unknown kind"));
        continue;
      }
      QStringList notes;
      const ElementSnapshot made = rebuilt(data,
                                           blank_snapshot(kind),
                                           op.value(QStringLiteral("json")).toObject(),
                                           notes);
      if (ElementOps::kind_of(made) != kind) {
        fail(QStringLiteral("the JSON does not describe a %1").arg(kind_name(kind)));
        continue;
      }
      data.execute_command(ElementOps::make_add(data, made));
      ++report.applied;
      report.log.append(
          QStringLiteral("added %1")
              .arg(label_of(kind, ElementOps::count(data, kind) - 1, made)));
      continue;
    }

    int kind = -1;
    int index = -1;
    QString error;
    if (!resolve_target(data, op, kind, index, error)) {
      fail(error);
      continue;
    }
    const ElementSnapshot before = ElementOps::snapshot(data, kind, index);

    if (verb == QLatin1String("remove")) {
      data.execute_command(ElementOps::make_remove(data, kind, index));
      ++report.applied;
      report.log.append(
          QStringLiteral("removed %1").arg(label_of(kind, index, before)));
      continue;
    }

    ElementSnapshot after = before;
    QStringList notes;
    if (verb == QLatin1String("move")) {
      QPointF point;
      if (point_of(op.value(QStringLiteral("to")), point)) {
        after = ElementOps::moved_to(before, point);
      } else if (point_of(op.value(QStringLiteral("by")), point)) {
        after = ElementOps::translated(before, point);
      } else {
        fail(QStringLiteral("needs 'to': [x, z] or 'by': [dx, dz]"));
        continue;
      }
    } else if (verb == QLatin1String("set")) {
      after = rebuilt(
          data,
          before,
          merged(element_json(before), op.value(QStringLiteral("fields")).toObject()),
          notes);
    } else if (verb == QLatin1String("replace")) {
      after = rebuilt(data, before, op.value(QStringLiteral("json")).toObject(), notes);
    } else {
      fail(QStringLiteral("unknown operation"));
      continue;
    }

    if (ElementOps::kind_of(after) != kind) {
      fail(QStringLiteral("the edit did not produce a %1").arg(kind_name(kind)));
      continue;
    }
    data.execute_command(ElementOps::make_update(
        data, index, before, after, verb + QLatin1Char(' ') + kind_name(kind)));
    ++report.applied;
    report.log.append(QStringLiteral("%1 %2%3")
                          .arg(verb, label_of(kind, index, after))
                          .arg(notes.isEmpty() ? QString()
                                               : QStringLiteral(" (%1)").arg(notes.join(
                                                     QStringLiteral("; ")))));
  }
  return report;
}

} // namespace MapEditor::BatchEdit
