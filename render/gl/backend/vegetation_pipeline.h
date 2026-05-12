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

  [[nodiscard]] auto tent_shader() const -> GL::Shader * {
    return m_tent_shader;
  }
  [[nodiscard]] auto supply_cart_shader() const -> GL::Shader * {
    return m_supply_cart_shader;
  }
  [[nodiscard]] auto weapon_rack_shader() const -> GL::Shader * {
    return m_weapon_rack_shader;
  }
  [[nodiscard]] auto ruins_shader() const -> GL::Shader * {
    return m_ruins_shader;
  }
  [[nodiscard]] auto dead_tree_shader() const -> GL::Shader * {
    return m_dead_tree_shader;
  }

  struct PropUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  };

  PropUniforms m_tent_uniforms;
  PropUniforms m_supply_cart_uniforms;
  PropUniforms m_weapon_rack_uniforms;
  PropUniforms m_ruins_uniforms;
  PropUniforms m_dead_tree_uniforms;

  GLuint m_tent_vao{0};
  GLuint m_tent_vertex_buffer{0};
  GLuint m_tent_index_buffer{0};
  GLsizei m_tent_index_count{0};
  GLsizei m_tent_vertex_count{0};

  GLuint m_supply_cart_vao{0};
  GLuint m_supply_cart_vertex_buffer{0};
  GLuint m_supply_cart_index_buffer{0};
  GLsizei m_supply_cart_index_count{0};
  GLsizei m_supply_cart_vertex_count{0};

  GLuint m_weapon_rack_vao{0};
  GLuint m_weapon_rack_vertex_buffer{0};
  GLuint m_weapon_rack_index_buffer{0};
  GLsizei m_weapon_rack_index_count{0};
  GLsizei m_weapon_rack_vertex_count{0};

  GLuint m_ruins_vao{0};
  GLuint m_ruins_vertex_buffer{0};
  GLuint m_ruins_index_buffer{0};
  GLsizei m_ruins_index_count{0};
  GLsizei m_ruins_vertex_count{0};

  GLuint m_dead_tree_vao{0};
  GLuint m_dead_tree_vertex_buffer{0};
  GLuint m_dead_tree_index_buffer{0};
  GLsizei m_dead_tree_index_count{0};
  GLsizei m_dead_tree_vertex_count{0};

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
  void initialize_tent_pipeline();
  void shutdown_tent_pipeline();
  void initialize_supply_cart_pipeline();
  void shutdown_supply_cart_pipeline();
  void initialize_weapon_rack_pipeline();
  void shutdown_weapon_rack_pipeline();
  void initialize_ruins_pipeline();
  void shutdown_ruins_pipeline();
  void initialize_dead_tree_pipeline();
  void shutdown_dead_tree_pipeline();
  void upload_prop_mesh_impl(
      const std::vector<std::pair<QVector3D, QVector3D>> &verts,
      const std::vector<uint16_t> &idx, GLuint &vao, GLuint &vbo, GLuint &ibo,
      GLsizei &vert_count, GLsizei &idx_count);
  void delete_prop_pipeline_impl(GLuint &vao, GLuint &vbo, GLuint &ibo,
                                 GLsizei &vc, GLsizei &ic);

  GL::ShaderCache *m_shader_cache;
  bool m_initialized{false};

  GL::Shader *m_stone_shader{nullptr};
  GL::Shader *m_plant_shader{nullptr};
  GL::Shader *m_pine_shader{nullptr};
  GL::Shader *m_olive_shader{nullptr};
  GL::Shader *m_firecamp_shader{nullptr};
  GL::Shader *m_tent_shader{nullptr};
  GL::Shader *m_supply_cart_shader{nullptr};
  GL::Shader *m_weapon_rack_shader{nullptr};
  GL::Shader *m_ruins_shader{nullptr};
  GL::Shader *m_dead_tree_shader{nullptr};
};

} // namespace Render::GL::BackendPipelines
