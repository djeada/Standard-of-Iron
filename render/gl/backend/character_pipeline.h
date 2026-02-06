#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <unordered_map>

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class CharacterPipeline final : public IPipeline {
public:
  explicit CharacterPipeline(GL::Backend *backend,
                             GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~CharacterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct BasicUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle material_id{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle instanced{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_basic_shader = nullptr;
  GL::Shader *m_archer_shader = nullptr;
  GL::Shader *m_swordsman_shader = nullptr;
  GL::Shader *m_spearman_shader = nullptr;

  BasicUniforms m_basic_uniforms;
  BasicUniforms m_archer_uniforms;
  BasicUniforms m_swordsman_uniforms;
  BasicUniforms m_spearman_uniforms;

  BasicUniforms *resolve_uniforms(GL::Shader *shader);

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;
  std::unordered_map<GL::Shader *, BasicUniforms> m_uniform_cache;
  GL::Shader *m_last_resolved_shader = nullptr;
  BasicUniforms *m_last_resolved_uniforms = nullptr;

  void cache_basic_uniforms();
  void cache_archer_uniforms();
  void cache_knight_uniforms();
  void cache_spearman_uniforms();
  BasicUniforms build_uniform_set(GL::Shader *shader) const;
  void cache_nation_variants(const QString &base_key);
};

} // namespace BackendPipelines
} // namespace Render::GL
