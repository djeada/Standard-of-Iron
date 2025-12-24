#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QVector2D>

namespace MapEditor {

/**
 * @brief Represents a terrain element on the map (hill, mountain, etc.)
 */
struct TerrainElement {
  QString type; // "hill", "mountain"
  float x = 0.0F;
  float z = 0.0F;
  float radius = 10.0F;
  float width = 10.0F;
  float depth = 10.0F;
  float height = 3.0F;
  float rotation = 0.0F;
  QJsonArray entrances;    // Stored as raw JSON for passthrough
  QJsonObject extraFields; // Additional JSON fields not rendered
};

/**
 * @brief Represents a firecamp element
 */
struct FirecampElement {
  float x = 0.0F;
  float z = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  QJsonObject extraFields;
};

/**
 * @brief Represents a linear element (river, road, bridge)
 */
struct LinearElement {
  QString type; // "river", "road", "bridge"
  QVector2D start;
  QVector2D end;
  float width = 3.0F;
  float height = 0.5F;     // For bridges
  QString style;           // For roads
  QJsonObject extraFields; // Additional JSON fields
};

/**
 * @brief Grid settings for the map
 */
struct GridSettings {
  int width = 100;
  int height = 100;
  float tileSize = 1.0F;
};

/**
 * @brief Holds all map data for the editor
 */
class MapData : public QObject {
  Q_OBJECT

public:
  explicit MapData(QObject *parent = nullptr);

  // File operations
  bool loadFromJson(const QString &filePath);
  bool saveToJson(const QString &filePath) const;

  // Map properties
  [[nodiscard]] QString name() const { return m_name; }
  void setName(const QString &name);

  [[nodiscard]] const GridSettings &grid() const { return m_grid; }
  void setGrid(const GridSettings &grid);

  // Terrain elements
  [[nodiscard]] const QVector<TerrainElement> &terrainElements() const {
    return m_terrain;
  }
  void addTerrainElement(const TerrainElement &element);
  void updateTerrainElement(int index, const TerrainElement &element);
  void removeTerrainElement(int index);

  // Firecamps
  [[nodiscard]] const QVector<FirecampElement> &firecamps() const {
    return m_firecamps;
  }
  void addFirecamp(const FirecampElement &element);
  void updateFirecamp(int index, const FirecampElement &element);
  void removeFirecamp(int index);

  // Linear elements (rivers, roads, bridges)
  [[nodiscard]] const QVector<LinearElement> &linearElements() const {
    return m_linearElements;
  }
  void addLinearElement(const LinearElement &element);
  void updateLinearElement(int index, const LinearElement &element);
  void removeLinearElement(int index);

  // Clear all data for new map
  void clear();

  // Check if modified
  [[nodiscard]] bool isModified() const { return m_modified; }
  void setModified(bool modified);

signals:
  void dataChanged();
  void modifiedChanged(bool modified);

private:
  QString m_name;
  GridSettings m_grid;
  QVector<TerrainElement> m_terrain;
  QVector<FirecampElement> m_firecamps;
  QVector<LinearElement> m_linearElements;

  // Passthrough data (preserved but not edited)
  QJsonObject m_biome;
  QJsonObject m_camera;
  QJsonArray m_spawns;
  QJsonObject m_victory;
  QJsonObject m_rain;
  QString m_description;
  QString m_coordSystem;
  int m_maxTroopsPerPlayer = 2000;

  bool m_modified = false;

  // Helper methods
  void parseTerrainArray(const QJsonArray &arr);
  void parseFirecampsArray(const QJsonArray &arr);
  void parseRiversArray(const QJsonArray &arr);
  void parseRoadsArray(const QJsonArray &arr);
  void parseBridgesArray(const QJsonArray &arr);

  [[nodiscard]] QJsonArray terrainToJson() const;
  [[nodiscard]] QJsonArray firecampsToJson() const;
  [[nodiscard]] QJsonArray riversToJson() const;
  [[nodiscard]] QJsonArray roadsToJson() const;
  [[nodiscard]] QJsonArray bridgesToJson() const;
};

} // namespace MapEditor
