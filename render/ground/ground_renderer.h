#pragma once
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <cstdint>
#include <vector>

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
    recompute_model();
    update_noise_offset();

    invalidate_params_cache();
  }

  void configure_extent(float extent) {
    m_extent = extent;
    recompute_model();
    update_noise_offset();

    invalidate_params_cache();
  }

  void set_color(const QVector3D &c) { m_color = c; }

  void set_biome(const Game::Map::BiomeSettings &settings) {
    m_biome_settings = settings;
    m_has_biome = true;
    update_noise_offset();

    invalidate_params_cache();
  }

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void recompute_model();
  void update_noise_offset();
  auto build_params() const -> Render::GL::TerrainChunkParams;
  void sync_biome_from_service();
  static auto biome_equals(const Game::Map::BiomeSettings &a,
                           const Game::Map::BiomeSettings &b) -> bool;

  float m_tile_size = 1.0F;
  int m_width = 50;
  int m_height = 50;
  float m_extent = 50.0F;

  QVector3D m_color{0.15F, 0.18F, 0.15F};
  QMatrix4x4 m_model;
  Game::Map::BiomeSettings m_biome_settings;
  bool m_has_biome = false;
  QVector2D m_noise_offset{0.0F, 0.0F};
  float m_noise_angle = 0.0F;

  mutable Render::GL::TerrainChunkParams m_cached_params{};
  mutable bool m_cached_params_valid = false;

  QMatrix4x4 m_last_submitted_model;
  bool m_model_dirty = true;

  std::uint64_t m_state_version = 1;
  std::uint64_t m_last_submitted_state_version = 0;

  void invalidate_params_cache() {
    m_cached_params_valid = false;
    ++m_state_version;
  }
};

} // namespace Render::GL
