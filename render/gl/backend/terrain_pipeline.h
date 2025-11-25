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
  [[nodiscard]] auto isInitialized() const -> bool override;

  struct GrassUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windSpeed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
  };

  struct GroundUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassPrimary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassSecondary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassDry{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tint{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle noiseOffset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle macroNoiseScale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle detail_noiseScale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilBlendHeight{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilBlendSharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle heightNoiseStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle heightNoiseFrequency{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambientBoost{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
    // Ground-type-specific uniforms
    GL::Shader::UniformHandle snowCoverage{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle moistureLevel{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle crackIntensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rockExposure{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassSaturation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilRoughness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle snowColor{GL::Shader::InvalidUniform};
  };

  struct TerrainUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassPrimary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassSecondary{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassDry{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rockLow{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rockHigh{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tint{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle noiseOffset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle macroNoiseScale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle detail_noiseScale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle slopeRockThreshold{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle slopeRockSharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilBlendHeight{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilBlendSharpness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle heightNoiseStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle heightNoiseFrequency{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambientBoost{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rockDetailStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
    // Ground-type-specific uniforms
    GL::Shader::UniformHandle snowCoverage{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle moistureLevel{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle crackIntensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rockExposure{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grassSaturation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle soilRoughness{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle snowColor{GL::Shader::InvalidUniform};
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
