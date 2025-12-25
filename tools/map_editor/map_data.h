#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector2D>
#include <QVector>
#include <memory>
#include <variant>
#include <vector>

namespace MapEditor {

struct TerrainElement {
  QString type;
  float x = 0.0F;
  float z = 0.0F;
  float radius = 10.0F;
  float width = 10.0F;
  float depth = 10.0F;
  float height = 3.0F;
  float rotation = 0.0F;
  QJsonArray entrances;
  QJsonObject extraFields;
};

struct FirecampElement {
  float x = 0.0F;
  float z = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  QJsonObject extraFields;
};

struct LinearElement {
  QString type;
  QVector2D start;
  QVector2D end;
  float width = 3.0F;
  float height = 0.5F;
  QString style;
  QJsonObject extraFields;
};

struct StructureElement {
  QString type;
  float x = 0.0F;
  float z = 0.0F;
  int playerId = 0;
  int maxPopulation = 150;
  QString nation;
  QJsonObject extraFields;
};

struct GridSettings {
  int width = 100;
  int height = 100;
  float tileSize = 1.0F;
};

class MapData;

class Command {
public:
  virtual ~Command() = default;
  virtual void execute() = 0;
  virtual void undo() = 0;
};

class MapData : public QObject {
  Q_OBJECT

public:
  explicit MapData(QObject *parent = nullptr);

  bool loadFromJson(const QString &filePath);
  bool saveToJson(const QString &filePath) const;

  [[nodiscard]] QString name() const { return m_name; }
  void setName(const QString &name);

  [[nodiscard]] const GridSettings &grid() const { return m_grid; }
  void setGrid(const GridSettings &grid);

  [[nodiscard]] const QVector<TerrainElement> &terrainElements() const {
    return m_terrain;
  }
  void addTerrainElement(const TerrainElement &element);
  void updateTerrainElement(int index, const TerrainElement &element);
  void removeTerrainElement(int index);

  [[nodiscard]] const QVector<FirecampElement> &firecamps() const {
    return m_firecamps;
  }
  void addFirecamp(const FirecampElement &element);
  void updateFirecamp(int index, const FirecampElement &element);
  void removeFirecamp(int index);

  [[nodiscard]] const QVector<LinearElement> &linearElements() const {
    return m_linearElements;
  }
  void addLinearElement(const LinearElement &element);
  void updateLinearElement(int index, const LinearElement &element);
  void removeLinearElement(int index);

  [[nodiscard]] const QVector<StructureElement> &structures() const {
    return m_structures;
  }
  void addStructure(const StructureElement &element);
  void updateStructure(int index, const StructureElement &element);
  void removeStructure(int index);

  void executeCommand(std::unique_ptr<Command> cmd);
  void undo();
  void redo();
  [[nodiscard]] bool canUndo() const { return !m_undoStack.empty(); }
  [[nodiscard]] bool canRedo() const { return !m_redoStack.empty(); }

  void clear();

  [[nodiscard]] bool isModified() const { return m_modified; }
  void setModified(bool modified);

signals:
  void dataChanged();
  void modifiedChanged(bool modified);
  void undoRedoChanged();

private:
  QString m_name;
  GridSettings m_grid;
  QVector<TerrainElement> m_terrain;
  QVector<FirecampElement> m_firecamps;
  QVector<LinearElement> m_linearElements;
  QVector<StructureElement> m_structures;

  QJsonObject m_biome;
  QJsonObject m_camera;
  QJsonArray m_spawns;
  QJsonObject m_victory;
  QJsonObject m_rain;
  QString m_description;
  QString m_coordSystem;
  int m_maxTroopsPerPlayer = 2000;

  bool m_modified = false;

  std::vector<std::unique_ptr<Command>> m_undoStack;
  std::vector<std::unique_ptr<Command>> m_redoStack;

  void parseTerrainArray(const QJsonArray &arr);
  void parseFirecampsArray(const QJsonArray &arr);
  void parseRiversArray(const QJsonArray &arr);
  void parseRoadsArray(const QJsonArray &arr);
  void parseBridgesArray(const QJsonArray &arr);
  void parseStructuresFromSpawns(const QJsonArray &arr);

  [[nodiscard]] QJsonArray terrainToJson() const;
  [[nodiscard]] QJsonArray firecampsToJson() const;
  [[nodiscard]] QJsonArray riversToJson() const;
  [[nodiscard]] QJsonArray roadsToJson() const;
  [[nodiscard]] QJsonArray bridgesToJson() const;
  [[nodiscard]] QJsonArray structuresToSpawnsJson() const;
};

} // namespace MapEditor
