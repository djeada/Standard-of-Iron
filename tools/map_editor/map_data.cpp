#include "map_data.h"
#include <QFile>
#include <QJsonDocument>

namespace MapEditor {

MapData::MapData(QObject *parent) : QObject(parent) { clear(); }

void MapData::clear() {
  m_name = "New Map";
  m_description.clear();
  m_coordSystem = "grid";
  m_maxTroopsPerPlayer = 2000;

  m_grid = GridSettings{100, 100, 1.0F};

  m_terrain.clear();
  m_firecamps.clear();
  m_linearElements.clear();
  m_structures.clear();

  m_biome = QJsonObject();
  m_camera = QJsonObject();
  m_spawns = QJsonArray();
  m_victory = QJsonObject();
  m_rain = QJsonObject();

  // Clear undo/redo stacks
  while (!m_undoStack.isEmpty()) {
    m_undoStack.pop();
  }
  while (!m_redoStack.isEmpty()) {
    m_redoStack.pop();
  }

  setModified(false);
  emit dataChanged();
  emit undoRedoChanged();
}

void MapData::setName(const QString &name) {
  if (m_name != name) {
    m_name = name;
    setModified(true);
    emit dataChanged();
  }
}

void MapData::setGrid(const GridSettings &grid) {
  m_grid = grid;
  setModified(true);
  emit dataChanged();
}

void MapData::setModified(bool modified) {
  if (m_modified != modified) {
    m_modified = modified;
    emit modifiedChanged(modified);
  }
}

void MapData::executeCommand(std::unique_ptr<Command> cmd) {
  if (!cmd) {
    return;
  }
  cmd->execute();
  m_undoStack.push(std::move(cmd));
  // Clear redo stack when new command is executed
  while (!m_redoStack.isEmpty()) {
    m_redoStack.pop();
  }
  setModified(true);
  emit undoRedoChanged();
}

void MapData::undo() {
  if (m_undoStack.isEmpty()) {
    return;
  }
  auto cmd = std::move(m_undoStack.top());
  m_undoStack.pop();
  cmd->undo();
  m_redoStack.push(std::move(cmd));
  setModified(true);
  emit dataChanged();
  emit undoRedoChanged();
}

void MapData::redo() {
  if (m_redoStack.isEmpty()) {
    return;
  }
  auto cmd = std::move(m_redoStack.top());
  m_redoStack.pop();
  cmd->execute();
  m_undoStack.push(std::move(cmd));
  setModified(true);
  emit dataChanged();
  emit undoRedoChanged();
}

bool MapData::loadFromJson(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError) {
    return false;
  }

  QJsonObject root = doc.object();

  // Parse basic properties
  m_name = root["name"].toString("Untitled Map");
  m_description = root["description"].toString();
  m_coordSystem = root["coordSystem"].toString("grid");
  m_maxTroopsPerPlayer = root["maxTroopsPerPlayer"].toInt(2000);

  // Parse grid
  if (root.contains("grid")) {
    QJsonObject gridObj = root["grid"].toObject();
    m_grid.width = gridObj["width"].toInt(100);
    m_grid.height = gridObj["height"].toInt(100);
    m_grid.tileSize = static_cast<float>(gridObj["tileSize"].toDouble(1.0));
  }

  // Store passthrough data
  m_biome = root["biome"].toObject();
  m_camera = root["camera"].toObject();
  m_spawns = root["spawns"].toArray();
  m_victory = root["victory"].toObject();
  m_rain = root["rain"].toObject();

  // Parse editable elements
  m_terrain.clear();
  m_firecamps.clear();
  m_linearElements.clear();
  m_structures.clear();

  if (root.contains("terrain")) {
    parseTerrainArray(root["terrain"].toArray());
  }
  if (root.contains("firecamps")) {
    parseFirecampsArray(root["firecamps"].toArray());
  }
  if (root.contains("rivers")) {
    parseRiversArray(root["rivers"].toArray());
  }
  if (root.contains("roads")) {
    parseRoadsArray(root["roads"].toArray());
  }
  if (root.contains("bridges")) {
    parseBridgesArray(root["bridges"].toArray());
  }
  // Parse structures from spawns (barracks, villages)
  if (root.contains("spawns")) {
    parseStructuresFromSpawns(root["spawns"].toArray());
  }

  // Clear undo/redo stacks on load
  while (!m_undoStack.isEmpty()) {
    m_undoStack.pop();
  }
  while (!m_redoStack.isEmpty()) {
    m_redoStack.pop();
  }

  setModified(false);
  emit dataChanged();
  emit undoRedoChanged();
  return true;
}

bool MapData::saveToJson(const QString &filePath) const {
  QJsonObject root;

  // Basic properties
  root["name"] = m_name;
  if (!m_description.isEmpty()) {
    root["description"] = m_description;
  }
  root["coordSystem"] = m_coordSystem;
  root["maxTroopsPerPlayer"] = m_maxTroopsPerPlayer;

  // Grid
  QJsonObject gridObj;
  gridObj["width"] = m_grid.width;
  gridObj["height"] = m_grid.height;
  gridObj["tileSize"] = static_cast<double>(m_grid.tileSize);
  root["grid"] = gridObj;

  // Passthrough data
  if (!m_biome.isEmpty()) {
    root["biome"] = m_biome;
  }
  if (!m_camera.isEmpty()) {
    root["camera"] = m_camera;
  }
  if (!m_victory.isEmpty()) {
    root["victory"] = m_victory;
  }
  if (!m_rain.isEmpty()) {
    root["rain"] = m_rain;
  }

  // Editable elements
  QJsonArray terrainArr = terrainToJson();
  if (!terrainArr.isEmpty()) {
    root["terrain"] = terrainArr;
  }

  QJsonArray firecampsArr = firecampsToJson();
  if (!firecampsArr.isEmpty()) {
    root["firecamps"] = firecampsArr;
  }

  QJsonArray riversArr = riversToJson();
  if (!riversArr.isEmpty()) {
    root["rivers"] = riversArr;
  }

  QJsonArray roadsArr = roadsToJson();
  if (!roadsArr.isEmpty()) {
    root["roads"] = roadsArr;
  }

  QJsonArray bridgesArr = bridgesToJson();
  if (!bridgesArr.isEmpty()) {
    root["bridges"] = bridgesArr;
  }

  // Merge structures into spawns
  QJsonArray spawnsArr = structuresToSpawnsJson();
  // Add non-structure spawns from original data
  for (const auto &spawn : m_spawns) {
    QJsonObject spawnObj = spawn.toObject();
    QString type = spawnObj["type"].toString();
    if (type != "barracks" && type != "village") {
      spawnsArr.append(spawn);
    }
  }
  if (!spawnsArr.isEmpty()) {
    root["spawns"] = spawnsArr;
  }

  QJsonDocument doc(root);
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  return true;
}

void MapData::parseTerrainArray(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    TerrainElement elem;
    elem.type = obj["type"].toString();
    elem.x = static_cast<float>(obj["x"].toDouble());
    elem.z = static_cast<float>(obj["z"].toDouble());
    elem.radius = static_cast<float>(obj["radius"].toDouble(10.0));
    elem.width = static_cast<float>(obj["width"].toDouble(10.0));
    elem.depth = static_cast<float>(obj["depth"].toDouble(10.0));
    elem.height = static_cast<float>(obj["height"].toDouble(3.0));
    elem.rotation = static_cast<float>(obj["rotation"].toDouble(0.0));
    elem.entrances = obj["entrances"].toArray();

    // Store extra fields
    QStringList knownKeys = {"type",     "x",         "z",     "radius",
                             "width",    "depth",     "height", "rotation",
                             "entrances"};
    for (const QString &key : obj.keys()) {
      if (!knownKeys.contains(key)) {
        elem.extraFields[key] = obj[key];
      }
    }

    m_terrain.append(elem);
  }
}

void MapData::parseFirecampsArray(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    FirecampElement elem;
    elem.x = static_cast<float>(obj["x"].toDouble());
    elem.z = static_cast<float>(obj["z"].toDouble());
    elem.intensity = static_cast<float>(obj["intensity"].toDouble(1.0));
    elem.radius = static_cast<float>(obj["radius"].toDouble(3.0));

    QStringList knownKeys = {"x", "z", "intensity", "radius"};
    for (const QString &key : obj.keys()) {
      if (!knownKeys.contains(key)) {
        elem.extraFields[key] = obj[key];
      }
    }

    m_firecamps.append(elem);
  }
}

void MapData::parseRiversArray(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "river";

    QJsonArray startArr = obj["start"].toArray();
    QJsonArray endArr = obj["end"].toArray();
    if (startArr.size() >= 2 && endArr.size() >= 2) {
      elem.start = QVector2D(static_cast<float>(startArr[0].toDouble()),
                             static_cast<float>(startArr[1].toDouble()));
      elem.end = QVector2D(static_cast<float>(endArr[0].toDouble()),
                           static_cast<float>(endArr[1].toDouble()));
    }
    elem.width = static_cast<float>(obj["width"].toDouble(3.0));

    QStringList knownKeys = {"start", "end", "width"};
    for (const QString &key : obj.keys()) {
      if (!knownKeys.contains(key)) {
        elem.extraFields[key] = obj[key];
      }
    }

    m_linearElements.append(elem);
  }
}

void MapData::parseRoadsArray(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "road";

    QJsonArray startArr = obj["start"].toArray();
    QJsonArray endArr = obj["end"].toArray();
    if (startArr.size() >= 2 && endArr.size() >= 2) {
      elem.start = QVector2D(static_cast<float>(startArr[0].toDouble()),
                             static_cast<float>(startArr[1].toDouble()));
      elem.end = QVector2D(static_cast<float>(endArr[0].toDouble()),
                           static_cast<float>(endArr[1].toDouble()));
    }
    elem.width = static_cast<float>(obj["width"].toDouble(3.0));
    elem.style = obj["style"].toString("default");

    QStringList knownKeys = {"start", "end", "width", "style"};
    for (const QString &key : obj.keys()) {
      if (!knownKeys.contains(key)) {
        elem.extraFields[key] = obj[key];
      }
    }

    m_linearElements.append(elem);
  }
}

void MapData::parseBridgesArray(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    LinearElement elem;
    elem.type = "bridge";

    QJsonArray startArr = obj["start"].toArray();
    QJsonArray endArr = obj["end"].toArray();
    if (startArr.size() >= 2 && endArr.size() >= 2) {
      elem.start = QVector2D(static_cast<float>(startArr[0].toDouble()),
                             static_cast<float>(startArr[1].toDouble()));
      elem.end = QVector2D(static_cast<float>(endArr[0].toDouble()),
                           static_cast<float>(endArr[1].toDouble()));
    }
    elem.width = static_cast<float>(obj["width"].toDouble(4.0));
    elem.height = static_cast<float>(obj["height"].toDouble(0.5));

    QStringList knownKeys = {"start", "end", "width", "height"};
    for (const QString &key : obj.keys()) {
      if (!knownKeys.contains(key)) {
        elem.extraFields[key] = obj[key];
      }
    }

    m_linearElements.append(elem);
  }
}

QJsonArray MapData::terrainToJson() const {
  QJsonArray arr;
  for (const auto &elem : m_terrain) {
    QJsonObject obj;
    obj["type"] = elem.type;
    obj["x"] = static_cast<double>(elem.x);
    obj["z"] = static_cast<double>(elem.z);

    if (elem.type == "hill") {
      // Hills can use either radius or width/depth for sizing
      // Write width/depth if they differ from default (10.0F)
      bool hasCustomDimensions =
          (elem.width != 10.0F && elem.width > 0.0F) ||
          (elem.depth != 10.0F && elem.depth > 0.0F);

      if (hasCustomDimensions) {
        if (elem.width > 0.0F) {
          obj["width"] = static_cast<double>(elem.width);
        }
        if (elem.depth > 0.0F) {
          obj["depth"] = static_cast<double>(elem.depth);
        }
      } else if (elem.radius > 0.0F) {
        // Use radius if no custom dimensions
        obj["radius"] = static_cast<double>(elem.radius);
      }
    } else {
      obj["radius"] = static_cast<double>(elem.radius);
    }

    obj["height"] = static_cast<double>(elem.height);
    if (elem.rotation != 0.0F) {
      obj["rotation"] = static_cast<double>(elem.rotation);
    }
    if (!elem.entrances.isEmpty()) {
      obj["entrances"] = elem.entrances;
    }

    // Add extra fields
    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::firecampsToJson() const {
  QJsonArray arr;
  for (const auto &elem : m_firecamps) {
    QJsonObject obj;
    obj["x"] = static_cast<double>(elem.x);
    obj["z"] = static_cast<double>(elem.z);
    obj["intensity"] = static_cast<double>(elem.intensity);
    obj["radius"] = static_cast<double>(elem.radius);

    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::riversToJson() const {
  QJsonArray arr;
  for (const auto &elem : m_linearElements) {
    if (elem.type != "river") {
      continue;
    }
    QJsonObject obj;
    obj["start"] = QJsonArray{static_cast<double>(elem.start.x()),
                              static_cast<double>(elem.start.y())};
    obj["end"] = QJsonArray{static_cast<double>(elem.end.x()),
                            static_cast<double>(elem.end.y())};
    obj["width"] = static_cast<double>(elem.width);

    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::roadsToJson() const {
  QJsonArray arr;
  for (const auto &elem : m_linearElements) {
    if (elem.type != "road") {
      continue;
    }
    QJsonObject obj;
    obj["start"] = QJsonArray{static_cast<double>(elem.start.x()),
                              static_cast<double>(elem.start.y())};
    obj["end"] = QJsonArray{static_cast<double>(elem.end.x()),
                            static_cast<double>(elem.end.y())};
    obj["width"] = static_cast<double>(elem.width);
    obj["style"] = elem.style.isEmpty() ? "default" : elem.style;

    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

QJsonArray MapData::bridgesToJson() const {
  QJsonArray arr;
  for (const auto &elem : m_linearElements) {
    if (elem.type != "bridge") {
      continue;
    }
    QJsonObject obj;
    obj["start"] = QJsonArray{static_cast<double>(elem.start.x()),
                              static_cast<double>(elem.start.y())};
    obj["end"] = QJsonArray{static_cast<double>(elem.end.x()),
                            static_cast<double>(elem.end.y())};
    obj["width"] = static_cast<double>(elem.width);
    obj["height"] = static_cast<double>(elem.height);

    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

// Element manipulation methods
void MapData::addTerrainElement(const TerrainElement &element) {
  m_terrain.append(element);
  setModified(true);
  emit dataChanged();
}

void MapData::updateTerrainElement(int index, const TerrainElement &element) {
  if (index >= 0 && index < m_terrain.size()) {
    m_terrain[index] = element;
    setModified(true);
    emit dataChanged();
  }
}

void MapData::removeTerrainElement(int index) {
  if (index >= 0 && index < m_terrain.size()) {
    m_terrain.removeAt(index);
    setModified(true);
    emit dataChanged();
  }
}

void MapData::addFirecamp(const FirecampElement &element) {
  m_firecamps.append(element);
  setModified(true);
  emit dataChanged();
}

void MapData::updateFirecamp(int index, const FirecampElement &element) {
  if (index >= 0 && index < m_firecamps.size()) {
    m_firecamps[index] = element;
    setModified(true);
    emit dataChanged();
  }
}

void MapData::removeFirecamp(int index) {
  if (index >= 0 && index < m_firecamps.size()) {
    m_firecamps.removeAt(index);
    setModified(true);
    emit dataChanged();
  }
}

void MapData::addLinearElement(const LinearElement &element) {
  m_linearElements.append(element);
  setModified(true);
  emit dataChanged();
}

void MapData::updateLinearElement(int index, const LinearElement &element) {
  if (index >= 0 && index < m_linearElements.size()) {
    m_linearElements[index] = element;
    setModified(true);
    emit dataChanged();
  }
}

void MapData::removeLinearElement(int index) {
  if (index >= 0 && index < m_linearElements.size()) {
    m_linearElements.removeAt(index);
    setModified(true);
    emit dataChanged();
  }
}

void MapData::addStructure(const StructureElement &element) {
  m_structures.append(element);
  setModified(true);
  emit dataChanged();
}

void MapData::updateStructure(int index, const StructureElement &element) {
  if (index >= 0 && index < m_structures.size()) {
    m_structures[index] = element;
    setModified(true);
    emit dataChanged();
  }
}

void MapData::removeStructure(int index) {
  if (index >= 0 && index < m_structures.size()) {
    m_structures.removeAt(index);
    setModified(true);
    emit dataChanged();
  }
}

void MapData::parseStructuresFromSpawns(const QJsonArray &arr) {
  for (const auto &val : arr) {
    QJsonObject obj = val.toObject();
    QString type = obj["type"].toString();

    // Only extract barracks and villages as editable structures
    if (type == "barracks" || type == "village") {
      StructureElement elem;
      elem.type = type;
      elem.x = static_cast<float>(obj["x"].toDouble());
      elem.z = static_cast<float>(obj["z"].toDouble());
      elem.playerId = obj["playerId"].toInt(0);
      elem.maxPopulation = obj["maxPopulation"].toInt(150);
      elem.nation = obj["nation"].toString();

      QStringList knownKeys = {"type", "x", "z", "playerId", "maxPopulation",
                               "nation"};
      for (const QString &key : obj.keys()) {
        if (!knownKeys.contains(key)) {
          elem.extraFields[key] = obj[key];
        }
      }

      m_structures.append(elem);
    }
  }
}

QJsonArray MapData::structuresToSpawnsJson() const {
  QJsonArray arr;
  for (const auto &elem : m_structures) {
    QJsonObject obj;
    obj["type"] = elem.type;
    obj["x"] = static_cast<double>(elem.x);
    obj["z"] = static_cast<double>(elem.z);
    if (elem.playerId > 0) {
      obj["playerId"] = elem.playerId;
    }
    obj["maxPopulation"] = elem.maxPopulation;
    if (!elem.nation.isEmpty()) {
      obj["nation"] = elem.nation;
    }

    for (const QString &key : elem.extraFields.keys()) {
      obj[key] = elem.extraFields[key];
    }

    arr.append(obj);
  }
  return arr;
}

} // namespace MapEditor
