#pragma once
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/terrain_service.h"
#include "render/frame/i_draw_source.h"
#include "render/frame/world_chunk.h"

namespace Render::GL {
class FrameSubmitter;
class DrawResources;
class Mesh;
class Texture;

class GroundRenderer : public IDrawSource {
public:
  static constexpr float k_ground_plane_y = -0.08F;
  // Clearance kept between the plane and a lake bed it has to duck under.
  static constexpr float k_bed_clearance = 0.04F;

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

  void set_color(const QVector3D& c) { m_color = c; }

  void set_light_direction(const QVector3D& dir) {
    m_light_direction =
        dir.isNull() ? QVector3D(0.35F, 0.85F, 0.42F) : dir.normalized();
    invalidate_params_cache();
  }

  void set_biome(const Game::Map::BiomeSettings& settings) {
    m_biome_settings = settings;
    m_has_biome = true;
    update_noise_offset();

    invalidate_params_cache();
  }

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

private:
  void recompute_model();
  void update_noise_offset();
  auto build_params() const -> Render::GL::TerrainChunkParams;
  void sync_biome_from_service();
  static auto biome_equals(const Game::Map::BiomeSettings& a,
                           const Game::Map::BiomeSettings& b) -> bool;

  // Where the plane lies: just under the ground, or under the lowest lake bed
  // when a map sets its water lower than that.
  float m_plane_y = k_ground_plane_y;
  float m_tile_size = 1.0F;
  int m_width = 50;
  int m_height = 50;
  float m_extent = 50.0F;

  QVector3D m_color{0.15F, 0.18F, 0.15F};
  QMatrix4x4 m_model;
  QVector3D m_light_direction{0.35F, 0.85F, 0.42F};
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
