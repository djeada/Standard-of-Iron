#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Game {
namespace Map {
class TerrainHeightMap;
struct BiomeSettings;
} // namespace Map
} // namespace Game

namespace Engine::Core {

class Serialization {
public:
  static QJsonObject serializeEntity(const class Entity *entity);
  static void deserializeEntity(class Entity *entity, const QJsonObject &json);

  static QJsonDocument serializeWorld(const class World *world);
  static void deserializeWorld(class World *world, const QJsonDocument &doc);

  static QJsonObject
  serializeTerrain(const Game::Map::TerrainHeightMap *heightMap,
                   const Game::Map::BiomeSettings &biome);
  static void deserializeTerrain(Game::Map::TerrainHeightMap *heightMap,
                                 Game::Map::BiomeSettings &biome,
                                 const QJsonObject &json);

  static bool saveToFile(const QString &filename, const QJsonDocument &doc);
  static QJsonDocument loadFromFile(const QString &filename);
};

} // namespace Engine::Core
