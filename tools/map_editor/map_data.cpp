#include "map_data.h"
#include "map_json_keys.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace MapEditor {

namespace {

constexpr auto coord_system_key = MapJsonKeys::coord_system;
constexpr auto legacy_coord_system_key = "coordSystem";
constexpr auto max_troops_key = MapJsonKeys::max_troops_per_player;
constexpr auto legacy_max_troops_key = "maxTroopsPerPlayer";

auto readStringWithFallback(const QJsonObject &obj, const QString &primary_key,
                            const QString &fallback_key,
                            const QString &default_value) -> QString {
  if (obj.contains(primary_key)) {
    return obj.value(primary_key).toString(default_value);
  }
  if (obj.contains(fallback_key)) {
    return obj.value(fallback_key).toString(default_value);
  }
  return default_value;
}

auto readIntWithFallback(const QJsonObject &obj, const QString &primary_key,
                         const QString &fallback_key,
                         int default_value) -> int {
  if (obj.contains(primary_key)) {
    return obj.value(primary_key).toInt(default_value);
  }
  if (obj.contains(fallback_key)) {
    return obj.value(fallback_key).toInt(default_value);
  }
  return default_value;
}

auto readLinearPoint(const QJsonArray &point, QVector2D &out) -> bool {
  if (point.size() < 2) {
    return false;
  }

  out = QVector2D(static_cast<float>(point[0].toDouble()),
                  static_cast<float>(point[1].toDouble()));
  return true;
}

auto applyLinearEndpoints(const QJsonObject &obj, LinearElement &elem) -> void {
  if (obj.contains(MapJsonKeys::waypoints) &&
      obj.value(MapJsonKeys::waypoints).isArray()) {
    const QJsonArray waypoints = obj.value(MapJsonKeys::waypoints).toArray();
    if (waypoints.size() >= 2 && waypoints.first().isArray() &&
        waypoints.last().isArray()) {
      QVector2D waypoint_start;
      QVector2D waypoint_end;
      if (readLinearPoint(waypoints.first().toArray(), waypoint_start) &&
          readLinearPoint(waypoints.last().toArray(), waypoint_end)) {
        elem.start = waypoint_start;
        elem.end = waypoint_end;
        return;
      }
    }
  }

  if (obj.contains(MapJsonKeys::start) &&
      obj.value(MapJsonKeys::start).isArray()) {
    readLinearPoint(obj.value(MapJsonKeys::start).toArray(), elem.start);
  }
  if (obj.contains(MapJsonKeys::end) && obj.value(MapJsonKeys::end).isArray()) {
    readLinearPoint(obj.value(MapJsonKeys::end).toArray(), elem.end);
  }
}

auto syncLinearWaypointsWithEndpoints(LinearElement &elem) -> void {
  if (!elem.extra_fields.contains(MapJsonKeys::waypoints) ||
      !elem.extra_fields.value(MapJsonKeys::waypoints).isArray()) {
    return;
  }

  QJsonArray waypoints =
      elem.extra_fields.value(MapJsonKeys::waypoints).toArray();
  if (waypoints.size() < 2) {
    return;
  }

  waypoints[0] = QJsonArray{static_cast<double>(elem.start.x()),
                            static_cast<double>(elem.start.y())};
  waypoints[waypoints.size() - 1] = QJsonArray{
      static_cast<double>(elem.end.x()), static_cast<double>(elem.end.y())};
  elem.extra_fields[MapJsonKeys::waypoints] = waypoints;
}

auto copyExtraFields(const QJsonObject &source,
                     const QStringList &known_keys) -> QJsonObject {
  QJsonObject extra_fields;
  for (const QString &key : source.keys()) {
    if (!known_keys.contains(key)) {
      extra_fields[key] = source[key];
    }
  }
  return extra_fields;
}

} // namespace

MapData::MapData(QObject *parent) : QObject(parent) { clear(); }

void MapData::clear() {
  m_name = "New Map";
  m_description.clear();
  m_coord_system = "grid";
  m_max_troops_per_player = 2000;

  m_grid = GridSettings{100, 100, 1.0F};

  m_terrain.clear();
  m_world_props.clear();
  m_linear_elements.clear();
  m_structures.clear();

  m_biome = QJsonObject();
  m_camera = QJsonObject();
  m_spawns = QJsonArray();
  m_victory = QJsonObject();
  m_rain = QJsonObject();
  m_extra_root_fields = QJsonObject();

  m_undo_stack.clear();
  m_redo_stack.clear();

  set_modified(false);
  emit data_changed();
  emit undo_redo_changed();
}

void MapData::set_name(const QString &name) {
  if (m_name != name) {
    m_name = name;
    set_modified(true);
    emit data_changed();
  }
}

void MapData::set_grid(const GridSettings &grid) {
  m_grid = grid;
  set_modified(true);
  emit data_changed();
}

void MapData::set_modified(bool modified) {
  if (m_modified != modified) {
    m_modified = modified;
    emit modified_changed(modified);
  }
}

void MapData::execute_command(std::unique_ptr<Command> cmd) {
  if (!cmd) {
    return;
  }
  cmd->execute();
  m_undo_stack.push_back(std::move(cmd));

  m_redo_stack.clear();
  set_modified(true);
  emit undo_redo_changed();
}

void MapData::undo() {
  if (m_undo_stack.empty()) {
    return;
  }
  auto cmd = std::move(m_undo_stack.back());
  m_undo_stack.pop_back();
  cmd->undo();
  m_redo_stack.push_back(std::move(cmd));
  set_modified(true);
  emit data_changed();
  emit undo_redo_changed();
}

void MapData::redo() {
  if (m_redo_stack.empty()) {
    return;
  }
  auto cmd = std::move(m_redo_stack.back());
  m_redo_stack.pop_back();
  cmd->execute();
  m_undo_stack.push_back(std::move(cmd));
  set_modified(true);
  emit data_changed();
  emit undo_redo_changed();
}

bool MapData::load_from_json(const QString &file_path, QString *out_error) {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (out_error != nullptr) {
      *out_error = file.errorString();
    }
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    if (out_error != nullptr) {
      if (error.error != QJsonParseError::NoError) {
        *out_error = QString("JSON parse error at byte %1: %2")
                         .arg(error.offset)
                         .arg(error.errorString());
      } else {
        *out_error = "Map JSON root must be an object.";
      }
    }
    return false;
  }

  QJsonObject root = doc.object();
  const QStringList known_root_keys = {
      MapJsonKeys::name,       MapJsonKeys::description, coord_system_key,
      legacy_coord_system_key, max_troops_key,           legacy_max_troops_key,
      MapJsonKeys::grid,       MapJsonKeys::biome,       MapJsonKeys::camera,
      MapJsonKeys::spawns,     MapJsonKeys::victory,     MapJsonKeys::rain,
      MapJsonKeys::terrain,    MapJsonKeys::world_props, MapJsonKeys::firecamps,
      MapJsonKeys::rivers,     MapJsonKeys::roads,       MapJsonKeys::bridges};

  m_extra_root_fields = copyExtraFields(root, known_root_keys);

  m_name = root[MapJsonKeys::name].toString("Untitled Map");
  m_description = root[MapJsonKeys::description].toString();
  m_coord_system = readStringWithFallback(root, coord_system_key,
                                          legacy_coord_system_key, "grid")
                       .trimmed()
                       .toLower();
  m_max_troops_per_player =
      readIntWithFallback(root, max_troops_key, legacy_max_troops_key, 2000);

  if (root.contains(MapJsonKeys::grid)) {
    const QJsonObject grid_obj = root[MapJsonKeys::grid].toObject();
    m_grid.width = grid_obj[MapJsonKeys::width].toInt(100);
    m_grid.height = grid_obj[MapJsonKeys::height].toInt(100);
    m_grid.tile_size =
        static_cast<float>(grid_obj[MapJsonKeys::tile_size].toDouble(1.0));
  }

  m_biome = root[MapJsonKeys::biome].toObject();
  m_camera = root[MapJsonKeys::camera].toObject();
  m_spawns = root[MapJsonKeys::spawns].toArray();
  m_victory = root[MapJsonKeys::victory].toObject();
  m_rain = root[MapJsonKeys::rain].toObject();

  m_terrain.clear();
  m_world_props.clear();
  m_linear_elements.clear();
  m_structures.clear();

  if (root.contains(MapJsonKeys::terrain)) {
    parse_terrain_array(root[MapJsonKeys::terrain].toArray());
  }
  if (root.contains(MapJsonKeys::world_props)) {
    parse_world_props_array(root[MapJsonKeys::world_props].toArray());
  }
  if (root.contains(MapJsonKeys::firecamps)) {
    parse_legacy_firecamps_array(root[MapJsonKeys::firecamps].toArray());
  }
  if (root.contains(MapJsonKeys::rivers)) {
    parse_rivers_array(root[MapJsonKeys::rivers].toArray());
  }
  if (root.contains(MapJsonKeys::roads)) {
    parse_roads_array(root[MapJsonKeys::roads].toArray());
  }
  if (root.contains(MapJsonKeys::bridges)) {
    parse_bridges_array(root[MapJsonKeys::bridges].toArray());
  }

  if (root.contains(MapJsonKeys::spawns)) {
    parse_structures_from_spawns(root[MapJsonKeys::spawns].toArray());
  }

  m_undo_stack.clear();
  m_redo_stack.clear();

  set_modified(false);
  emit data_changed();
  emit undo_redo_changed();
  return true;
}

bool MapData::save_to_json(const QString &file_path, QString *out_error) const {
  if (file_path.trimmed().isEmpty()) {
    if (out_error != nullptr) {
      *out_error = "No output path was provided.";
    }
    return false;
  }

  QJsonObject root = m_extra_root_fields;

  root[MapJsonKeys::name] = m_name;
  if (!m_description.isEmpty()) {
    root[MapJsonKeys::description] = m_description;
  }
  root[coord_system_key] = m_coord_system;
  root[max_troops_key] = m_max_troops_per_player;

  QJsonObject grid_obj;
  grid_obj[MapJsonKeys::width] = m_grid.width;
  grid_obj[MapJsonKeys::height] = m_grid.height;
  grid_obj[MapJsonKeys::tile_size] = static_cast<double>(m_grid.tile_size);
  root[MapJsonKeys::grid] = grid_obj;

  if (!m_biome.isEmpty()) {
    root[MapJsonKeys::biome] = m_biome;
  }
  if (!m_camera.isEmpty()) {
    root[MapJsonKeys::camera] = m_camera;
  }
  if (!m_victory.isEmpty()) {
    root[MapJsonKeys::victory] = m_victory;
  }
  if (!m_rain.isEmpty()) {
    root[MapJsonKeys::rain] = m_rain;
  }

  QJsonArray terrain_arr = terrain_to_json();
  if (!terrain_arr.isEmpty()) {
    root[MapJsonKeys::terrain] = terrain_arr;
  }

  QJsonArray world_props_arr = world_props_to_json();
  if (!world_props_arr.isEmpty()) {
    root[MapJsonKeys::world_props] = world_props_arr;
  }

  QJsonArray rivers_arr = rivers_to_json();
  if (!rivers_arr.isEmpty()) {
    root[MapJsonKeys::rivers] = rivers_arr;
  }

  QJsonArray roads_arr = roads_to_json();
  if (!roads_arr.isEmpty()) {
    root[MapJsonKeys::roads] = roads_arr;
  }

  QJsonArray bridges_arr = bridges_to_json();
  if (!bridges_arr.isEmpty()) {
    root[MapJsonKeys::bridges] = bridges_arr;
  }

  QJsonArray spawns_arr = structures_to_spawns_json();

  for (const auto &spawn : m_spawns) {
    QJsonObject spawn_obj = spawn.toObject();
    const QString type = spawn_obj[MapJsonKeys::type].toString();
    if (type != "barracks" && type != "village") {
      spawns_arr.append(spawn);
    }
  }
  if (!spawns_arr.isEmpty()) {
    root[MapJsonKeys::spawns] = spawns_arr;
  }

  const QJsonDocument doc(root);
  const QByteArray json_data = doc.toJson(QJsonDocument::Indented);

  QSaveFile file(file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (out_error != nullptr) {
      *out_error = file.errorString();
    }
    return false;
  }

  const qint64 bytes_written = file.write(json_data);
  if (bytes_written != json_data.size()) {
    if (out_error != nullptr) {
      *out_error = file.errorString().trimmed().isEmpty()
                       ? "Failed to write the complete JSON payload."
                       : file.errorString();
    }
    file.cancelWriting();
    return false;
  }

  if (!file.commit()) {
    if (out_error != nullptr) {
      *out_error = file.errorString();
    }
    return false;
  }

  return true;
}

void MapData::parse_terrain_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    TerrainElement elem;
    elem.type = obj[MapJsonKeys::type].toString();
    elem.x = static_cast<float>(obj[MapJsonKeys::x].toDouble());
    elem.z = static_cast<float>(obj[MapJsonKeys::z].toDouble());
    elem.radius = static_cast<float>(obj[MapJsonKeys::radius].toDouble(10.0));
    elem.width = static_cast<float>(obj[MapJsonKeys::width].toDouble(10.0));
    elem.depth = static_cast<float>(obj[MapJsonKeys::depth].toDouble(10.0));
    elem.height = static_cast<float>(obj[MapJsonKeys::height].toDouble(3.0));
    elem.rotation =
        static_cast<float>(obj[MapJsonKeys::rotation].toDouble(0.0));
    elem.entrances = obj[MapJsonKeys::entrances].toArray();

    const QStringList known_keys = {
        MapJsonKeys::type,   MapJsonKeys::x,        MapJsonKeys::z,
        MapJsonKeys::radius, MapJsonKeys::width,    MapJsonKeys::depth,
        MapJsonKeys::height, MapJsonKeys::rotation, MapJsonKeys::entrances};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_terrain.append(elem);
  }
}

void MapData::parse_world_props_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    WorldPropElement elem;
    elem.type = obj[MapJsonKeys::type].toString(QStringLiteral("firecamp"));
    elem.x = static_cast<float>(obj[MapJsonKeys::x].toDouble());
    elem.z = static_cast<float>(obj[MapJsonKeys::z].toDouble());
    elem.scale = static_cast<float>(obj[MapJsonKeys::scale].toDouble(1.0));
    elem.rotation =
        static_cast<float>(obj[MapJsonKeys::rotation].toDouble(0.0));
    elem.intensity =
        static_cast<float>(obj[MapJsonKeys::intensity].toDouble(1.0));
    elem.radius = static_cast<float>(obj[MapJsonKeys::radius].toDouble(3.0));
    elem.persistent = obj[MapJsonKeys::persistent].toBool(true);

    const QStringList known_keys = {
        MapJsonKeys::type,   MapJsonKeys::x,         MapJsonKeys::z,
        MapJsonKeys::scale,  MapJsonKeys::rotation,  MapJsonKeys::intensity,
        MapJsonKeys::radius, MapJsonKeys::persistent};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_world_props.append(elem);
  }
}

void MapData::parse_legacy_firecamps_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    WorldPropElement elem;
    elem.type = QStringLiteral("firecamp");
    elem.x = static_cast<float>(obj[MapJsonKeys::x].toDouble());
    elem.z = static_cast<float>(obj[MapJsonKeys::z].toDouble());
    elem.intensity =
        static_cast<float>(obj[MapJsonKeys::intensity].toDouble(1.0));
    elem.radius = static_cast<float>(obj[MapJsonKeys::radius].toDouble(3.0));

    const QStringList known_keys = {MapJsonKeys::x, MapJsonKeys::z,
                                    MapJsonKeys::intensity,
                                    MapJsonKeys::radius};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_world_props.append(elem);
  }
}

void MapData::parse_rivers_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "river";

    applyLinearEndpoints(obj, elem);
    elem.width = static_cast<float>(obj[MapJsonKeys::width].toDouble(3.0));

    const QStringList known_keys = {MapJsonKeys::start, MapJsonKeys::end,
                                    MapJsonKeys::width};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_linear_elements.append(elem);
  }
}

void MapData::parse_roads_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "road";

    applyLinearEndpoints(obj, elem);
    elem.width = static_cast<float>(obj[MapJsonKeys::width].toDouble(3.0));
    elem.style = obj[MapJsonKeys::style].toString("default");

    const QStringList known_keys = {MapJsonKeys::start, MapJsonKeys::end,
                                    MapJsonKeys::width, MapJsonKeys::style};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_linear_elements.append(elem);
  }
}

void MapData::parse_bridges_array(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "bridge";

    applyLinearEndpoints(obj, elem);
    elem.width = static_cast<float>(obj[MapJsonKeys::width].toDouble(4.0));
    elem.height = static_cast<float>(obj[MapJsonKeys::height].toDouble(0.5));

    const QStringList known_keys = {MapJsonKeys::start, MapJsonKeys::end,
                                    MapJsonKeys::width, MapJsonKeys::height};
    elem.extra_fields = copyExtraFields(obj, known_keys);

    m_linear_elements.append(elem);
  }
}

QJsonArray MapData::terrain_to_json() const {
  QJsonArray arr;
  for (const auto &elem : m_terrain) {
    QJsonObject obj;
    obj[MapJsonKeys::type] = elem.type;
    obj[MapJsonKeys::x] = static_cast<double>(elem.x);
    obj[MapJsonKeys::z] = static_cast<double>(elem.z);

    if (elem.type == "hill") {

      bool has_custom_dimensions = (elem.width != 10.0F && elem.width > 0.0F) ||
                                   (elem.depth != 10.0F && elem.depth > 0.0F);

      if (has_custom_dimensions) {
        if (elem.width > 0.0F) {
          obj[MapJsonKeys::width] = static_cast<double>(elem.width);
        }
        if (elem.depth > 0.0F) {
          obj[MapJsonKeys::depth] = static_cast<double>(elem.depth);
        }
      } else if (elem.radius > 0.0F) {

        obj[MapJsonKeys::radius] = static_cast<double>(elem.radius);
      }
    } else {
      obj[MapJsonKeys::radius] = static_cast<double>(elem.radius);
    }

    obj[MapJsonKeys::height] = static_cast<double>(elem.height);
    if (elem.rotation != 0.0F) {
      obj[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
    }
    if (!elem.entrances.isEmpty()) {
      obj[MapJsonKeys::entrances] = elem.entrances;
    }

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::world_props_to_json() const {
  QJsonArray arr;
  for (const auto &elem : m_world_props) {
    QJsonObject obj;
    obj[MapJsonKeys::type] = elem.type;
    obj[MapJsonKeys::x] = static_cast<double>(elem.x);
    obj[MapJsonKeys::z] = static_cast<double>(elem.z);
    obj[MapJsonKeys::scale] = static_cast<double>(elem.scale);
    if (elem.rotation != 0.0F) {
      obj[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
    }
    if (elem.type == QStringLiteral("firecamp")) {
      obj[MapJsonKeys::intensity] = static_cast<double>(elem.intensity);
      obj[MapJsonKeys::radius] = static_cast<double>(elem.radius);
      obj[MapJsonKeys::persistent] = elem.persistent;
    }

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::rivers_to_json() const {
  QJsonArray arr;
  for (const auto &elem : m_linear_elements) {
    if (elem.type != "river") {
      continue;
    }
    QJsonObject obj;
    obj[MapJsonKeys::start] = QJsonArray{static_cast<double>(elem.start.x()),
                                         static_cast<double>(elem.start.y())};
    obj[MapJsonKeys::end] = QJsonArray{static_cast<double>(elem.end.x()),
                                       static_cast<double>(elem.end.y())};
    obj[MapJsonKeys::width] = static_cast<double>(elem.width);

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::roads_to_json() const {
  QJsonArray arr;
  for (const auto &elem : m_linear_elements) {
    if (elem.type != "road") {
      continue;
    }
    QJsonObject obj;
    obj[MapJsonKeys::start] = QJsonArray{static_cast<double>(elem.start.x()),
                                         static_cast<double>(elem.start.y())};
    obj[MapJsonKeys::end] = QJsonArray{static_cast<double>(elem.end.x()),
                                       static_cast<double>(elem.end.y())};
    obj[MapJsonKeys::width] = static_cast<double>(elem.width);
    obj[MapJsonKeys::style] = elem.style.isEmpty() ? "default" : elem.style;

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::bridges_to_json() const {
  QJsonArray arr;
  for (const auto &elem : m_linear_elements) {
    if (elem.type != "bridge") {
      continue;
    }
    QJsonObject obj;
    obj[MapJsonKeys::start] = QJsonArray{static_cast<double>(elem.start.x()),
                                         static_cast<double>(elem.start.y())};
    obj[MapJsonKeys::end] = QJsonArray{static_cast<double>(elem.end.x()),
                                       static_cast<double>(elem.end.y())};
    obj[MapJsonKeys::width] = static_cast<double>(elem.width);
    obj[MapJsonKeys::height] = static_cast<double>(elem.height);

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

void MapData::add_terrain_element(const TerrainElement &element) {
  m_terrain.append(element);
  set_modified(true);
  emit data_changed();
}

void MapData::insert_terrain_element(int index, const TerrainElement &element) {
  if (index >= 0 && index <= m_terrain.size()) {
    m_terrain.insert(index, element);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::update_terrain_element(int index, const TerrainElement &element) {
  if (index >= 0 && index < m_terrain.size()) {
    m_terrain[index] = element;
    set_modified(true);
    emit data_changed();
  }
}

void MapData::remove_terrain_element(int index) {
  if (index >= 0 && index < m_terrain.size()) {
    m_terrain.removeAt(index);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::add_world_prop(const WorldPropElement &element) {
  m_world_props.append(element);
  set_modified(true);
  emit data_changed();
}

void MapData::insert_world_prop(int index, const WorldPropElement &element) {
  if (index >= 0 && index <= m_world_props.size()) {
    m_world_props.insert(index, element);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::update_world_prop(int index, const WorldPropElement &element) {
  if (index >= 0 && index < m_world_props.size()) {
    m_world_props[index] = element;
    set_modified(true);
    emit data_changed();
  }
}

void MapData::remove_world_prop(int index) {
  if (index >= 0 && index < m_world_props.size()) {
    m_world_props.removeAt(index);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::add_linear_element(const LinearElement &element) {
  LinearElement normalized = element;
  syncLinearWaypointsWithEndpoints(normalized);
  m_linear_elements.append(normalized);
  set_modified(true);
  emit data_changed();
}

void MapData::insert_linear_element(int index, const LinearElement &element) {
  if (index >= 0 && index <= m_linear_elements.size()) {
    LinearElement normalized = element;
    syncLinearWaypointsWithEndpoints(normalized);
    m_linear_elements.insert(index, normalized);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::update_linear_element(int index, const LinearElement &element) {
  if (index >= 0 && index < m_linear_elements.size()) {
    LinearElement normalized = element;
    syncLinearWaypointsWithEndpoints(normalized);
    m_linear_elements[index] = normalized;
    set_modified(true);
    emit data_changed();
  }
}

void MapData::remove_linear_element(int index) {
  if (index >= 0 && index < m_linear_elements.size()) {
    m_linear_elements.removeAt(index);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::add_structure(const StructureElement &element) {
  m_structures.append(element);
  set_modified(true);
  emit data_changed();
}

void MapData::insert_structure(int index, const StructureElement &element) {
  if (index >= 0 && index <= m_structures.size()) {
    m_structures.insert(index, element);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::update_structure(int index, const StructureElement &element) {
  if (index >= 0 && index < m_structures.size()) {
    m_structures[index] = element;
    set_modified(true);
    emit data_changed();
  }
}

void MapData::remove_structure(int index) {
  if (index >= 0 && index < m_structures.size()) {
    m_structures.removeAt(index);
    set_modified(true);
    emit data_changed();
  }
}

void MapData::parse_structures_from_spawns(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    const QString type = obj[MapJsonKeys::type].toString();

    if (type == "barracks" || type == "village") {
      StructureElement elem;
      elem.type = type;
      elem.x = static_cast<float>(obj[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(obj[MapJsonKeys::z].toDouble());
      elem.player_id = obj[MapJsonKeys::player_id].toInt(0);
      elem.max_population = obj[MapJsonKeys::max_population].toInt(150);
      elem.nation = obj[MapJsonKeys::nation].toString();

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::x,
                                      MapJsonKeys::z,
                                      MapJsonKeys::player_id,
                                      MapJsonKeys::max_population,
                                      MapJsonKeys::nation};
      elem.extra_fields = copyExtraFields(obj, known_keys);

      m_structures.append(elem);
    }
  }
}

QJsonArray MapData::structures_to_spawns_json() const {
  QJsonArray arr;
  for (const auto &elem : m_structures) {
    QJsonObject obj;
    obj[MapJsonKeys::type] = elem.type;
    obj[MapJsonKeys::x] = static_cast<double>(elem.x);
    obj[MapJsonKeys::z] = static_cast<double>(elem.z);
    if (elem.player_id > 0) {
      obj[MapJsonKeys::player_id] = elem.player_id;
    }
    obj[MapJsonKeys::max_population] = elem.max_population;
    if (!elem.nation.isEmpty()) {
      obj[MapJsonKeys::nation] = elem.nation;
    }

    for (const QString &key : elem.extra_fields.keys()) {
      obj[key] = elem.extra_fields[key];
    }

    arr.append(obj);
  }
  return arr;
}

void MapData::record_command(std::unique_ptr<Command> cmd) {
  if (!cmd) {
    return;
  }
  m_undo_stack.push_back(std::move(cmd));
  m_redo_stack.clear();
  emit undo_redo_changed();
}

QString MapData::undo_description() const {
  return m_undo_stack.empty() ? QString{} : m_undo_stack.back()->description();
}

QString MapData::redo_description() const {
  return m_redo_stack.empty() ? QString{} : m_redo_stack.back()->description();
}

} // namespace MapEditor
