#pragma once

#include "map_definition.h"
#include <QString>
#include <QVariantList>
#include <QVector3D>
#include <functional>
#include <memory>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace Render {
namespace GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class FogRenderer;
class StoneRenderer;
} // namespace GL
} // namespace Render

namespace Game {
namespace Map {

struct SkirmishLoadResult {
  bool ok = false;
  QString mapName;
  QString errorMessage;
  Engine::Core::EntityID playerUnitId = 0;
  float camFov = 45.0f;
  float camNear = 0.1f;
  float camFar = 1000.0f;
  int gridWidth = 50;
  int gridHeight = 50;
  float tileSize = 1.0f;
  int maxTroopsPerPlayer = 50;
  VictoryConfig victoryConfig;
  QVector3D focusPosition;
  bool hasFocusPosition = false;
};

class SkirmishLoader {
public:
  using OwnersUpdatedCallback = std::function<void()>;
  using VisibilityMaskReadyCallback = std::function<void()>;

  SkirmishLoader(Engine::Core::World &world, Render::GL::Renderer &renderer,
                 Render::GL::Camera &camera);

  void setGroundRenderer(Render::GL::GroundRenderer *ground) {
    m_ground = ground;
  }
  void setTerrainRenderer(Render::GL::TerrainRenderer *terrain) {
    m_terrain = terrain;
  }
  void setBiomeRenderer(Render::GL::BiomeRenderer *biome) { m_biome = biome; }
  void setFogRenderer(Render::GL::FogRenderer *fog) { m_fog = fog; }
  void setStoneRenderer(Render::GL::StoneRenderer *stone) { m_stone = stone; }

  void setOnOwnersUpdated(OwnersUpdatedCallback callback) {
    m_onOwnersUpdated = callback;
  }

  void setOnVisibilityMaskReady(VisibilityMaskReadyCallback callback) {
    m_onVisibilityMaskReady = callback;
  }

  SkirmishLoadResult start(const QString &mapPath,
                           const QVariantList &playerConfigs,
                           int selectedPlayerId, int &outSelectedPlayerId);

private:
  Engine::Core::World &m_world;
  Render::GL::Renderer &m_renderer;
  Render::GL::Camera &m_camera;
  Render::GL::GroundRenderer *m_ground = nullptr;
  Render::GL::TerrainRenderer *m_terrain = nullptr;
  Render::GL::BiomeRenderer *m_biome = nullptr;
  Render::GL::FogRenderer *m_fog = nullptr;
  Render::GL::StoneRenderer *m_stone = nullptr;
  OwnersUpdatedCallback m_onOwnersUpdated;
  VisibilityMaskReadyCallback m_onVisibilityMaskReady;
};

} // namespace Map
} // namespace Game
