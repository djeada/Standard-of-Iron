#pragma once
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <cstdint>

#include "../../game/map/terrain.h"
#include "terrain_gpu.h"

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class GroundRenderer {
public:
  void configure(float tileSize, int width, int height) {
    m_tileSize = tileSize;
    m_width = width;
    m_height = height;
    recomputeModel();
    updateNoiseOffset();

    invalidateParamsCache();
  }

  void configureExtent(float extent) {
    m_extent = extent;
    recomputeModel();
    updateNoiseOffset();

    invalidateParamsCache();
  }

  void setColor(const QVector3D &c) { m_color = c; }

  void setBiome(const Game::Map::BiomeSettings &settings) {
    m_biomeSettings = settings;
    m_hasBiome = true;
    updateNoiseOffset();

    invalidateParamsCache();
  }

  void submit(Renderer &renderer, ResourceManager &resources);

private:
  void recomputeModel();
  void updateNoiseOffset();
  Render::GL::TerrainChunkParams buildParams() const;

  float m_tileSize = 1.0f;
  int m_width = 50;
  int m_height = 50;
  float m_extent = 50.0f;

  QVector3D m_color{0.15f, 0.18f, 0.15f};
  QMatrix4x4 m_model;
  Game::Map::BiomeSettings m_biomeSettings;
  bool m_hasBiome = false;
  QVector2D m_noiseOffset{0.0f, 0.0f};

  mutable Render::GL::TerrainChunkParams m_cachedParams{};
  mutable bool m_cachedParamsValid = false;

  QMatrix4x4 m_lastSubmittedModel;
  bool m_modelDirty = true;

  std::uint64_t m_stateVersion = 1;
  std::uint64_t m_lastSubmittedStateVersion = 0;

  inline void invalidateParamsCache() {
    m_cachedParamsValid = false;
    ++m_stateVersion;
  }
};

} // namespace GL
} // namespace Render
