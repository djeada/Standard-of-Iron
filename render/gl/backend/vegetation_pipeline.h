#pragma once

#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QVector2D>
#include <QVector3D>
#include <vector>

namespace Render::GL::BackendPipelines {

class VegetationPipeline : public IPipeline {
public:
  explicit VegetationPipeline(GL::ShaderCache *shaderCache);
  ~VegetationPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto isInitialized() const -> bool override {
    return m_initialized;
  }

  [[nodiscard]] auto stoneShader() const -> GL::Shader * {
    return m_stoneShader;
  }
  [[nodiscard]] auto plantShader() const -> GL::Shader * {
    return m_plantShader;
  }
  [[nodiscard]] auto pineShader() const -> GL::Shader * { return m_pineShader; }
  [[nodiscard]] auto firecampShader() const -> GL::Shader * {
    return m_firecampShader;
  }

  struct StoneUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_stoneUniforms;

  struct PlantUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windSpeed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_plantUniforms;

  struct PineUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle windSpeed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_pineUniforms;

  struct FireCampUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flickerSpeed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flickerAmount{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle glowStrength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle fireTexture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_right{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_forward{GL::Shader::InvalidUniform};
  } m_firecampUniforms;

  GLuint m_stoneVao{0};
  GLuint m_stoneVertexBuffer{0};
  GLuint m_stoneIndexBuffer{0};
  GLsizei m_stoneIndexCount{0};
  GLsizei m_stoneVertexCount{0};

  GLuint m_plantVao{0};
  GLuint m_plantVertexBuffer{0};
  GLuint m_plantIndexBuffer{0};
  GLsizei m_plantIndexCount{0};
  GLsizei m_plantVertexCount{0};

  GLuint m_pineVao{0};
  GLuint m_pineVertexBuffer{0};
  GLuint m_pineIndexBuffer{0};
  GLsizei m_pineIndexCount{0};
  GLsizei m_pineVertexCount{0};

  GLuint m_firecampVao{0};
  GLuint m_firecampVertexBuffer{0};
  GLuint m_firecampIndexBuffer{0};
  GLsizei m_firecampIndexCount{0};
  GLsizei m_firecampVertexCount{0};

private:
  void initializeStonePipeline();
  void shutdownStonePipeline();
  void initializePlantPipeline();
  void shutdownPlantPipeline();
  void initializePinePipeline();
  void shutdownPinePipeline();
  void initializeFireCampPipeline();
  void shutdownFireCampPipeline();

  GL::ShaderCache *m_shaderCache;
  bool m_initialized{false};

  GL::Shader *m_stoneShader{nullptr};
  GL::Shader *m_plantShader{nullptr};
  GL::Shader *m_pineShader{nullptr};
  GL::Shader *m_firecampShader{nullptr};
};

} // namespace Render::GL::BackendPipelines
