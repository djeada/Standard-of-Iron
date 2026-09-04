#pragma once

#include <unordered_map>

#include "render/gl/shader.h"

namespace Render::GL {

class ShaderCache;

class ShaderUniformCache {
public:
  explicit ShaderUniformCache(ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}

  struct BasicUniforms {
    Shader::UniformHandle mvp{Shader::InvalidUniform};
    Shader::UniformHandle model{Shader::InvalidUniform};
    Shader::UniformHandle texture{Shader::InvalidUniform};
    Shader::UniformHandle use_texture{Shader::InvalidUniform};
    Shader::UniformHandle color{Shader::InvalidUniform};
    Shader::UniformHandle alpha{Shader::InvalidUniform};
    Shader::UniformHandle material_id{Shader::InvalidUniform};
    Shader::UniformHandle material_detail{Shader::InvalidUniform};
    Shader::UniformHandle has_material_detail{Shader::InvalidUniform};
    Shader::UniformHandle instanced{Shader::InvalidUniform};
    Shader::UniformHandle view_proj{Shader::InvalidUniform};
    Shader::UniformHandle light_dir{Shader::InvalidUniform};
    Shader::UniformHandle ambient_strength{Shader::InvalidUniform};
    Shader::UniformHandle camera_pos{Shader::InvalidUniform};
    Shader* instanced_variant{nullptr};
  };

  auto initialize() -> bool;

  auto resolve_uniforms(Shader* shader) -> BasicUniforms*;

private:
  [[nodiscard]] auto build_uniform_set(Shader* shader) const -> BasicUniforms;

  ShaderCache* m_shader_cache = nullptr;
  Shader* m_basic_shader = nullptr;
  std::unordered_map<Shader*, BasicUniforms> m_uniform_cache;
  Shader* m_last_resolved_shader = nullptr;
  BasicUniforms* m_last_resolved_uniforms = nullptr;
};

} // namespace Render::GL
