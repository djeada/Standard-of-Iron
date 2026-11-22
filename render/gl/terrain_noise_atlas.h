#pragma once

#include <QVector2D>

#include <memory>

#include "render/gl/gl_handle.h"

namespace Render::GL {

class Shader;

class TerrainNoiseAtlas {
public:
  struct AtlasBakeParams {
    int width = 0;
    int height = 0;
    float tile_size = 1.0F;
    QVector2D noise_offset{0.0F, 0.0F};
    float macro_noise_scale = 1.0F;
    bool low_detail = false;
  };

  TerrainNoiseAtlas();
  ~TerrainNoiseAtlas();

  TerrainNoiseAtlas(const TerrainNoiseAtlas&) = delete;
  auto operator=(const TerrainNoiseAtlas&) -> TerrainNoiseAtlas& = delete;

  void bake_microdetail(bool low_detail);

  // Returns true once the attempt is settled (baked, or failed for good) and
  // false when it could not run at all (no context, empty terrain).
  [[nodiscard]] auto bake_atlas(const AtlasBakeParams& params) -> bool;

  [[nodiscard]] auto microdetail_texture() const noexcept -> unsigned int {
    return m_microdetail_texture.id();
  }
  [[nodiscard]] auto atlas_texture() const noexcept -> unsigned int {
    return m_atlas_texture.id();
  }
  [[nodiscard]] auto atlas_detail_texture() const noexcept -> unsigned int {
    return m_atlas_detail_texture.id();
  }

private:
  std::unique_ptr<Shader> m_atlas_bake_shader;
  std::unique_ptr<Shader> m_microdetail_bake_shader;
  TextureHandle m_microdetail_texture;
  TextureHandle m_atlas_texture;
  TextureHandle m_atlas_detail_texture;
  FramebufferHandle m_atlas_fbo;
  VertexArrayHandle m_fullscreen_vao;
  int m_atlas_size = 0;
};

} // namespace Render::GL
