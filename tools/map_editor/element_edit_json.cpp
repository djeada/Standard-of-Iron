#include "element_edit_json.h"

#include <QJsonArray>

#include <cmath>
#include <utility>

#include "map_json_keys.h"

namespace MapEditor::ElementEditJson {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

auto is_mountain(const QString& type) -> bool {
  return type.trimmed().compare(QStringLiteral("mountain"), Qt::CaseInsensitive) == 0;
}

void append_extra_fields(QJsonObject& json, const QJsonObject& extra_fields) {
  for (auto it = extra_fields.begin(); it != extra_fields.end(); ++it) {
    json[it.key()] = it.value();
  }
}

auto extra_fields_of(const QJsonObject& json,
                     const QStringList& known_keys) -> QJsonObject {
  QJsonObject extra;
  for (auto it = json.begin(); it != json.end(); ++it) {
    if (!known_keys.contains(it.key())) {
      extra[it.key()] = it.value();
    }
  }
  return extra;
}

auto read_float(const QJsonObject& json, const QString& key, double fallback) -> float {
  return static_cast<float>(json[key].toDouble(fallback));
}

auto terrain_document(const TerrainElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json = terrain_element_to_json(elem);
  doc.json[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
  if (is_mountain(elem.type)) {
    doc.json.remove(MapJsonKeys::entrances);
  }
  const QString type = elem.type.trimmed().toLower();
  doc.title = "Edit Terrain: " + elem.type;
  doc.schema_sub_type = elem.type;
  doc.hill_projection =
      type == QStringLiteral("hill") || type == QStringLiteral("mountain");
  return doc;
}

auto world_prop_document(const WorldPropElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json[MapJsonKeys::type] = elem.type;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  if (elem.type == QStringLiteral("firecamp")) {
    doc.json[MapJsonKeys::intensity] = static_cast<double>(elem.intensity);
    doc.json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
    doc.json[MapJsonKeys::persistent] = elem.persistent;
  } else {
    doc.json[MapJsonKeys::scale] = static_cast<double>(elem.scale);
    doc.json[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
  }
  append_extra_fields(doc.json, elem.extra_fields);
  doc.title = "Edit Prop: " + prettify_identifier(elem.type);
  doc.schema_sub_type = elem.type;
  return doc;
}

auto linear_document(const LinearElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json[MapJsonKeys::type] = elem.type;
  doc.json[MapJsonKeys::start] = QJsonArray{static_cast<double>(elem.start.x()),
                                            static_cast<double>(elem.start.y())};
  doc.json[MapJsonKeys::end] =
      QJsonArray{static_cast<double>(elem.end.x()), static_cast<double>(elem.end.y())};
  doc.json[MapJsonKeys::width] = static_cast<double>(elem.width);
  if (!elem.waypoints.isEmpty()) {
    doc.json[MapJsonKeys::waypoints] = waypoints_to_json(elem.waypoints);
  }
  if (elem.type == QStringLiteral("bridge")) {
    doc.json[MapJsonKeys::height] = static_cast<double>(elem.height);
  }
  if (elem.type == QStringLiteral("road") && !elem.style.isEmpty()) {
    doc.json[MapJsonKeys::style] = elem.style;
  }
  if (elem.type == QStringLiteral("wall")) {
    doc.json[MapJsonKeys::player_id] = elem.player_id;
    if (!elem.nation.isEmpty()) {
      doc.json[MapJsonKeys::nation] = elem.nation;
    }
  }
  append_extra_fields(doc.json, elem.extra_fields);
  doc.title = "Edit " + elem.type;
  doc.schema_sub_type = elem.type;
  return doc;
}

auto structure_document(const StructureElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json[MapJsonKeys::type] = elem.type;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  doc.json[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
  doc.json[MapJsonKeys::player_id] = elem.player_id;
  if (elem.max_population != 100) {
    doc.json[MapJsonKeys::max_population] = elem.max_population;
  }
  if (!elem.nation.isEmpty()) {
    doc.json[MapJsonKeys::nation] = elem.nation;
  }
  append_extra_fields(doc.json, elem.extra_fields);
  doc.title = "Edit " + elem.type;
  doc.schema_sub_type = elem.type;
  return doc;
}

auto troop_document(const TroopSpawnElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json[MapJsonKeys::type] = elem.type;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  if (elem.player_id >= 0) {
    doc.json[MapJsonKeys::player_id] = elem.player_id;
  }
  if (elem.max_population >= 0) {
    doc.json[MapJsonKeys::max_population] = elem.max_population;
  }
  if (!elem.nation.isEmpty()) {
    doc.json[MapJsonKeys::nation] = elem.nation;
  }
  if (!elem.behavior.isEmpty()) {
    doc.json[MapJsonKeys::behavior] = elem.behavior;
  }
  if (elem.guard_radius != 10.0F) {
    doc.json[MapJsonKeys::guard_radius] = static_cast<double>(elem.guard_radius);
  }
  if (!elem.patrol_waypoints.isEmpty()) {
    doc.json[MapJsonKeys::patrol_waypoints] = elem.patrol_waypoints;
  }
  append_extra_fields(doc.json, elem.extra_fields);
  doc.title = "Edit Troop: " + prettify_identifier(elem.type);
  doc.schema_sub_type = elem.type;
  return doc;
}

auto undead_zone_document(const UndeadZoneElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json["id"] = elem.id;
  doc.json["anchor_type"] = elem.anchor_type;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  doc.json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
  doc.json["leash_radius"] = static_cast<double>(elem.leash_radius);
  doc.json["owner_id"] = elem.owner_id;
  doc.json["team_id"] = elem.team_id;
  if (!elem.awaken_on.isEmpty()) {
    doc.json["awaken_on"] = elem.awaken_on;
  }
  if (!elem.waves.isEmpty()) {
    doc.json["waves"] = elem.waves;
  }
  if (!elem.clear_reward.isEmpty()) {
    doc.json["clear_reward"] = elem.clear_reward;
  }
  doc.title = "Edit Undead Zone: " + elem.id;
  doc.schema_sub_type = elem.anchor_type;
  return doc;
}

auto wildlife_document(const WildlifeAreaElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json["species"] = elem.species;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  doc.json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
  doc.title = "Edit Wildlife Range: " + wildlife_species_label(elem.species);
  return doc;
}

auto forest_document(const ForestElement& elem) -> EditDocument {
  EditDocument doc;
  doc.json["id"] = elem.id;
  doc.json[MapJsonKeys::x] = static_cast<double>(elem.x);
  doc.json[MapJsonKeys::z] = static_cast<double>(elem.z);
  doc.json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
  append_extra_fields(doc.json, elem.extra_fields);
  doc.title =
      "Edit Forest: " + (elem.id.isEmpty() ? QStringLiteral("forest") : elem.id);
  return doc;
}

auto terrain_from_json(const QJsonObject& json) -> EditResult {
  TerrainElement elem = terrain_element_from_json(json);
  if (is_mountain(elem.type)) {
    elem.entrances = QJsonArray{};
  }
  return {std::move(elem), QStringLiteral("Edit terrain"), {}};
}

auto world_prop_from_json(const QJsonObject& json) -> EditResult {
  WorldPropElement elem;
  elem.type = json[MapJsonKeys::type].toString(QStringLiteral("firecamp"));
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.scale = read_float(json, MapJsonKeys::scale, 1.0);
  elem.rotation = read_float(json, MapJsonKeys::rotation, 0.0);
  elem.intensity = read_float(json, MapJsonKeys::intensity, 1.0);
  elem.radius = read_float(json, MapJsonKeys::radius, 3.0);
  elem.persistent = json[MapJsonKeys::persistent].toBool(true);
  elem.extra_fields = extra_fields_of(json,
                                      {MapJsonKeys::type,
                                       MapJsonKeys::x,
                                       MapJsonKeys::z,
                                       MapJsonKeys::scale,
                                       MapJsonKeys::rotation,
                                       MapJsonKeys::intensity,
                                       MapJsonKeys::radius,
                                       MapJsonKeys::persistent});
  QString description = "Edit " + elem.type;
  return {std::move(elem), std::move(description), {}};
}

auto linear_from_json(const QJsonObject& json,
                      const LinearElement& before,
                      const QVector<LinearElement>& linear_elements) -> EditResult {
  LinearElement elem;
  elem.type = json[MapJsonKeys::type].toString();
  elem.structure_order = before.structure_order;

  const QJsonArray start = json[MapJsonKeys::start].toArray();
  const QJsonArray end = json[MapJsonKeys::end].toArray();
  if (start.size() >= 2 && end.size() >= 2) {
    elem.start = QVector2D(static_cast<float>(start[0].toDouble()),
                           static_cast<float>(start[1].toDouble()));
    elem.end = QVector2D(static_cast<float>(end[0].toDouble()),
                         static_cast<float>(end[1].toDouble()));
  }
  elem.width = read_float(json, MapJsonKeys::width, 3.0);
  elem.height = read_float(json, MapJsonKeys::height, 0.5);
  elem.style = json[MapJsonKeys::style].toString(QStringLiteral("default"));
  elem.player_id = json[MapJsonKeys::player_id].toInt(0);
  elem.nation = json[MapJsonKeys::nation].toString();
  if (supports_waypoints(elem.type)) {
    elem.waypoints = waypoints_from_json(json[MapJsonKeys::waypoints].toArray());
  }
  elem.extra_fields = extra_fields_of(json,
                                      {MapJsonKeys::type,
                                       MapJsonKeys::start,
                                       MapJsonKeys::end,
                                       MapJsonKeys::width,
                                       MapJsonKeys::height,
                                       MapJsonKeys::style,
                                       MapJsonKeys::player_id,
                                       MapJsonKeys::nation,
                                       MapJsonKeys::waypoints});

  QStringList notes;
  if (elem.type == QStringLiteral("bridge")) {
    if (elem.height < k_min_bridge_height) {
      elem.height = k_min_bridge_height;
      notes << QStringLiteral("Bridge height raised to minimum %1.")
                   .arg(static_cast<double>(k_min_bridge_height), 0, 'f', 2);
    }
    const float required_width =
        compute_min_bridge_width(elem.start, elem.end, linear_elements);
    if (elem.width < required_width) {
      notes << QStringLiteral("Bridge width raised to %1 to span crossed river(s) from "
                              "bank to bank.")
                   .arg(static_cast<double>(required_width), 0, 'f', 2);
      elem.width = required_width;
    }
  }
  if (elem.type == QStringLiteral("wall")) {
    const QVector2D delta = elem.end - elem.start;
    if (std::abs(delta.x()) >= std::abs(delta.y())) {
      elem.end.setY(elem.start.y());
    } else {
      elem.end.setX(elem.start.x());
    }
  }
  QString description = "Edit " + elem.type;
  return {std::move(elem), std::move(description), std::move(notes)};
}

auto structure_from_json(const QJsonObject& json,
                         const StructureElement& before) -> EditResult {
  StructureElement elem;
  elem.type = json[MapJsonKeys::type].toString();
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.rotation = read_float(json, MapJsonKeys::rotation, 0.0);
  elem.player_id = json[MapJsonKeys::player_id].toInt(0);
  elem.max_population = json[MapJsonKeys::max_population].toInt(100);
  elem.nation = json[MapJsonKeys::nation].toString();
  elem.spawn_order = before.spawn_order;
  elem.structure_order = before.structure_order;
  elem.extra_fields = extra_fields_of(json,
                                      {MapJsonKeys::type,
                                       MapJsonKeys::x,
                                       MapJsonKeys::z,
                                       MapJsonKeys::rotation,
                                       MapJsonKeys::player_id,
                                       MapJsonKeys::max_population,
                                       MapJsonKeys::nation});
  QString description = "Edit " + elem.type;
  return {std::move(elem), std::move(description), {}};
}

auto troop_from_json(const QJsonObject& json,
                     const TroopSpawnElement& before) -> EditResult {
  TroopSpawnElement elem;
  elem.type = json[MapJsonKeys::type].toString();
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.player_id = json.contains(MapJsonKeys::player_id) &&
                           !json.value(MapJsonKeys::player_id).isNull()
                       ? json[MapJsonKeys::player_id].toInt(-1)
                       : -1;
  elem.max_population = json.contains(MapJsonKeys::max_population)
                            ? json[MapJsonKeys::max_population].toInt(100)
                            : -1;
  elem.nation = json[MapJsonKeys::nation].toString();
  elem.behavior = json[MapJsonKeys::behavior].toString();
  elem.guard_radius = read_float(json, MapJsonKeys::guard_radius, 10.0);
  elem.patrol_waypoints = json[MapJsonKeys::patrol_waypoints].toArray();
  elem.spawn_order = before.spawn_order;
  elem.extra_fields = extra_fields_of(json,
                                      {MapJsonKeys::type,
                                       MapJsonKeys::x,
                                       MapJsonKeys::z,
                                       MapJsonKeys::player_id,
                                       MapJsonKeys::max_population,
                                       MapJsonKeys::nation,
                                       MapJsonKeys::behavior,
                                       MapJsonKeys::guard_radius,
                                       MapJsonKeys::patrol_waypoints});
  QString description = "Edit " + elem.type;
  return {std::move(elem), std::move(description), {}};
}

auto undead_zone_from_json(const QJsonObject& json) -> EditResult {
  UndeadZoneElement elem;
  elem.id = json["id"].toString();
  elem.anchor_type = json["anchor_type"].toString(QStringLiteral("magic_shrine"));
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.radius = read_float(json, MapJsonKeys::radius, 8.0);
  elem.leash_radius = read_float(json, QStringLiteral("leash_radius"), 14.0);
  elem.owner_id = json["owner_id"].toInt(99);
  elem.team_id = json["team_id"].toInt(99);
  elem.awaken_on = json["awaken_on"].toArray();
  elem.waves = json["waves"].toArray();
  elem.clear_reward = json["clear_reward"].toObject();
  return {std::move(elem), QStringLiteral("Edit undead zone"), {}};
}

auto wildlife_from_json(const QJsonObject& json) -> EditResult {
  WildlifeAreaElement elem;
  elem.species = json["species"].toString(QStringLiteral("sheep"));
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.radius = read_float(json, MapJsonKeys::radius, 14.0);
  return {std::move(elem), QStringLiteral("Edit wildlife range"), {}};
}

auto forest_from_json(const QJsonObject& json) -> EditResult {
  ForestElement elem;
  elem.id = json["id"].toString();
  elem.x = read_float(json, MapJsonKeys::x, 0.0);
  elem.z = read_float(json, MapJsonKeys::z, 0.0);
  elem.radius = read_float(json, MapJsonKeys::radius, 12.0);
  elem.extra_fields = extra_fields_of(
      json,
      {QStringLiteral("id"), MapJsonKeys::x, MapJsonKeys::z, MapJsonKeys::radius});
  return {std::move(elem), QStringLiteral("Edit forest"), {}};
}

} // namespace

auto to_document(const ElementSnapshot& element) -> std::optional<EditDocument> {
  return std::visit(
      Overloaded{
          [](std::monostate) -> std::optional<EditDocument> { return {}; },
          [](const TerrainElement& e) -> std::optional<EditDocument> {
            return terrain_document(e);
          },
          [](const WorldPropElement& e) -> std::optional<EditDocument> {
            return world_prop_document(e);
          },
          [](const LinearElement& e) -> std::optional<EditDocument> {
            return linear_document(e);
          },
          [](const StructureElement& e) -> std::optional<EditDocument> {
            return structure_document(e);
          },
          [](const TroopSpawnElement& e) -> std::optional<EditDocument> {
            return troop_document(e);
          },
          [](const UndeadZoneElement& e) -> std::optional<EditDocument> {
            return undead_zone_document(e);
          },
          [](const WildlifeAreaElement& e) -> std::optional<EditDocument> {
            return wildlife_document(e);
          },
          [](const ForestElement& e) -> std::optional<EditDocument> {
            return forest_document(e);
          },
      },
      element);
}

auto from_json(const ElementSnapshot& before,
               const QJsonObject& json,
               const QVector<LinearElement>& linear_elements) -> EditResult {
  return std::visit(
      Overloaded{
          [](std::monostate) { return EditResult{}; },
          [&](const TerrainElement&) { return terrain_from_json(json); },
          [&](const WorldPropElement&) { return world_prop_from_json(json); },
          [&](const LinearElement& e) {
            return linear_from_json(json, e, linear_elements);
          },
          [&](const StructureElement& e) { return structure_from_json(json, e); },
          [&](const TroopSpawnElement& e) { return troop_from_json(json, e); },
          [&](const UndeadZoneElement&) { return undead_zone_from_json(json); },
          [&](const WildlifeAreaElement&) { return wildlife_from_json(json); },
          [&](const ForestElement&) { return forest_from_json(json); },
      },
      before);
}

auto prettify_identifier(const QString& value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString& part : parts) {
    part[0] = part[0].toUpper();
  }
  return parts.join(QLatin1Char(' '));
}

} // namespace MapEditor::ElementEditJson
