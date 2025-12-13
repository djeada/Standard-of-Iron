#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <QVector3D>
#include <QtOpenGL>
#include <cstddef>

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class TerrainPipeline final : public IPipeline {
public:
  explicit TerrainPipeline(GL::Backend *backend, GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~TerrainPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct GrassUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
  };

  struct GroundUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_primary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_secondary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_dry{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tint{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle noise_offset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle macro_noise_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle detail_noise_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_blend_height{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_blend_sharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_noise_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_noise_frequency{
        GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_boost{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};

    GL::Shader::UniformHandle snow_coverage{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle moisture_level{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle crack_intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_saturation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_roughness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle snow_color{GL::Shader::InvalidUniform};
  };

  struct TerrainUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_primary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_secondary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_dry{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rock_low{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rock_high{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tint{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle noise_offset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle macro_noise_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle detail_noise_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle slope_rock_threshold{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle slope_rock_sharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_blend_height{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_blend_sharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_noise_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_noise_frequency{
        GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_boost{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rock_detail_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};

    GL::Shader::UniformHandle snow_coverage{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle moisture_level{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle crack_intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rock_exposure{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grass_saturation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soil_roughness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle snow_color{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_grassShader = nullptr;
  GL::Shader *m_groundShader = nullptr;
  GL::Shader *m_terrainShader = nullptr;

  GrassUniforms m_grassUniforms;
  GroundUniforms m_groundUniforms;
  TerrainUniforms m_terrainUniforms;

  GLuint m_grassVao = 0;
  GLuint m_grassVertexBuffer = 0;
  GLsizei m_grassVertexCount = 0;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;

  void cacheGrassUniforms();
  void cacheGroundUniforms();
  void cacheTerrainUniforms();

  void initializeGrassGeometry();
  void shutdownGrassGeometry();
};

} // namespace BackendPipelines
} // namespace Render::GL
