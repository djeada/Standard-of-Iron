#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <vector>

namespace Game::Map {
class TerrainHeightMap;
struct BiomeSettings;
struct RoadSegment;
} // namespace Game::Map

namespace Engine::Core {

class Serialization {
public:
  static auto serializeEntity(const class Entity *entity) -> QJsonObject;
  static void deserializeEntity(class Entity *entity, const QJsonObject &json);

  static auto serializeWorld(const class World *world) -> QJsonDocument;
  static void deserializeWorld(class World *world, const QJsonDocument &doc);

  static auto serializeTerrain(const Game::Map::TerrainHeightMap *height_map,
                               const Game::Map::BiomeSettings &biome,
                               const std::vector<Game::Map::RoadSegment> &roads)
      -> QJsonObject;
  static void deserializeTerrain(Game::Map::TerrainHeightMap *height_map,
                                 Game::Map::BiomeSettings &biome,
                                 std::vector<Game::Map::RoadSegment> &roads,
                                 const QJsonObject &json);

  static auto saveToFile(const QString &filename,
                         const QJsonDocument &doc) -> bool;
  static auto loadFromFile(const QString &filename) -> QJsonDocument;
};

} // namespace Engine::Core
