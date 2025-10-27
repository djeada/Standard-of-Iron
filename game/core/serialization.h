#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Game::Map {
class TerrainHeightMap;
struct BiomeSettings;
} // namespace Game::Map

namespace Engine::Core {

class Serialization {
public:
  static auto serializeEntity(const class Entity *entity) -> QJsonObject;
  static void deserializeEntity(class Entity *entity, const QJsonObject &json);

  static auto serializeWorld(const class World *world) -> QJsonDocument;
  static void deserializeWorld(class World *world, const QJsonDocument &doc);

  static auto
  serializeTerrain(const Game::Map::TerrainHeightMap *height_map,
                   const Game::Map::BiomeSettings &biome) -> QJsonObject;
  static void deserializeTerrain(Game::Map::TerrainHeightMap *height_map,
                                 Game::Map::BiomeSettings &biome,
                                 const QJsonObject &json);

  static auto saveToFile(const QString &filename,
                         const QJsonDocument &doc) -> bool;
  static auto loadFromFile(const QString &filename) -> QJsonDocument;
};

} // namespace Engine::Core
