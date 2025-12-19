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
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override {
    return m_initialized;
  }

  [[nodiscard]] auto stone_shader() const -> GL::Shader * {
    return m_stoneShader;
  }
  [[nodiscard]] auto plant_shader() const -> GL::Shader * {
    return m_plantShader;
  }
  [[nodiscard]] auto pine_shader() const -> GL::Shader * {
    return m_pineShader;
  }
  [[nodiscard]] auto olive_shader() const -> GL::Shader * {
    return m_oliveShader;
  }
  [[nodiscard]] auto firecamp_shader() const -> GL::Shader * {
    return m_firecampShader;
  }

  struct StoneUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_stoneUniforms;

  struct PlantUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_plantUniforms;

  struct PineUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_pineUniforms;

  struct OliveUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_oliveUniforms;

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

  GLuint m_oliveVao{0};
  GLuint m_oliveVertexBuffer{0};
  GLuint m_oliveIndexBuffer{0};
  GLsizei m_oliveIndexCount{0};
  GLsizei m_oliveVertexCount{0};

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
  void initialize_pine_pipeline();
  void shutdownPinePipeline();
  void initialize_olive_pipeline();
  void shutdown_olive_pipeline();
  void initialize_fire_camp_pipeline();
  void shutdownFireCampPipeline();

  GL::ShaderCache *m_shaderCache;
  bool m_initialized{false};

  GL::Shader *m_stoneShader{nullptr};
  GL::Shader *m_plantShader{nullptr};
  GL::Shader *m_pineShader{nullptr};
  GL::Shader *m_oliveShader{nullptr};
  GL::Shader *m_firecampShader{nullptr};
};

} // namespace Render::GL::BackendPipelines
