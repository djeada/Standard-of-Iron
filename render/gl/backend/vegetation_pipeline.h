#pragma once

#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QVector2D>
#include <QVector3D>
#include <vector>

namespace Render::GL::BackendPipelines {

class VegetationPipeline : public IPipeline {
public:
  explicit VegetationPipeline(GL::ShaderCache *shader_cache);
  ~VegetationPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override {
    return m_initialized;
  }

  [[nodiscard]] auto stone_shader() const -> GL::Shader * {
    return m_stone_shader;
  }
  [[nodiscard]] auto plant_shader() const -> GL::Shader * {
    return m_plant_shader;
  }
  [[nodiscard]] auto pine_shader() const -> GL::Shader * {
    return m_pine_shader;
  }
  [[nodiscard]] auto olive_shader() const -> GL::Shader * {
    return m_olive_shader;
  }
  [[nodiscard]] auto firecamp_shader() const -> GL::Shader * {
    return m_firecamp_shader;
  }

  struct StoneUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_stone_uniforms;

  struct PlantUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_plant_uniforms;

  struct PineUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_pine_uniforms;

  struct OliveUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  } m_olive_uniforms;

  struct FireCampUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flicker_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flicker_amount{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle glow_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle fire_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_right{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_forward{GL::Shader::InvalidUniform};
  } m_firecamp_uniforms;

  GLuint m_stone_vao{0};
  GLuint m_stone_vertex_buffer{0};
  GLuint m_stone_index_buffer{0};
  GLsizei m_stone_index_count{0};
  GLsizei m_stone_vertex_count{0};

  GLuint m_plant_vao{0};
  GLuint m_plant_vertex_buffer{0};
  GLuint m_plant_index_buffer{0};
  GLsizei m_plant_index_count{0};
  GLsizei m_plant_vertex_count{0};

  GLuint m_pine_vao{0};
  GLuint m_pine_vertex_buffer{0};
  GLuint m_pine_index_buffer{0};
  GLsizei m_pine_index_count{0};
  GLsizei m_pine_vertex_count{0};

  GLuint m_olive_vao{0};
  GLuint m_olive_vertex_buffer{0};
  GLuint m_olive_index_buffer{0};
  GLsizei m_olive_index_count{0};
  GLsizei m_olive_vertex_count{0};

  GLuint m_firecamp_vao{0};
  GLuint m_firecamp_vertex_buffer{0};
  GLuint m_firecamp_index_buffer{0};
  GLsizei m_firecamp_index_count{0};
  GLsizei m_firecamp_vertex_count{0};

private:
  void initialize_stone_pipeline();
  void shutdown_stone_pipeline();
  void initialize_plant_pipeline();
  void shutdown_plant_pipeline();
  void initialize_pine_pipeline();
  void shutdown_pine_pipeline();
  void initialize_olive_pipeline();
  void shutdown_olive_pipeline();
  void initialize_fire_camp_pipeline();
  void shutdown_fire_camp_pipeline();

  GL::ShaderCache *m_shader_cache;
  bool m_initialized{false};

  GL::Shader *m_stone_shader{nullptr};
  GL::Shader *m_plant_shader{nullptr};
  GL::Shader *m_pine_shader{nullptr};
  GL::Shader *m_olive_shader{nullptr};
  GL::Shader *m_firecamp_shader{nullptr};
};

} // namespace Render::GL::BackendPipelines
