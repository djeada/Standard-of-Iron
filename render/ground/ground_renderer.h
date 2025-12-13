#pragma once
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <cstdint>

#include "../../game/map/terrain.h"
#include "../../game/map/terrain_service.h"
#include "../i_render_pass.h"
#include "terrain_gpu.h"

namespace Render::GL {
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class GroundRenderer : public IRenderPass {
public:
  void configure(float tile_size, int width, int height) {
    m_tile_size = tile_size;
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
    m_biome_settings = settings;
    m_hasBiome = true;
    updateNoiseOffset();

    invalidateParamsCache();
  }

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void recomputeModel();
  void updateNoiseOffset();
  auto buildParams() const -> Render::GL::TerrainChunkParams;
  void syncBiomeFromService();
  static auto biomeEquals(const Game::Map::BiomeSettings &a,
                          const Game::Map::BiomeSettings &b) -> bool;

  float m_tile_size = 1.0F;
  int m_width = 50;
  int m_height = 50;
  float m_extent = 50.0F;

  QVector3D m_color{0.15F, 0.18F, 0.15F};
  QMatrix4x4 m_model;
  Game::Map::BiomeSettings m_biome_settings;
  bool m_hasBiome = false;
  QVector2D m_noiseOffset{0.0F, 0.0F};
  float m_noiseAngle = 0.0F;

  mutable Render::GL::TerrainChunkParams m_cachedParams{};
  mutable bool m_cachedParamsValid = false;

  QMatrix4x4 m_lastSubmittedModel;
  bool m_modelDirty = true;

  std::uint64_t m_stateVersion = 1;
  std::uint64_t m_lastSubmittedStateVersion = 0;

  void invalidateParamsCache() {
    m_cachedParamsValid = false;
    ++m_stateVersion;
  }
};

} // namespace Render::GL
